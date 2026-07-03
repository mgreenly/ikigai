package registry

import "testing"

func TestServicesHaveUniquePorts(t *testing.T) {
	// R-B00K-9JYR
	seen := make(map[int]string, len(Services))
	for _, service := range Services {
		if previous, exists := seen[service.Port]; exists {
			t.Fatalf("port %d is assigned to both %q and %q", service.Port, previous, service.Name)
		}
		seen[service.Port] = service.Name
	}
}

func TestServicesHaveUniqueNames(t *testing.T) {
	// R-B18G-NBPG
	seen := make(map[string]int, len(Services))
	for _, service := range Services {
		if previous, exists := seen[service.Name]; exists {
			t.Fatalf("name %q is assigned to both port %d and port %d", service.Name, previous, service.Port)
		}
		seen[service.Name] = service.Port
	}
}

func TestServicesAreWithinBlockRanges(t *testing.T) {
	// R-B2GD-13G5
	expectedRanges := map[Block]struct {
		lo int
		hi int
	}{
		Core:       {lo: 3000, hi: 3099},
		Apps:       {lo: 3100, hi: 3199},
		Connectors: {lo: 3200, hi: 3299},
		Custom:     {lo: 3000, hi: 3099},
	}

	for block, expected := range expectedRanges {
		lo, hi := block.Range()
		if lo != expected.lo || hi != expected.hi {
			t.Fatalf("block %d range = %d-%d, want %d-%d", block, lo, hi, expected.lo, expected.hi)
		}
	}

	for _, service := range Services {
		lo, hi := service.Block.Range()
		if service.Port < lo || service.Port > hi {
			t.Fatalf("%q uses port %d outside block %d range %d-%d", service.Name, service.Port, service.Block, lo, hi)
		}
	}
}

func TestDashboardPinnedToDefaultPort(t *testing.T) {
	// R-B3O9-EV6U
	for _, service := range Services {
		if service.Name == "dashboard" {
			if service.Port != 3000 {
				t.Fatalf("dashboard port = %d, want 3000", service.Port)
			}
			return
		}
	}
	t.Fatal("dashboard is missing from Services")
}
