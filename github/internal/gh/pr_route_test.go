package gh

import (
	"context"
	"encoding/json"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"reflect"
	"strings"
	"testing"

	"appkit/server"
)

func TestPRHandlerLoopbackReturnsPRDetailR_EPVL_Z2UI(t *testing.T) {
	// R-EPVL-Z2UI
	var paths []string
	c := newRESTTestClient(t, func(req *http.Request) (*http.Response, error) {
		paths = append(paths, req.URL.Path)
		switch req.URL.Path {
		case "/repos/acme/widgets/pulls/7":
			assertRESTRequest(t, req, http.MethodGet, "/repos/acme/widgets/pulls/7")
			return jsonResponse(http.StatusOK, `{"number":7,"title":"Add thing","state":"open","body":"ready","html_url":"https://example.test/pr/7"}`), nil
		case "/repos/acme/widgets/pulls/7/files":
			assertRESTRequest(t, req, http.MethodGet, "/repos/acme/widgets/pulls/7/files")
			return jsonResponse(http.StatusOK, `[{"filename":"main.go","status":"modified","additions":3,"deletions":1,"changes":4,"patch":"@@ patch"}]`), nil
		default:
			t.Fatalf("unexpected path %s", req.URL.Path)
			return nil, nil
		}
	})

	rr := httptest.NewRecorder()
	req := httptest.NewRequest(http.MethodGet, "/pr?repo=widgets&number=7", nil)
	c.PRHandler().ServeHTTP(rr, req)

	if rr.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200; body %q", rr.Code, rr.Body.String())
	}
	if got := rr.Header().Get("Content-Type"); got != "application/json" {
		t.Fatalf("Content-Type = %q, want application/json", got)
	}
	var got PRDetail
	if err := json.Unmarshal(rr.Body.Bytes(), &got); err != nil {
		t.Fatalf("decode response: %v", err)
	}
	want := PRDetail{
		PR: PR{Number: 7, Title: "Add thing", State: "open", Body: "ready", HTMLURL: "https://example.test/pr/7"},
		Files: []PRFile{{
			Filename: "main.go", Status: "modified", Additions: 3, Deletions: 1, Changes: 4, Patch: "@@ patch",
		}},
	}
	if !reflect.DeepEqual(got, want) {
		t.Fatalf("PRHandler() response = %+v, want %+v", got, want)
	}
	if !reflect.DeepEqual(paths, []string{"/repos/acme/widgets/pulls/7", "/repos/acme/widgets/pulls/7/files"}) {
		t.Fatalf("paths = %v, want PRGet endpoints", paths)
	}
}

func TestPRHandlerChassisLoopbackGuardR_FT0R_PBSD(t *testing.T) {
	// R-FT0R-PBSD
	c := newPRRouteNoTransportClient(t)
	rr := httptest.NewRecorder()
	req := httptest.NewRequest(http.MethodGet, "/pr?repo=widgets&number=7", nil)
	req.Header.Set("X-Forwarded-Proto", "https")
	newPRRouteServer(t, c).ServeHTTP(rr, req)
	if rr.Code != http.StatusNotFound || rr.Body.String() != "404 page not found\n" {
		t.Fatalf("forwarded request = %d %q, want bare 404", rr.Code, rr.Body.String())
	}

	c = newRESTTestClient(t, func(req *http.Request) (*http.Response, error) {
		switch req.URL.Path {
		case "/repos/acme/widgets/pulls/7":
			return jsonResponse(http.StatusOK, `{"number":7,"title":"Add thing","state":"open","body":"ready","html_url":"https://example.test/pr/7"}`), nil
		case "/repos/acme/widgets/pulls/7/files":
			return jsonResponse(http.StatusOK, `[]`), nil
		default:
			t.Fatalf("unexpected path %s", req.URL.Path)
			return nil, nil
		}
	})
	rr = httptest.NewRecorder()
	req = httptest.NewRequest(http.MethodGet, "/pr?repo=widgets&number=7", nil)
	req.Header.Set("X-Owner-Email", "owner@example.com")
	newPRRouteServer(t, c).ServeHTTP(rr, req)
	if rr.Code != http.StatusOK || !strings.Contains(rr.Body.String(), `"number":7`) {
		t.Fatalf("loopback identity request = %d %q, want PR JSON", rr.Code, rr.Body.String())
	}
}

func newPRRouteServer(t *testing.T, c *Client) http.Handler {
	t.Helper()
	srv, err := server.New(server.Options{
		Addr:    "127.0.0.1:0",
		Logger:  slog.New(slog.DiscardHandler),
		Apex:    true,
		Version: "test",
		Service: "github",
		Register: func(rt *server.Router) error {
			rt.HandleLoopback("GET /pr", c.PRHandler())
			return nil
		},
	})
	if err != nil {
		t.Fatal(err)
	}
	return srv.Handler
}

func TestPRHandlerBadInputAndNotFoundR_ETJB_4E2L(t *testing.T) {
	// R-ETJB-4E2L
	badInputs := []string{
		"/pr",
		"/pr?repo=&number=7",
		"/pr?repo=widgets",
		"/pr?repo=widgets&number=abc",
		"/pr?repo=widgets&number=0",
		"/pr?repo=widgets&number=-1",
	}
	for _, target := range badInputs {
		t.Run(target, func(t *testing.T) {
			c := newPRRouteNoTransportClient(t)
			rr := httptest.NewRecorder()

			c.PRHandler().ServeHTTP(rr, httptest.NewRequest(http.MethodGet, target, nil))

			if rr.Code != http.StatusBadRequest {
				t.Fatalf("status = %d, want 400; body %q", rr.Code, rr.Body.String())
			}
		})
	}

	c := newRESTTestClient(t, func(req *http.Request) (*http.Response, error) {
		assertRESTRequest(t, req, http.MethodGet, "/repos/acme/widgets/pulls/404")
		return jsonResponse(http.StatusNotFound, `{"message":"not found"}`), nil
	})
	rr := httptest.NewRecorder()
	c.PRHandler().ServeHTTP(rr, httptest.NewRequest(http.MethodGet, "/pr?repo=widgets&number=404", nil))
	if rr.Code != http.StatusNotFound {
		t.Fatalf("status = %d, want 404 for ErrNotFound; body %q", rr.Code, rr.Body.String())
	}
}

func newPRRouteNoTransportClient(t *testing.T) *Client {
	t.Helper()
	return newRESTTestClient(t, func(req *http.Request) (*http.Response, error) {
		t.Fatalf("unexpected GitHub request for %s %s", req.Method, req.URL.String())
		return nil, context.Canceled
	})
}
