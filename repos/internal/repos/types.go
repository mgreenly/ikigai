package repos

import "time"

const (
	StatusQueued    = "queued"
	StatusRunning   = "running"
	StatusSucceeded = "succeeded"
	StatusFailed    = "failed"
	StatusCancelled = "cancelled"
)

// Repo is a provisioned GitHub repository.
type Repo struct {
	Name          string
	OwnerEmail    string
	CloneURL      string
	DefaultBranch string
	CreatedAt     time.Time
}

// Session is one durable agent run against a repository.
type Session struct {
	ID           string
	RepoName     string
	OwnerEmail   string
	IssueNumber  *int
	Attempt      int
	Branch       string
	Instructions string
	Status       string
	Error        *string
	PRURL        *string
	CreatedAt    time.Time
	StartedAt    *time.Time
	EndedAt      *time.Time
	LogPath      string
}
