package gmail

import (
	"context"
	"errors"
	"net/http"
	"strconv"
)

// AttachmentSource is the portion of the Gmail client needed to serve an
// attachment without coupling handler tests to HTTP or OAuth.
type AttachmentSource interface {
	MessageGet(ctx context.Context, id, format string) (Message, error)
	AttachmentGet(ctx context.Context, messageID, attachmentID string) ([]byte, error)
}

// AttachmentHandler returns the loopback-only GET /attachment handler.
func AttachmentHandler(src AttachmentSource) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.Header.Get("X-Owner-Email") != "" || r.Header.Get("X-Forwarded-Proto") != "" {
			http.Error(w, "not found", http.StatusNotFound)
			return
		}

		messageID := r.URL.Query().Get("message_id")
		partID := r.URL.Query().Get("part_id")
		if messageID == "" || partID == "" {
			http.Error(w, "not found", http.StatusNotFound)
			return
		}

		message, err := src.MessageGet(r.Context(), messageID, "full")
		if err != nil {
			attachmentError(w, err)
			return
		}
		mimeType, attachmentID, ok := attachmentPart(message.Payload, partID)
		if !ok {
			http.Error(w, "not found", http.StatusNotFound)
			return
		}
		body, err := src.AttachmentGet(r.Context(), messageID, attachmentID)
		if err != nil {
			attachmentError(w, err)
			return
		}
		if mimeType == "" {
			mimeType = "application/octet-stream"
		}
		w.Header().Set("Content-Type", mimeType)
		w.Header().Set("Content-Length", strconv.Itoa(len(body)))
		_, _ = w.Write(body)
	})
}

func attachmentError(w http.ResponseWriter, err error) {
	if errors.Is(err, ErrNotFound) {
		http.Error(w, "not found", http.StatusNotFound)
		return
	}
	http.Error(w, "bad gateway", http.StatusBadGateway)
}

func attachmentPart(part MessagePart, partID string) (mimeType, attachmentID string, ok bool) {
	if part.PartID == partID {
		if part.Body.AttachmentID == "" {
			return "", "", false
		}
		return part.MimeType, part.Body.AttachmentID, true
	}
	for _, child := range part.Parts {
		if mimeType, attachmentID, ok := attachmentPart(child, partID); ok {
			return mimeType, attachmentID, true
		}
	}
	return "", "", false
}
