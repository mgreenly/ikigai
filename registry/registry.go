package registry

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
