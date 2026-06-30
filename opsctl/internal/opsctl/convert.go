package opsctl

import (
	"bytes"
	"context"
	"errors"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"syscall"
)

// ConvertOldLayout migrates a live service from the legacy per-service layout
// into the state/cache/libexec/bin layout. It is intentionally explicit operator
// tooling: deploy/setup/start paths do not call it.
func (o *Opsctl) ConvertOldLayout(ctx context.Context, app string) error {
	if err := ctx.Err(); err != nil {
		return err
	}
	if app == "" {
		return fmt.Errorf("convert: app is required")
	}

	l := o.layout(app)
	oldData := filepath.Join(l.AppDir(), "data")
	if err := os.MkdirAll(l.StateDir(), 0o755); err != nil {
		return fmt.Errorf("convert: mkdir state: %w", err)
	}
	if err := os.MkdirAll(l.CacheDir(), 0o755); err != nil {
		return fmt.Errorf("convert: mkdir cache: %w", err)
	}
	if err := os.MkdirAll(l.LibexecDir(), 0o755); err != nil {
		return fmt.Errorf("convert: mkdir libexec: %w", err)
	}

	for _, name := range []string{app + ".db", app + ".db-wal", app + ".db-shm"} {
		if err := moveIfPresent(filepath.Join(oldData, name), filepath.Join(l.StateDir(), name)); err != nil {
			return fmt.Errorf("convert: move %s: %w", name, err)
		}
	}
	if err := moveIfPresent(filepath.Join(oldData, app+".db.generation"), l.GenerationPath()); err != nil {
		return fmt.Errorf("convert: move generation: %w", err)
	}

	version, oldBinary, err := legacyLiveBinary(l)
	if err != nil {
		return err
	}
	if version != "" {
		newBinary := l.LibexecBinary(version)
		if err := moveIfPresent(oldBinary, newBinary); err != nil {
			return fmt.Errorf("convert: move live binary: %w", err)
		}
		if err := atomicSwap(l.RunLink(), filepath.Join("..", "libexec", app+"-"+version)); err != nil {
			return fmt.Errorf("convert: point bin/run: %w", err)
		}
	}

	return ctx.Err()
}

func legacyLiveBinary(l Layout) (version, binary string, err error) {
	current := filepath.Join(l.AppDir(), "current")
	target, err := os.Readlink(current)
	if err != nil {
		if os.IsNotExist(err) {
			return "", "", nil
		}
		return "", "", fmt.Errorf("convert: read current: %w", err)
	}
	targetPath := target
	if !filepath.IsAbs(targetPath) {
		targetPath = filepath.Join(l.AppDir(), targetPath)
	}
	targetPath = filepath.Clean(targetPath)

	fi, err := os.Stat(targetPath)
	if err != nil {
		return "", "", fmt.Errorf("convert: stat current target: %w", err)
	}
	if fi.IsDir() {
		version = filepath.Base(targetPath)
		binary = filepath.Join(targetPath, l.App)
		if _, err := os.Stat(binary); err == nil {
			return version, binary, nil
		}
		if _, err := os.Stat(l.LibexecBinary(version)); err == nil {
			return version, l.LibexecBinary(version), nil
		}
		found, err := singleExecutable(targetPath)
		if err != nil {
			return "", "", err
		}
		return version, found, nil
	}

	version = filepath.Base(filepath.Dir(targetPath))
	return version, targetPath, nil
}

func singleExecutable(dir string) (string, error) {
	entries, err := os.ReadDir(dir)
	if err != nil {
		return "", fmt.Errorf("convert: read release dir: %w", err)
	}
	var found string
	for _, e := range entries {
		if e.IsDir() {
			continue
		}
		path := filepath.Join(dir, e.Name())
		fi, err := e.Info()
		if err != nil {
			return "", fmt.Errorf("convert: stat release entry: %w", err)
		}
		if fi.Mode().IsRegular() && fi.Mode().Perm()&0o111 != 0 {
			if found != "" {
				return "", fmt.Errorf("convert: release dir %s has multiple executable files; expected %s", dir, filepath.Base(dir))
			}
			found = path
		}
	}
	if found == "" {
		return "", fmt.Errorf("convert: release dir %s has no executable live binary", dir)
	}
	return found, nil
}

func moveIfPresent(src, dst string) error {
	if src == dst {
		return nil
	}
	srcInfo, err := os.Lstat(src)
	if err != nil {
		if os.IsNotExist(err) {
			return nil
		}
		return err
	}
	if err := os.MkdirAll(filepath.Dir(dst), 0o755); err != nil {
		return err
	}
	if _, err := os.Lstat(dst); err == nil {
		same, cmpErr := sameFileContents(src, dst)
		if cmpErr != nil {
			return cmpErr
		}
		if !same {
			return fmt.Errorf("%s already exists with different contents", dst)
		}
		if srcInfo.IsDir() {
			return os.RemoveAll(src)
		}
		return os.Remove(src)
	} else if !os.IsNotExist(err) {
		return err
	}
	if err := os.Rename(src, dst); err == nil {
		return nil
	} else if !isCrossDevice(err) {
		return err
	}
	if err := copyPath(src, dst, srcInfo); err != nil {
		return err
	}
	if srcInfo.IsDir() {
		return os.RemoveAll(src)
	}
	return os.Remove(src)
}

func sameFileContents(a, b string) (bool, error) {
	ai, err := os.Stat(a)
	if err != nil {
		return false, err
	}
	bi, err := os.Stat(b)
	if err != nil {
		return false, err
	}
	if ai.IsDir() || bi.IsDir() {
		return ai.IsDir() == bi.IsDir(), nil
	}
	if ai.Size() != bi.Size() {
		return false, nil
	}
	ab, err := os.ReadFile(a)
	if err != nil {
		return false, err
	}
	bb, err := os.ReadFile(b)
	if err != nil {
		return false, err
	}
	return bytes.Equal(ab, bb), nil
}

func isCrossDevice(err error) bool {
	linkErr, ok := err.(*os.LinkError)
	return ok && errors.Is(linkErr.Err, syscall.EXDEV)
}

func copyPath(src, dst string, info os.FileInfo) error {
	if info.IsDir() {
		return fmt.Errorf("directory moves across devices are not supported: %s", src)
	}
	in, err := os.Open(src)
	if err != nil {
		return err
	}
	defer in.Close()
	out, err := os.OpenFile(dst, os.O_CREATE|os.O_WRONLY|os.O_EXCL, info.Mode().Perm())
	if err != nil {
		return err
	}
	if _, err := io.Copy(out, in); err != nil {
		out.Close()
		os.Remove(dst)
		return err
	}
	if err := out.Close(); err != nil {
		os.Remove(dst)
		return err
	}
	return os.Chmod(dst, info.Mode().Perm())
}
