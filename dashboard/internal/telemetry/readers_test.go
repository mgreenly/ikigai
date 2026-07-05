package telemetry

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestReadMemInfoParsesAvailableAndTotalKiBAsBytes(t *testing.T) {
	fixture := strings.NewReader("SwapTotal: 0 kB\nMemTotal: 8000 kB\nMemAvailable: 1234 kB\n")

	avail, total, err := readMemInfo(fixture)
	if err != nil {
		t.Fatalf("readMemInfo returned error: %v", err)
	}
	// R-F4RC-1TND
	if avail != 1234*1024 {
		t.Fatalf("MemAvailable bytes = %d, want %d", avail, 1234*1024)
	}
	if total != 8000*1024 {
		t.Fatalf("MemTotal bytes = %d, want %d", total, 8000*1024)
	}
}

func TestReadDiskFreeReportsRealFilesystemValues(t *testing.T) {
	free, total, err := readDiskFree(t.TempDir())
	if err != nil {
		t.Fatalf("readDiskFree returned error: %v", err)
	}
	// R-F5Z8-FLE2
	if total <= 0 {
		t.Fatalf("total = %d, want a non-zero filesystem total", total)
	}
	if free < 0 {
		t.Fatalf("free = %d, want non-negative free bytes", free)
	}
	if free > total {
		t.Fatalf("free = %d, want <= total %d", free, total)
	}
}

func TestReadCgroupMemReadsServiceMemoryCurrent(t *testing.T) {
	root := t.TempDir()
	path := filepath.Join(root, "system.slice", "crm.service")
	if err := os.MkdirAll(path, 0o755); err != nil {
		t.Fatalf("create cgroup fixture: %v", err)
	}
	if err := os.WriteFile(filepath.Join(path, "memory.current"), []byte("123456\n"), 0o644); err != nil {
		t.Fatalf("write memory.current: %v", err)
	}

	value, err := readCgroupMem(root, "crm")
	if err != nil {
		t.Fatalf("readCgroupMem returned error: %v", err)
	}
	// R-F774-TD4R
	if value != 123456 {
		t.Fatalf("memory.current value = %d, want 123456", value)
	}
}

func TestReadCgroupMemAbsentPathReturnsZeroWithoutError(t *testing.T) {
	value, err := readCgroupMem(t.TempDir(), "crm")
	if err != nil {
		t.Fatalf("readCgroupMem returned error: %v", err)
	}
	// R-F8F1-74VG
	if value != 0 {
		t.Fatalf("absent cgroup value = %d, want 0", value)
	}
}

func TestDirSizeSumsNestedRegularFiles(t *testing.T) {
	root := t.TempDir()
	nested := filepath.Join(root, "nested")
	if err := os.MkdirAll(nested, 0o755); err != nil {
		t.Fatalf("create nested fixture: %v", err)
	}
	if err := os.WriteFile(filepath.Join(root, "one.bin"), []byte("abcde"), 0o644); err != nil {
		t.Fatalf("write one.bin: %v", err)
	}
	if err := os.WriteFile(filepath.Join(nested, "two.bin"), []byte("abcdefg"), 0o644); err != nil {
		t.Fatalf("write two.bin: %v", err)
	}

	size, err := dirSize(root)
	if err != nil {
		t.Fatalf("dirSize returned error: %v", err)
	}
	// R-F9MX-KWM5
	if size != 12 {
		t.Fatalf("dirSize = %d, want 12", size)
	}
}

func TestDirSizeAbsentDirectoryReturnsZeroWithoutError(t *testing.T) {
	size, err := dirSize(filepath.Join(t.TempDir(), "missing"))
	if err != nil {
		t.Fatalf("dirSize returned error: %v", err)
	}
	// R-FAUT-YOCU
	if size != 0 {
		t.Fatalf("absent dir size = %d, want 0", size)
	}
}
