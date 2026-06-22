// Package worker will run asynchronous wiki jobs.
package worker

import "context"

// Task is a unit of background work.
type Task func(context.Context) error

// Service is the ingest queue surface consumed by the single worker.
type Service interface {
	ProcessNext(ctx context.Context) (bool, error)
	Wait(ctx context.Context) error
}

type bootSweeper interface {
	RequeueWorking(ctx context.Context) (int, error)
}

// Run processes pending ingest jobs with one worker loop.
func Run(ctx context.Context, services ...Service) error {
	if len(services) == 0 || services[0] == nil {
		<-ctx.Done()
		return nil
	}
	svc := services[0]
	if sweeper, ok := svc.(bootSweeper); ok {
		if _, err := sweeper.RequeueWorking(ctx); err != nil {
			if ctx.Err() != nil {
				return nil
			}
			return err
		}
	}
	for {
		processed, err := svc.ProcessNext(ctx)
		if err != nil {
			return err
		}
		if processed {
			continue
		}
		if err := svc.Wait(ctx); err != nil {
			if ctx.Err() != nil {
				return nil
			}
			return err
		}
	}
}
