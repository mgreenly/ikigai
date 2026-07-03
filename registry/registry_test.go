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

func TestPortKnownReturnsRegisteredPort(t *testing.T) {
	// R-B642-6EO8
	tests := map[string]int{
		"crm":       3100,
		"dashboard": 3000,
	}

	for name, wantPort := range tests {
		port, ok := Port(name)
		if !ok {
			t.Fatalf("Port(%q) ok = false, want true", name)
		}
		if port != wantPort {
			t.Fatalf("Port(%q) port = %d, want %d", name, port, wantPort)
		}
	}
}

func TestPortUnknownReturnsFalse(t *testing.T) {
	// R-B7BY-K6EX
	port, ok := Port("missing")
	if ok {
		t.Fatal(`Port("missing") ok = true, want false`)
	}
	if port != 0 {
		t.Fatalf(`Port("missing") port = %d, want 0`, port)
	}
}

func TestMustPortReturnsKnownAndPanicsUnknown(t *testing.T) {
	// R-B8JU-XY5M
	if port := MustPort("crm"); port != 3100 {
		t.Fatalf(`MustPort("crm") = %d, want 3100`, port)
	}

	defer func() {
		if recovered := recover(); recovered == nil {
			t.Fatal(`MustPort("missing") did not panic`)
		}
	}()
	MustPort("missing")
}

func TestBaseURLComposesLoopbackOrigin(t *testing.T) {
	// R-B9RR-BPWB
	if got, want := BaseURL("crm"), "http://127.0.0.1:3100"; got != want {
		t.Fatalf(`BaseURL("crm") = %q, want %q`, got, want)
	}
}
