package mcp

import (
	"reflect"
	"strings"
	"testing"
)

func TestConfigSchemaIncludesProviderModelAndOptionalExpansion(t *testing.T) {
	// R-KE1K-MUZ4
	createConfig := inputConfigSchema(t, "create")
	updateConfig := inputConfigSchema(t, "update")
	if !reflect.DeepEqual(createConfig, updateConfig) {
		t.Fatalf("create and update config schemas differ:\ncreate=%#v\nupdate=%#v", createConfig, updateConfig)
	}

	required, ok := createConfig["required"].([]string)
	if !ok {
		t.Fatalf("config required field has type %T: %#v", createConfig["required"], createConfig["required"])
	}
	if !reflect.DeepEqual(required, []string{"provider", "model"}) {
		t.Fatalf("required config keys = %v, want [provider model]", required)
	}

	properties, ok := createConfig["properties"].(map[string]any)
	if !ok {
		t.Fatalf("config properties has type %T: %#v", createConfig["properties"], createConfig["properties"])
	}
	wantTypes := map[string]string{
		"provider":           "string",
		"model":              "string",
		"temperature":        "number",
		"top_p":              "number",
		"max_tokens":         "integer",
		"effort":             "string",
		"thinking_budget":    "integer",
		"thinking_level":     "string",
		"thinking":           "string",
		"max_attempts":       "integer",
		"base_delay":         "string",
		"max_delay":          "string",
		"max_elapsed":        "string",
		"ignore_retry_after": "boolean",
		"tool_loop_limit":    "integer",
		"base_url":           "string",
	}
	for key, wantType := range wantTypes {
		prop, ok := properties[key].(map[string]any)
		if !ok {
			t.Fatalf("config property %q missing or wrong type: %#v", key, properties[key])
		}
		if got := prop["type"]; got != wantType {
			t.Fatalf("config property %q type = %v, want %q", key, got, wantType)
		}
		if _, hasEnum := prop["enum"]; hasEnum {
			t.Fatalf("config property %q must not define an enum: %#v", key, prop)
		}
	}
	if len(properties) != len(wantTypes) {
		t.Fatalf("config property count = %d, want %d: %#v", len(properties), len(wantTypes), properties)
	}
}

func TestDescribeDescriptorDocumentsExpandedConfigAndJSONL(t *testing.T) {
	// R-KF9H-0MPT
	description, ok := findToolDescriptor(t, "describe")["description"].(string)
	if !ok || description == "" {
		t.Fatalf("describe descriptor has no description: %#v", findToolDescriptor(t, "describe")["description"])
	}
	for _, want := range []string{
		"anthropic",
		"openai",
		"google",
		"zai",
		"provider",
		"model",
		"temperature",
		"top_p",
		"max_tokens",
		"effort",
		"thinking_budget",
		"thinking_level",
		"thinking",
		"max_attempts",
		"base_delay",
		"max_delay",
		"max_elapsed",
		"ignore_retry_after",
		"tool_loop_limit",
		"base_url",
		"sampling",
		"retry/backoff",
		"LogRecord JSONL",
	} {
		if !strings.Contains(description, want) {
			t.Fatalf("describe descriptor missing %q:\n%s", want, description)
		}
	}
}

func inputConfigSchema(t *testing.T, toolName string) map[string]any {
	t.Helper()
	toolDesc := findToolDescriptor(t, toolName)
	inputSchema, ok := toolDesc["inputSchema"].(map[string]any)
	if !ok {
		t.Fatalf("%s inputSchema has type %T: %#v", toolName, toolDesc["inputSchema"], toolDesc["inputSchema"])
	}
	properties, ok := inputSchema["properties"].(map[string]any)
	if !ok {
		t.Fatalf("%s properties has type %T: %#v", toolName, inputSchema["properties"], inputSchema["properties"])
	}
	config, ok := properties["config"].(map[string]any)
	if !ok {
		t.Fatalf("%s config schema has type %T: %#v", toolName, properties["config"], properties["config"])
	}
	return config
}

func findToolDescriptor(t *testing.T, name string) map[string]any {
	t.Helper()
	for _, desc := range toolDescriptors() {
		if desc["name"] == name {
			return desc
		}
	}
	t.Fatalf("tool descriptor %q not found", name)
	return nil
}
