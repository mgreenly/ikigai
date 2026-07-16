package repos

import (
	"fmt"
	"path/filepath"
)

// ResolveStateRoot returns the configured state directory as an absolute path.
func ResolveStateRoot(getenv func(string) string) (string, error) {
	stateRoot := getenv("REPOS_STATE_DIR")
	if stateRoot == "" {
		stateRoot = "state"
	}
	absolute, err := filepath.Abs(stateRoot)
	if err != nil {
		return "", fmt.Errorf("resolve state root: %w", err)
	}
	return absolute, nil
}
