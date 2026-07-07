package files

import (
	"bufio"
	"crypto/md5"
	"encoding/hex"
	"errors"
	"fmt"
	"io"
	"io/fs"
	"os"
	"path/filepath"
	"regexp"
	"sort"
	"strings"
)

// ErrEscapes is the sentinel every confinement rejection wraps.
var ErrEscapes = errors.New("path escapes the working dir")

type Match struct {
	Path string
	Line int
	Text string
}

type FileInfo struct {
	Path string
	Size int64
	Md5  string
}

// ConfinePath resolves p against root and verifies the result stays inside root.
func ConfinePath(root, p string) (string, error) {
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
	path, err = filepath.Abs(path)
	if err != nil {
		return "", err
	}
	resolved := resolveExisting(path)
	if !inside(base, resolved) {
		return "", fmt.Errorf("%w: %q", ErrEscapes, p)
	}
	return resolved, nil
}

func Read(root, path string, offset, limit int) (string, error) {
	confined, err := ConfinePath(root, path)
	if err != nil {
		return "", err
	}
	content, err := os.ReadFile(confined)
	if err != nil {
		return "", err
	}
	return sliceLines(string(content), offset, limit), nil
}

func Write(root, path, content string, appendFile bool) error {
	confined, err := ConfinePath(root, path)
	if err != nil {
		return err
	}
	if err := os.MkdirAll(filepath.Dir(confined), 0o755); err != nil {
		return err
	}
	flag := os.O_CREATE | os.O_WRONLY | os.O_TRUNC
	if appendFile {
		flag = os.O_CREATE | os.O_WRONLY | os.O_APPEND
	}
	file, err := os.OpenFile(confined, flag, 0o644)
	if err != nil {
		return err
	}
	_, writeErr := file.WriteString(content)
	closeErr := file.Close()
	if writeErr != nil {
		return writeErr
	}
	return closeErr
}

func Edit(root, path, oldStr, newStr string, replaceAll bool) (int, error) {
	if oldStr == "" {
		return 0, errors.New("old string is required")
	}
	confined, err := ConfinePath(root, path)
	if err != nil {
		return 0, err
	}
	content, err := os.ReadFile(confined)
	if err != nil {
		return 0, err
	}
	text := string(content)
	replaced := strings.Count(text, oldStr)
	if replaced == 0 {
		return 0, errors.New("old string not found")
	}
	count := 1
	if replaceAll {
		count = -1
	} else {
		replaced = 1
	}
	updated := strings.Replace(text, oldStr, newStr, count)
	if err := os.WriteFile(confined, []byte(updated), 0o644); err != nil {
		return 0, err
	}
	return replaced, nil
}

func Glob(root, pattern, path string) ([]string, error) {
	if pattern == "" {
		return nil, errors.New("pattern is required")
	}
	base, err := searchPath(root, path)
	if err != nil {
		return nil, err
	}
	relPattern, err := confinedGlobPattern(base, pattern)
	if err != nil {
		return nil, err
	}
	if err := validateSlashPattern(relPattern); err != nil {
		return nil, err
	}

	out := []string{}
	err = filepath.WalkDir(base, func(path string, d fs.DirEntry, walkErr error) error {
		if walkErr != nil {
			return walkErr
		}
		if d.IsDir() || !d.Type().IsRegular() {
			return nil
		}
		rel, err := filepath.Rel(base, path)
		if err != nil {
			return nil
		}
		rel = filepath.ToSlash(rel)
		ok, err := matchSlashPattern(relPattern, rel)
		if err != nil {
			return err
		}
		if ok {
			out = append(out, rel)
		}
		return nil
	})
	if err != nil {
		return nil, err
	}
	sort.Strings(out)
	return out, nil
}

func confinedGlobPattern(base, pattern string) (string, error) {
	absPattern := pattern
	if !filepath.IsAbs(absPattern) {
		absPattern = filepath.Join(base, absPattern)
	}
	absPattern = filepath.Clean(absPattern)
	if !inside(base, absPattern) {
		return "", fmt.Errorf("%w: %q", ErrEscapes, pattern)
	}
	literalPrefix := globLiteralPrefix(base, absPattern)
	if !inside(base, resolveExisting(literalPrefix)) {
		return "", fmt.Errorf("%w: %q", ErrEscapes, pattern)
	}
	relPattern, err := filepath.Rel(base, absPattern)
	if err != nil {
		return "", err
	}
	return filepath.ToSlash(relPattern), nil
}

func globLiteralPrefix(base, absPattern string) string {
	rel, err := filepath.Rel(base, absPattern)
	if err != nil || rel == "." {
		return absPattern
	}
	prefix := base
	for _, segment := range strings.Split(rel, string(os.PathSeparator)) {
		if segment == "" || segment == "." {
			continue
		}
		if segment == ".." || segment == "**" || hasGlobMeta(segment) {
			break
		}
		prefix = filepath.Join(prefix, segment)
	}
	return prefix
}

func hasGlobMeta(segment string) bool {
	return strings.ContainsAny(segment, "*?[")
}

func matchSlashPattern(pattern, path string) (bool, error) {
	if pattern == "." {
		return path == ".", nil
	}
	return matchPatternSegments(strings.Split(pattern, "/"), strings.Split(path, "/"))
}

func validateSlashPattern(pattern string) error {
	for _, segment := range strings.Split(pattern, "/") {
		if segment == "**" {
			continue
		}
		if _, err := filepath.Match(segment, ""); err != nil {
			return err
		}
	}
	return nil
}

func matchPatternSegments(pattern, path []string) (bool, error) {
	if len(pattern) == 0 {
		return len(path) == 0, nil
	}
	if pattern[0] == "**" {
		for i := 0; i <= len(path); i++ {
			ok, err := matchPatternSegments(pattern[1:], path[i:])
			if err != nil || ok {
				return ok, err
			}
		}
		return false, nil
	}
	if len(path) == 0 {
		return false, nil
	}
	ok, err := filepath.Match(pattern[0], path[0])
	if err != nil || !ok {
		return ok, err
	}
	return matchPatternSegments(pattern[1:], path[1:])
}

func Grep(root, pattern, path, glob string) ([]Match, error) {
	if pattern == "" {
		return nil, errors.New("pattern is required")
	}
	re, err := regexp.Compile(pattern)
	if err != nil {
		return nil, err
	}
	base, err := searchPath(root, path)
	if err != nil {
		return nil, err
	}
	rootBase, err := sandboxRoot(root)
	if err != nil {
		return nil, err
	}
	var matches []Match
	addMatches := func(path string) error {
		if glob != "" {
			ok, err := filepath.Match(glob, filepath.Base(path))
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
		rel, err := filepath.Rel(rootBase, path)
		if err != nil {
			rel = path
		}
		rel = filepath.ToSlash(rel)
		scanner := bufio.NewScanner(file)
		for line := 1; scanner.Scan(); line++ {
			text := scanner.Text()
			if re.MatchString(text) {
				matches = append(matches, Match{Path: rel, Line: line, Text: text})
			}
		}
		return scanner.Err()
	}
	info, err := os.Stat(base)
	if err != nil {
		return nil, err
	}
	if !info.IsDir() {
		if err := addMatches(base); err != nil {
			return nil, err
		}
	} else {
		err = filepath.WalkDir(base, func(path string, d fs.DirEntry, walkErr error) error {
			if walkErr != nil {
				return walkErr
			}
			if d.IsDir() || !d.Type().IsRegular() {
				return nil
			}
			confined, err := ConfinePath(root, path)
			if err != nil {
				return nil
			}
			return addMatches(confined)
		})
		if err != nil {
			return nil, err
		}
	}
	sort.Slice(matches, func(i, j int) bool {
		if matches[i].Path != matches[j].Path {
			return matches[i].Path < matches[j].Path
		}
		return matches[i].Line < matches[j].Line
	})
	return matches, nil
}

func List(root, scope string) ([]FileInfo, error) {
	base, err := searchPath(root, scope)
	if err != nil {
		return nil, err
	}
	rootBase, err := sandboxRoot(root)
	if err != nil {
		return nil, err
	}
	info, err := os.Stat(base)
	if errors.Is(err, fs.ErrNotExist) {
		return []FileInfo{}, nil
	}
	if err != nil {
		return nil, err
	}
	var out []FileInfo
	addFile := func(path string) error {
		md5hex, size, err := hashFile(path)
		if err != nil {
			return err
		}
		rel, err := filepath.Rel(rootBase, path)
		if err != nil {
			return err
		}
		out = append(out, FileInfo{Path: filepath.ToSlash(rel), Size: size, Md5: md5hex})
		return nil
	}
	if !info.IsDir() {
		if info.Mode().IsRegular() {
			if err := addFile(base); err != nil {
				return nil, err
			}
		}
		return out, nil
	}
	err = filepath.WalkDir(base, func(path string, d fs.DirEntry, walkErr error) error {
		if walkErr != nil {
			return walkErr
		}
		if d.IsDir() || !d.Type().IsRegular() {
			return nil
		}
		confined, err := ConfinePath(root, path)
		if err != nil {
			return nil
		}
		return addFile(confined)
	})
	if err != nil {
		return nil, err
	}
	sort.Slice(out, func(i, j int) bool { return out[i].Path < out[j].Path })
	return out, nil
}

func Mkdir(root, path string) error {
	confined, err := ConfinePath(root, path)
	if err != nil {
		return err
	}
	return os.MkdirAll(confined, 0o755)
}

func searchPath(root, p string) (string, error) {
	if p == "" {
		return sandboxRoot(root)
	}
	return ConfinePath(root, p)
}

func sandboxRoot(root string) (string, error) {
	if root == "" {
		var err error
		root, err = os.Getwd()
		if err != nil {
			return "", err
		}
	}
	abs, err := filepath.Abs(root)
	if err != nil {
		return "", err
	}
	real, err := filepath.EvalSymlinks(abs)
	if err != nil {
		return "", err
	}
	return filepath.Clean(real), nil
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

func inside(root, path string) bool {
	rel, err := filepath.Rel(root, path)
	if err != nil {
		return false
	}
	return rel == "." || (rel != ".." && !strings.HasPrefix(rel, ".."+string(os.PathSeparator)))
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

func hashFile(path string) (string, int64, error) {
	file, err := os.Open(path)
	if err != nil {
		return "", 0, err
	}
	defer file.Close()
	hash := md5.New()
	size, err := io.Copy(hash, file)
	if err != nil {
		return "", 0, err
	}
	return hex.EncodeToString(hash.Sum(nil)), size, nil
}
