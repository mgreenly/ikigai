package tools

import (
	"bufio"
	"context"
	"crypto/sha256"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"io/fs"
	"net"
	"net/http"
	"net/url"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"sort"
	"strconv"
	"strings"
	"time"

	"github.com/ikigenba/agentkit"
)

const (
	nameBash  = "Bash"
	nameRead  = "Read"
	nameWrite = "Write"
	nameEdit  = "Edit"
	nameGlob  = "Glob"
	nameGrep  = "Grep"
	nameFetch = "Fetch"
)

// All returns the seven built-in tools confined to sandboxRoot.
func All(sandboxRoot string, sourcePortAllowed func(port int) bool) []agentkit.Tool {
	return []agentkit.Tool{
		agentkit.NewTool(nameRead, "Read a file inside the sandbox.", func(ctx context.Context, in readInput) (string, error) {
			return readFile(sandboxRoot, in)
		}),
		agentkit.NewTool(nameBash, "Run a foreground shell command from the sandbox root.", func(ctx context.Context, in bashInput) (string, error) {
			return runBash(ctx, sandboxRoot, in)
		}),
		agentkit.NewTool(nameWrite, "Write a file inside the sandbox.", func(ctx context.Context, in writeInput) (string, error) {
			return writeFile(sandboxRoot, in)
		}),
		agentkit.NewTool(nameEdit, "Edit a file inside the sandbox by replacing text.", func(ctx context.Context, in editInput) (string, error) {
			return editFile(sandboxRoot, in)
		}),
		agentkit.NewTool(nameGlob, "Find files inside the sandbox using a glob pattern.", func(ctx context.Context, in globInput) (string, error) {
			return globFiles(sandboxRoot, in)
		}),
		agentkit.NewTool(nameGrep, "Search files inside the sandbox for a regular expression.", func(ctx context.Context, in grepInput) (string, error) {
			return grepFiles(sandboxRoot, in)
		}),
		agentkit.NewTool(nameFetch, "Fetch a suite content URL into the sandbox.", func(ctx context.Context, in fetchInput) (string, error) {
			return fetchFile(ctx, sandboxRoot, sourcePortAllowed, in)
		}),
	}
}

type bashInput struct {
	Command string `json:"command" jsonschema:"required,description=The command to execute"`
}

type readInput struct {
	FilePath string `json:"file_path" jsonschema:"required,description=Path to read, relative to the sandbox root"`
	Offset   int    `json:"offset,omitempty" jsonschema:"description=1-based line offset; zero starts at the beginning"`
	Limit    int    `json:"limit,omitempty" jsonschema:"description=Maximum lines to return; zero returns all remaining lines"`
}

type writeInput struct {
	FilePath string `json:"file_path" jsonschema:"required,description=Path to write, relative to the sandbox root"`
	Content  string `json:"content" jsonschema:"required,description=File content"`
}

type editInput struct {
	FilePath   string `json:"file_path" jsonschema:"required,description=Path to edit, relative to the sandbox root"`
	OldString  string `json:"old_string" jsonschema:"required,description=Text to replace"`
	NewString  string `json:"new_string" jsonschema:"required,description=Replacement text"`
	ReplaceAll bool   `json:"replace_all,omitempty" jsonschema:"description=Replace all occurrences instead of only the first"`
}

type globInput struct {
	Pattern string `json:"pattern" jsonschema:"required,description=Glob pattern relative to path"`
	Path    string `json:"path,omitempty" jsonschema:"description=Directory to search, relative to the sandbox root"`
}

type grepInput struct {
	Pattern string `json:"pattern" jsonschema:"required,description=Regular expression to search for"`
	Path    string `json:"path,omitempty" jsonschema:"description=File or directory to search, relative to the sandbox root"`
	Glob    string `json:"glob,omitempty" jsonschema:"description=Optional file glob matched against base names"`
}

type fetchInput struct {
	ContentURL string `json:"content_url" jsonschema:"required,description=A suite loopback content URL (e.g. from an event payload or a tool result)"`
	DestPath   string `json:"dest_path" jsonschema:"required,description=Destination file path, relative to the sandbox root"`
}

func fetchFile(ctx context.Context, root string, sourcePortAllowed func(int) bool, in fetchInput) (string, error) {
	dest, err := confinePath(root, in.DestPath)
	if err != nil {
		return "", fmt.Errorf("validation: destination path must stay inside the sandbox: %w", err)
	}
	u, err := validateContentURL(in.ContentURL, sourcePortAllowed)
	if err != nil {
		return "", err
	}
	if err := os.MkdirAll(filepath.Dir(dest), 0o755); err != nil {
		return "", fmt.Errorf("source_unavailable: prepare destination: %w", err)
	}
	base, err := sandboxRoot(root)
	if err != nil {
		return "", fmt.Errorf("source_unavailable: resolve sandbox: %w", err)
	}
	tmp, err := os.CreateTemp(base, ".fetch-*")
	if err != nil {
		return "", fmt.Errorf("source_unavailable: create temporary file: %w", err)
	}
	tmpName := tmp.Name()
	defer os.Remove(tmpName)

	client := &http.Client{
		Timeout: 0,
		CheckRedirect: func(_ *http.Request, _ []*http.Request) error {
			return http.ErrUseLastResponse
		},
		Transport: &http.Transport{
			DialContext:           (&net.Dialer{Timeout: 10 * time.Second}).DialContext,
			ResponseHeaderTimeout: 10 * time.Second,
		},
	}
	req, err := http.NewRequestWithContext(ctx, http.MethodGet, u.String(), nil)
	if err != nil {
		return "", fmt.Errorf("validation: invalid content URL: %w", err)
	}
	resp, err := client.Do(req)
	if err != nil {
		return "", fmt.Errorf("source_unavailable: fetch content URL: %w", err)
	}
	defer resp.Body.Close()
	if resp.StatusCode == http.StatusNotFound {
		return "", fmt.Errorf("not_found: the reference is stale or absent — re-derive it from the holder")
	}
	if resp.StatusCode == http.StatusConflict {
		return "", fmt.Errorf("conflict: the reference revision moved — re-derive it from the holder")
	}
	if resp.StatusCode < http.StatusOK || resp.StatusCode >= http.StatusMultipleChoices {
		return "", fmt.Errorf("source_unavailable: source returned HTTP %d", resp.StatusCode)
	}

	hash := sha256.New()
	size, err := io.Copy(tmp, io.TeeReader(resp.Body, hash))
	if closeErr := tmp.Close(); err == nil {
		err = closeErr
	}
	if err != nil {
		return "", fmt.Errorf("source_unavailable: stream content: %w", err)
	}
	if err := os.Rename(tmpName, dest); err != nil {
		return "", fmt.Errorf("source_unavailable: finalize content: %w", err)
	}
	result, err := json.Marshal(struct {
		Path        string `json:"path"`
		Size        int64  `json:"size"`
		ContentHash string `json:"content_hash"`
	}{in.DestPath, size, fmt.Sprintf("%x", hash.Sum(nil))})
	if err != nil {
		return "", err
	}
	return string(result), nil
}

func validateContentURL(raw string, sourcePortAllowed func(int) bool) (*url.URL, error) {
	u, err := url.Parse(raw)
	if err != nil || u.Scheme != "http" {
		return nil, fmt.Errorf("validation: content URL must use the http scheme")
	}
	if u.Hostname() != "127.0.0.1" && u.Hostname() != "::1" {
		return nil, fmt.Errorf("validation: content URL host must be literal 127.0.0.1 or ::1")
	}
	portText := u.Port()
	port, err := strconv.Atoi(portText)
	if err != nil || portText == "" || port < 1 || port > 65535 {
		return nil, fmt.Errorf("validation: content URL must carry an explicit valid port")
	}
	if sourcePortAllowed == nil || !sourcePortAllowed(port) {
		return nil, fmt.Errorf("validation: content URL port %d is not registered", port)
	}
	return u, nil
}

func runBash(ctx context.Context, root string, in bashInput) (string, error) {
	if strings.TrimSpace(in.Command) == "" {
		return "", errors.New("command is required")
	}
	dir, err := sandboxRoot(root)
	if err != nil {
		return "", err
	}
	cmd := exec.CommandContext(ctx, "bash", "-c", in.Command)
	cmd.Dir = dir
	out, err := cmd.CombinedOutput()
	if err != nil {
		return string(out), err
	}
	return string(out), nil
}

func readFile(root string, in readInput) (string, error) {
	path, err := confinePath(root, in.FilePath)
	if err != nil {
		return "", err
	}
	content, err := os.ReadFile(path)
	if err != nil {
		return "", err
	}
	return sliceLines(string(content), in.Offset, in.Limit), nil
}

func writeFile(root string, in writeInput) (string, error) {
	path, err := confinePath(root, in.FilePath)
	if err != nil {
		return "", err
	}
	if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
		return "", err
	}
	if err := os.WriteFile(path, []byte(in.Content), 0o644); err != nil {
		return "", err
	}
	return fmt.Sprintf("wrote %s", in.FilePath), nil
}

func editFile(root string, in editInput) (string, error) {
	if in.OldString == "" {
		return "", errors.New("old_string is required")
	}
	path, err := confinePath(root, in.FilePath)
	if err != nil {
		return "", err
	}
	content, err := os.ReadFile(path)
	if err != nil {
		return "", err
	}
	count := 1
	if in.ReplaceAll {
		count = -1
	}
	updated := strings.Replace(string(content), in.OldString, in.NewString, count)
	if updated == string(content) {
		return "", errors.New("old_string not found")
	}
	if err := os.WriteFile(path, []byte(updated), 0o644); err != nil {
		return "", err
	}
	return fmt.Sprintf("edited %s", in.FilePath), nil
}

func globFiles(root string, in globInput) (string, error) {
	base, err := searchPath(root, in.Path)
	if err != nil {
		return "", err
	}
	if in.Pattern == "" {
		return "", errors.New("pattern is required")
	}
	pattern := in.Pattern
	if !filepath.IsAbs(pattern) {
		pattern = filepath.Join(base, pattern)
	}
	pattern, err = confinePath(root, pattern)
	if err != nil {
		return "", err
	}
	matches, err := filepath.Glob(pattern)
	if err != nil {
		return "", err
	}
	sort.Strings(matches)
	for i, match := range matches {
		if rel, err := filepath.Rel(base, match); err == nil {
			matches[i] = filepath.ToSlash(rel)
		}
	}
	out, err := json.Marshal(matches)
	if err != nil {
		return "", err
	}
	return string(out), nil
}

func grepFiles(root string, in grepInput) (string, error) {
	if in.Pattern == "" {
		return "", errors.New("pattern is required")
	}
	re, err := regexp.Compile(in.Pattern)
	if err != nil {
		return "", err
	}
	base, err := searchPath(root, in.Path)
	if err != nil {
		return "", err
	}
	var matches []string
	addMatches := func(path string) error {
		if in.Glob != "" {
			ok, err := filepath.Match(in.Glob, filepath.Base(path))
			if err != nil {
				return err
			}
			if !ok {
				return nil
			}
		}
		file, err := os.Open(path)
		if err != nil {
			return err
		}
		defer file.Close()
		scanner := bufio.NewScanner(file)
		for line := 1; scanner.Scan(); line++ {
			text := scanner.Text()
			if re.MatchString(text) {
				rel, err := filepath.Rel(base, path)
				if err != nil {
					rel = path
				}
				matches = append(matches, fmt.Sprintf("%s:%d:%s", filepath.ToSlash(rel), line, text))
			}
		}
		return scanner.Err()
	}

	info, err := os.Stat(base)
	if err != nil {
		return "", err
	}
	if !info.IsDir() {
		if err := addMatches(base); err != nil {
			return "", err
		}
	} else {
		err = filepath.WalkDir(base, func(path string, d fs.DirEntry, walkErr error) error {
			if walkErr != nil {
				return walkErr
			}
			if d.IsDir() {
				return nil
			}
			return addMatches(path)
		})
		if err != nil {
			return "", err
		}
	}
	sort.Strings(matches)
	out, err := json.Marshal(matches)
	if err != nil {
		return "", err
	}
	return string(out), nil
}

func sliceLines(content string, offset, limit int) string {
	if offset <= 1 && limit <= 0 {
		return content
	}
	lines := strings.SplitAfter(content, "\n")
	start := 0
	if offset > 1 {
		start = offset - 1
	}
	if start >= len(lines) {
		return ""
	}
	end := len(lines)
	if limit > 0 && start+limit < end {
		end = start + limit
	}
	return strings.Join(lines[start:end], "")
}

func searchPath(root, p string) (string, error) {
	if p == "" {
		return sandboxRoot(root)
	}
	return confinePath(root, p)
}

func sandboxRoot(root string) (string, error) {
	if root == "" {
		return os.Getwd()
	}
	abs, err := filepath.Abs(root)
	if err != nil {
		return "", err
	}
	return filepath.Clean(abs), nil
}

func confinePath(root, p string) (string, error) {
	if strings.TrimSpace(p) == "" {
		return "", errors.New("path is required")
	}
	base, err := sandboxRoot(root)
	if err != nil {
		return "", err
	}
	path := p
	if !filepath.IsAbs(path) {
		path = filepath.Join(base, path)
	}
	path = filepath.Clean(path)
	realBase := resolveExisting(base)
	resolved := resolveExisting(path)
	rel, err := filepath.Rel(realBase, resolved)
	if err != nil || rel == ".." || strings.HasPrefix(rel, ".."+string(os.PathSeparator)) {
		return "", fmt.Errorf("path escapes sandbox: %q", p)
	}
	return path, nil
}

func resolveExisting(path string) string {
	existing := filepath.Clean(path)
	var remainder string
	for {
		if _, err := os.Lstat(existing); err == nil {
			break
		}
		parent := filepath.Dir(existing)
		if parent == existing {
			return filepath.Clean(path)
		}
		remainder = filepath.Join(filepath.Base(existing), remainder)
		existing = parent
	}
	resolved, err := filepath.EvalSymlinks(existing)
	if err != nil {
		resolved = existing
	}
	if remainder == "" {
		return filepath.Clean(resolved)
	}
	return filepath.Clean(filepath.Join(resolved, remainder))
}
