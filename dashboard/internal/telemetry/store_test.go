package telemetry

import (
	"testing"
	"time"
)

func TestStoreRetainsLastMaxSamplesInInsertionOrder(t *testing.T) {
	store := NewStore()
	base := time.Date(2026, 7, 5, 12, 0, 0, 0, time.UTC)

	for i := 0; i < MaxSamples+1; i++ {
		store.Append("mem.free", Sample{
			At:    base.Add(time.Duration(i) * time.Minute),
			Value: int64(i),
		})
	}

	samples := store.Snapshot().Series["mem.free"]
	// R-EZVQ-IQOL
	if len(samples) != MaxSamples {
		t.Fatalf("snapshot length = %d, want %d", len(samples), MaxSamples)
	}
	if samples[0].Value != 1 {
		t.Fatalf("first retained sample value = %d, want 1", samples[0].Value)
	}
	if samples[len(samples)-1].Value != MaxSamples {
		t.Fatalf("last retained sample value = %d, want %d", samples[len(samples)-1].Value, MaxSamples)
	}
}

func TestSnapshotIsIndependentOfLiveStore(t *testing.T) {
	store := NewStore()
	store.Append("disk.free", Sample{Value: 7})

	snapshot := store.Snapshot()
	store.Append("disk.free", Sample{Value: 9})

	samples := snapshot.Series["disk.free"]
	// R-F13M-WIFA
	if len(samples) != 1 {
		t.Fatalf("snapshot length after later append = %d, want 1", len(samples))
	}
	if samples[0].Value != 7 {
		t.Fatalf("snapshot first value after later append = %d, want 7", samples[0].Value)
	}
}

func TestSnapshotCarriesCapacities(t *testing.T) {
	store := NewStore()
	store.SetCapacities(16*1024, 2048*1024)

	snapshot := store.Snapshot()
	// R-F2BJ-AA5Z
	if snapshot.TotalMem != 16*1024 {
		t.Fatalf("TotalMem = %d, want %d", snapshot.TotalMem, 16*1024)
	}
	if snapshot.TotalDisk != 2048*1024 {
		t.Fatalf("TotalDisk = %d, want %d", snapshot.TotalDisk, 2048*1024)
	}
}
