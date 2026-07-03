package registry

import "strconv"

// Block groups services by type; the port number carries a hint of the kind.
type Block int

const (
	Core Block = iota
	Apps
	Connectors
	Custom
)

// Service is one row of the registry: identity, address, and type.
type Service struct {
	Name  string
	Port  int
	Block Block
}

// Services is the authoritative, frozen registry. Assignments are fixed by name;
// adding a service appends a row, it never shifts an existing one.
var Services = []Service{
	// core - 3000+, counts up
	{"dashboard", 3000, Core},
	{"wiki", 3001, Core},
	{"prompts", 3002, Core},
	{"scripts", 3003, Core},
	{"sites", 3004, Core},
	{"cron", 3005, Core},
	{"webhooks", 3006, Core},

	// applications - 3100+, counts up
	{"crm", 3100, Apps},
	{"ledger", 3101, Apps},

	// connectors - 3200+, counts up
	{"dropbox", 3200, Connectors},
	{"notify", 3201, Connectors},
	{"gmail", 3202, Connectors},
	{"github", 3203, Connectors},
}

const loopbackHost = "127.0.0.1"

var servicePorts = func() map[string]int {
	ports := make(map[string]int, len(Services))
	for _, service := range Services {
		ports[service.Name] = service.Port
	}
	return ports
}()

// Port returns the loopback port for a known service name.
func Port(name string) (port int, ok bool) {
	port, ok = servicePorts[name]
	if !ok {
		return 0, false
	}
	return port, true
}

// MustPort returns the port for name, or panics if name is unknown.
func MustPort(name string) int {
	port, ok := Port(name)
	if !ok {
		panic("registry: unknown service " + strconv.Quote(name))
	}
	return port
}

// BaseURL returns the loopback HTTP origin for a known service name.
func BaseURL(name string) string {
	return "http://" + loopbackHost + ":" + strconv.Itoa(MustPort(name))
}

// Range returns the inclusive port range a Block owns.
func (b Block) Range() (lo, hi int) {
	switch b {
	case Core, Custom:
		return 3000, 3099
	case Apps:
		return 3100, 3199
	case Connectors:
		return 3200, 3299
	default:
		return 0, -1
	}
}
