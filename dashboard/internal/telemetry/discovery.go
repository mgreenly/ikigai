package telemetry

import "appkit/inventory"

// services returns MCP service names discovered from deploy manifests.
func services(manifestRoot string) ([]string, error) {
	discovered, err := inventory.Read(manifestRoot)
	if err != nil {
		return nil, err
	}
	names := make([]string, 0, len(discovered))
	for _, svc := range discovered {
		names = append(names, svc.Name)
	}
	return names, nil
}
