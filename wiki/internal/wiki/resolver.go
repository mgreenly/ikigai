package wiki

import (
	"context"
	"database/sql"
	"errors"
)

// Resolver resolves a raw subject name through canonical subjects and aliases.
type Resolver struct {
	subjects *SubjectStore
	aliases  *AliasStore
}

func NewResolver(db sqlStore) *Resolver {
	return &Resolver{
		subjects: NewSubjectStore(db),
		aliases:  NewAliasStore(db),
	}
}

func NewResolverForSubjects(subjects *SubjectStore) *Resolver {
	if subjects == nil {
		return nil
	}
	return NewResolver(subjects.db)
}

func (r *Resolver) ResolveByName(ctx context.Context, name string) (Subject, error) {
	if r == nil {
		return Subject{}, ErrSubjectNotFound
	}
	subject, err := r.subjects.GetByNormName(ctx, name)
	if err == nil {
		return subject, nil
	}
	if !errors.Is(err, sql.ErrNoRows) {
		return Subject{}, err
	}

	alias, err := r.aliases.GetByNormName(ctx, name)
	if errors.Is(err, sql.ErrNoRows) {
		return Subject{}, ErrSubjectNotFound
	}
	if err != nil {
		return Subject{}, err
	}
	subject, err = r.subjects.Get(ctx, alias.SubjectID)
	if errors.Is(err, sql.ErrNoRows) {
		return Subject{}, ErrSubjectNotFound
	}
	return subject, err
}
