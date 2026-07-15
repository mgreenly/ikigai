package repos

import (
	"context"
	"fmt"
	"strings"
)

// IssueContent is the untrusted issue material pinned into a session.
type IssueContent struct {
	Title    string
	Body     string
	Comments []string
}

// Protocol implements the deterministic issue label and pull-request lifecycle.
type Protocol struct{ peer *GitHubPeer }

func NewProtocol(peer *GitHubPeer) *Protocol { return &Protocol{peer: peer} }

func identity(session Session) Identity {
	return Identity{OwnerEmail: session.OwnerEmail, SessionID: session.ID}
}

func (p *Protocol) FetchIssue(ctx context.Context, session Session) (IssueContent, error) {
	if session.IssueNumber == nil {
		return IssueContent{}, fmt.Errorf("fetch issue: manual session")
	}
	issue, err := p.peer.IssueGet(ctx, identity(session), session.RepoName, *session.IssueNumber)
	if err != nil {
		return IssueContent{}, err
	}
	comments, err := p.peer.IssueComments(ctx, identity(session), session.RepoName, *session.IssueNumber)
	if err != nil {
		return IssueContent{}, err
	}
	content := IssueContent{Title: issue.Title, Body: issue.Body}
	for _, comment := range comments {
		content.Comments = append(content.Comments, comment.Body)
	}
	return content, nil
}

func (p *Protocol) PostQueued(ctx context.Context, session Session) error {
	return p.peer.IssueComment(ctx, identity(session), session.RepoName, *session.IssueNumber, "queued")
}

func (p *Protocol) Admit(ctx context.Context, session Session) error {
	if session.IssueNumber == nil {
		return nil
	}
	id, issue := identity(session), *session.IssueNumber
	if err := p.peer.LabelRemove(ctx, id, session.RepoName, issue, "execute"); err != nil {
		return err
	}
	if session.Attempt > 1 {
		if err := p.peer.LabelRemove(ctx, id, session.RepoName, issue, "failed"); err != nil {
			return err
		}
	}
	if err := p.peer.LabelAdd(ctx, id, session.RepoName, issue, []string{"executing"}); err != nil {
		return err
	}
	return p.peer.IssueComment(ctx, id, session.RepoName, issue, "session "+session.ID)
}

func (p *Protocol) Success(ctx context.Context, session Session, repo Repo, title, summary, checkSummary string) (string, error) {
	body := strings.TrimSpace(summary) + "\n\nCheck: " + checkSummary + "\n\nSession: " + session.ID
	if session.IssueNumber != nil {
		body += fmt.Sprintf("\n\nFixes #%d", *session.IssueNumber)
	}
	if strings.TrimSpace(title) == "" {
		title = "Automated changes for " + session.Branch
	}
	pr, err := p.peer.PRCreate(ctx, identity(session), repo.Name, title, session.Branch, repo.DefaultBranch, body)
	if err != nil {
		return "", err
	}
	if session.IssueNumber == nil {
		return pr.URL, nil
	}
	id, issue := identity(session), *session.IssueNumber
	if err := p.peer.IssueComment(ctx, id, repo.Name, issue, pr.URL); err != nil {
		return "", err
	}
	if err := p.peer.LabelRemove(ctx, id, repo.Name, issue, "executing"); err != nil {
		return "", err
	}
	if err := p.peer.LabelRemove(ctx, id, repo.Name, issue, "failed"); err != nil {
		return "", err
	}
	return pr.URL, nil
}

func (p *Protocol) Failure(ctx context.Context, session Session, reason string) error {
	if session.IssueNumber == nil {
		return nil
	}
	id, issue := identity(session), *session.IssueNumber
	if err := p.peer.LabelRemove(ctx, id, session.RepoName, issue, "executing"); err != nil {
		return err
	}
	if err := p.peer.LabelAdd(ctx, id, session.RepoName, issue, []string{"failed"}); err != nil {
		return err
	}
	return p.peer.IssueComment(ctx, id, session.RepoName, issue, reason)
}
