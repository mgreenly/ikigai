package repos

import (
	"context"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"reflect"
	"testing"
)

func TestProtocolAdmissionOrdersLabelsAndAssertsIdentity(t *testing.T) {
	// R-FDAF-MVIC
	var names []string
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		var call struct {
			Params struct {
				Name string `json:"name"`
			} `json:"params"`
		}
		if err := json.NewDecoder(r.Body).Decode(&call); err != nil {
			t.Error(err)
		}
		if r.Header.Get("X-Owner-Email") != "owner@example.com" || r.Header.Get("X-Client-Id") != "repos:session-7" {
			t.Errorf("identity headers = %q, %q", r.Header.Get("X-Owner-Email"), r.Header.Get("X-Client-Id"))
		}
		names = append(names, call.Params.Name)
		json.NewEncoder(w).Encode(map[string]any{"jsonrpc": "2.0", "id": 1, "result": map[string]any{}})
	}))
	defer server.Close()
	issue := 7
	protocol := NewProtocol(NewGitHubPeerAt(server.URL, server.Client()))
	if err := protocol.Admit(context.Background(), Session{
		ID: "session-7", RepoName: "alpha", OwnerEmail: "owner@example.com", IssueNumber: &issue, Attempt: 1,
	}); err != nil {
		t.Fatal(err)
	}
	if want := []string{"label_remove", "label_add", "issue_comment"}; !reflect.DeepEqual(names, want) {
		t.Fatalf("calls = %v, want %v", names, want)
	}
}

func TestProtocolRetryAdmissionRemovesFailedLabel(t *testing.T) {
	// R-FKLT-XHYI
	var names []string
	var labels []string
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		var call struct {
			Params struct {
				Name      string         `json:"name"`
				Arguments map[string]any `json:"arguments"`
			} `json:"params"`
		}
		json.NewDecoder(r.Body).Decode(&call)
		names = append(names, call.Params.Name)
		if label, ok := call.Params.Arguments["label"].(string); ok {
			labels = append(labels, label)
		}
		json.NewEncoder(w).Encode(map[string]any{"jsonrpc": "2.0", "id": 1, "result": map[string]any{}})
	}))
	defer server.Close()
	issue := 9
	protocol := NewProtocol(NewGitHubPeerAt(server.URL, server.Client()))
	if err := protocol.Admit(context.Background(), Session{
		ID: "retry", RepoName: "alpha", OwnerEmail: "owner@example.com", IssueNumber: &issue, Attempt: 2,
	}); err != nil {
		t.Fatal(err)
	}
	if !reflect.DeepEqual(names, []string{"label_remove", "label_remove", "label_add", "issue_comment"}) ||
		!reflect.DeepEqual(labels, []string{"execute", "failed"}) {
		t.Fatalf("retry calls = %v, labels = %v", names, labels)
	}
}
