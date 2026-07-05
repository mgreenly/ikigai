package telemetry

import (
	"sync"
	"time"
)

const MaxSamples = 1440

// Sample is one telemetry reading. Value is bytes; 0 means unavailable.
type Sample struct {
	At    time.Time
	Value int64
}

// Store holds the bounded in-memory telemetry history and chart capacities.
type Store struct {
	mu        sync.Mutex
	series    map[string]*ring
	totalMem  int64
	totalDisk int64
}

// Snapshot is an independent ordered copy of the store state.
type Snapshot struct {
	Series    map[string][]Sample
	TotalMem  int64
	TotalDisk int64
}

type ring struct {
	buf  []Sample
	head int
	len  int
}

// NewStore returns an empty telemetry store.
func NewStore() *Store {
	return &Store{series: make(map[string]*ring)}
}

// Append records one sample on a series, evicting the oldest sample when full.
func (s *Store) Append(key string, sample Sample) {
	s.mu.Lock()
	defer s.mu.Unlock()

	r := s.series[key]
	if r == nil {
		r = &ring{buf: make([]Sample, MaxSamples)}
		s.series[key] = r
	}
	r.append(sample)
}

// SetCapacities records the current total memory and disk capacities.
func (s *Store) SetCapacities(totalMem, totalDisk int64) {
	s.mu.Lock()
	defer s.mu.Unlock()

	s.totalMem = totalMem
	s.totalDisk = totalDisk
}

// Snapshot returns an independent ordered copy of every series and capacity.
func (s *Store) Snapshot() Snapshot {
	s.mu.Lock()
	defer s.mu.Unlock()

	out := Snapshot{
		Series:    make(map[string][]Sample, len(s.series)),
		TotalMem:  s.totalMem,
		TotalDisk: s.totalDisk,
	}
	for key, r := range s.series {
		out.Series[key] = r.samples()
	}
	return out
}

func (r *ring) append(sample Sample) {
	if r.len < MaxSamples {
		r.buf[(r.head+r.len)%MaxSamples] = sample
		r.len++
		return
	}
	r.buf[r.head] = sample
	r.head = (r.head + 1) % MaxSamples
}

func (r *ring) samples() []Sample {
	out := make([]Sample, r.len)
	for i := range out {
		out[i] = r.buf[(r.head+i)%MaxSamples]
	}
	return out
}
