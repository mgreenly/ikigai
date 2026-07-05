package telemetry

import (
	"bufio"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"syscall"
)

// readMemInfo parses MemAvailable and MemTotal from /proc/meminfo content.
func readMemInfo(r io.Reader) (avail, total int64, err error) {
	scanner := bufio.NewScanner(r)
	var availSeen, totalSeen bool
	for scanner.Scan() {
		fields := strings.Fields(scanner.Text())
		if len(fields) < 2 {
			continue
		}
		key := strings.TrimSuffix(fields[0], ":")
		if key != "MemAvailable" && key != "MemTotal" {
			continue
		}
		kib, err := strconv.ParseInt(fields[1], 10, 64)
		if err != nil {
			return 0, 0, fmt.Errorf("parse %s: %w", key, err)
		}
		switch key {
		case "MemAvailable":
			avail = kib * 1024
			availSeen = true
		case "MemTotal":
			total = kib * 1024
			totalSeen = true
		}
	}
	if err := scanner.Err(); err != nil {
		return 0, 0, err
	}
	if !availSeen || !totalSeen {
		return avail, total, fmt.Errorf("missing MemAvailable or MemTotal")
	}
	return avail, total, nil
}

// readDiskFree returns free and total bytes for the filesystem backing path.
func readDiskFree(path string) (free, total int64, err error) {
	if path == "" {
		path = "/opt"
	}
	var stat syscall.Statfs_t
	if err := syscall.Statfs(path, &stat); err != nil {
		if os.IsNotExist(err) {
			return 0, 0, nil
		}
		return 0, 0, err
	}
	blockSize := int64(stat.Bsize)
	return int64(stat.Bavail) * blockSize, int64(stat.Blocks) * blockSize, nil
}

// readCgroupMem reads cgroup-v2 current memory usage for svc.service.
func readCgroupMem(cgroupRoot, svc string) (int64, error) {
	if cgroupRoot == "" {
		cgroupRoot = "/sys/fs/cgroup"
	}
	path := filepath.Join(cgroupRoot, "system.slice", svc+".service", "memory.current")
	content, err := os.ReadFile(path)
	if err != nil {
		if os.IsNotExist(err) {
			return 0, nil
		}
		return 0, err
	}
	value, err := strconv.ParseInt(strings.TrimSpace(string(content)), 10, 64)
	if err != nil {
		return 0, fmt.Errorf("parse %s: %w", path, err)
	}
	return value, nil
}

// dirSize returns the summed size of regular files under dir.
func dirSize(dir string) (int64, error) {
	var total int64
	err := filepath.WalkDir(dir, func(path string, d os.DirEntry, err error) error {
		if err != nil {
			if path == dir && os.IsNotExist(err) {
				return nil
			}
			return err
		}
		info, err := d.Info()
		if err != nil {
			return err
		}
		if info.Mode().IsRegular() {
			total += info.Size()
		}
		return nil
	})
	if err != nil {
		return 0, err
	}
	return total, nil
}
