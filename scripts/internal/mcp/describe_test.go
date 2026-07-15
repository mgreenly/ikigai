package mcp

import (
	"strings"
	"testing"
)

func TestDescribeTeachesSuiteRuntimeContract(t *testing.T) {
	// R-IOUA-M8K1
	result, err := toolDescribe()
	if err != nil {
		t.Fatalf("toolDescribe() error = %v", err)
	}
	content, ok := result["content"].([]map[string]any)
	if !ok || len(content) != 1 {
		t.Fatalf("describe content = %#v, want one content block", result["content"])
	}
	text, ok := content[0]["text"].(string)
	if !ok {
		t.Fatalf("describe text = %#v, want string", content[0]["text"])
	}

	for _, want := range []string{
		"suite module",
		"suite.event",
		"suite.mcp",
		"suite.fetch",
		"suite.files",
		"suite.ToolError",
		"not_found",
		"source_unavailable",
		"Non-directory run_fs_list entries",
		"content_url",
	} {
		if !strings.Contains(text, want) {
			t.Errorf("describe result does not contain %q", want)
		}
	}

	shareStart := strings.Index(text, "suite.files.*")
	if shareStart < 0 {
		t.Fatalf("describe result does not start the file share guidance")
	}
	shareEnd := strings.Index(text[shareStart:], "- Suite-service failures")
	if shareEnd < 0 {
		t.Fatalf("describe result does not end the file share guidance")
	}
	shareGuidance := text[shareStart : shareStart+shareEnd]
	if !strings.Contains(shareGuidance, "file share") {
		t.Errorf("file share guidance does not name the file share")
	}
	if strings.Contains(strings.ToLower(shareGuidance), "dropbox") {
		t.Errorf("file share guidance names its backing service: %q", shareGuidance)
	}
}

func TestDescribeTeachesAbsoluteSharePaths(t *testing.T) {
	// R-ZGSP-VKCD
	result, err := toolDescribe()
	if err != nil {
		t.Fatalf("toolDescribe() error = %v", err)
	}
	content, ok := result["content"].([]map[string]any)
	if !ok || len(content) != 1 {
		t.Fatalf("describe content = %#v, want one content block", result["content"])
	}
	text, ok := content[0]["text"].(string)
	if !ok {
		t.Fatalf("describe text = %#v, want string", content[0]["text"])
	}

	for _, want := range []string{
		"Share paths are absolute and /-rooted; relative spellings are accepted and treated as rooted.",
		`suite.files.put("summary.pdf", "/reports/summary.pdf")`,
	} {
		if !strings.Contains(text, want) {
			t.Errorf("describe result does not contain %q", want)
		}
	}
	if strings.Contains(text, `suite.files.put("reports/summary.pdf", "summary.pdf")`) {
		t.Errorf("describe result still teaches a relative share path")
	}
}
