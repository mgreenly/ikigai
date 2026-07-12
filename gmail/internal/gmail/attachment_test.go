package gmail

import (
	"context"
	"errors"
	"net/http"
	"net/http/httptest"
	"net/url"
	"strconv"
	"testing"
)

type fakeAttachmentSource struct {
	message                       Message
	messageErr                    error
	attachment                    []byte
	attachmentErr                 error
	messageFn                     func(int) (Message, error)
	messageCalls, attachmentCalls int
	lastAttachmentID              string
}

func (f *fakeAttachmentSource) MessageGet(_ context.Context, _, _ string) (Message, error) {
	f.messageCalls++
	if f.messageFn != nil {
		return f.messageFn(f.messageCalls)
	}
	return f.message, f.messageErr
}

func (f *fakeAttachmentSource) AttachmentGet(_ context.Context, _, attachmentID string) ([]byte, error) {
	f.attachmentCalls++
	f.lastAttachmentID = attachmentID
	return f.attachment, f.attachmentErr
}

func attachmentRequest(messageID, partID string) *http.Request {
	q := url.Values{"message_id": {messageID}, "part_id": {partID}}
	return httptest.NewRequest(http.MethodGet, "/attachment?"+q.Encode(), nil)
}

func TestAttachmentHandlerIdentityGuard(t *testing.T) {
	// R-WX7D-ZS9N
	src := &fakeAttachmentSource{message: Message{Payload: MessagePart{PartID: "1", MimeType: "application/pdf", Body: Body{AttachmentID: "a"}}}, attachment: []byte("pdf")}
	h := AttachmentHandler(src)
	for header, value := range map[string]string{"X-Owner-Email": "a@b.c", "X-Forwarded-Proto": "https"} {
		r := attachmentRequest("m", "1")
		r.Header.Set(header, value)
		rec := httptest.NewRecorder()
		h.ServeHTTP(rec, r)
		if rec.Code != http.StatusNotFound {
			t.Fatalf("%s guard status = %d, want 404", header, rec.Code)
		}
	}
}

func TestAttachmentHandlerResolvesFreshTokenByPartID(t *testing.T) {
	// R-3G57-009Q
	bytes := []byte("%PDF-1.7\n")
	src := &fakeAttachmentSource{attachment: bytes, messageFn: func(n int) (Message, error) {
		return Message{Payload: MessagePart{Parts: []MessagePart{{PartID: "2", MimeType: "application/pdf", Body: Body{AttachmentID: "minted-token-" + strconv.Itoa(n)}}}}}, nil
	}}
	rec := httptest.NewRecorder()
	AttachmentHandler(src).ServeHTTP(rec, attachmentRequest("m", "2"))
	if rec.Code != http.StatusOK || rec.Body.String() != string(bytes) {
		t.Fatalf("response = %d %q, want 200 attachment bytes", rec.Code, rec.Body.String())
	}
	if src.lastAttachmentID != "minted-token-1" {
		t.Fatalf("AttachmentGet token = %q, want fresh minted token", src.lastAttachmentID)
	}
	rec = httptest.NewRecorder()
	AttachmentHandler(src).ServeHTTP(rec, attachmentRequest("m", "2"))
	if src.lastAttachmentID != "minted-token-2" {
		t.Fatalf("second AttachmentGet token = %q, want newly minted token", src.lastAttachmentID)
	}
	if got := rec.Header().Get("Content-Type"); got != "application/pdf" {
		t.Fatalf("Content-Type = %q", got)
	}
	if got := rec.Header().Get("Content-Length"); got != strconv.Itoa(len(bytes)) {
		t.Fatalf("Content-Length = %q", got)
	}
}

func TestAttachmentHandlerMissingPartIDDoesNotCallSource(t *testing.T) {
	// R-3HD3-DS0F
	for _, target := range []string{"/attachment", "/attachment?part_id=2", "/attachment?message_id=m", "/attachment?message_id=&part_id=2", "/attachment?message_id=m&part_id=", "/attachment?message_id=m&attachment_id=legacy"} {
		src := &fakeAttachmentSource{}
		rec := httptest.NewRecorder()
		AttachmentHandler(src).ServeHTTP(rec, httptest.NewRequest(http.MethodGet, target, nil))
		if rec.Code != http.StatusNotFound || src.messageCalls != 0 || src.attachmentCalls != 0 {
			t.Fatalf("%s = status %d, calls %d/%d; want 404 and no calls", target, rec.Code, src.messageCalls, src.attachmentCalls)
		}
	}
}

func TestAttachmentHandlerMapsAbsenceAndUpstreamFailures(t *testing.T) {
	// R-3IKZ-RJR4
	for name, src := range map[string]*fakeAttachmentSource{
		"message missing": {messageErr: ErrNotFound},
		"part missing":    {message: Message{}},
		"inline part":     {message: Message{Payload: MessagePart{PartID: "2"}}},
	} {
		t.Run(name, func(t *testing.T) {
			rec := httptest.NewRecorder()
			AttachmentHandler(src).ServeHTTP(rec, attachmentRequest("m", "2"))
			if rec.Code != http.StatusNotFound {
				t.Fatalf("status = %d, want 404", rec.Code)
			}
			if name == "inline part" && src.attachmentCalls != 0 {
				t.Fatalf("inline part fetched attachment")
			}
		})
	}
	rec := httptest.NewRecorder()
	AttachmentHandler(&fakeAttachmentSource{messageErr: errors.New("upstream 503")}).ServeHTTP(rec, attachmentRequest("m", "2"))
	if rec.Code != http.StatusBadGateway {
		t.Fatalf("upstream failure status = %d, want 502", rec.Code)
	}
}
