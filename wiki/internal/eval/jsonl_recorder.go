package eval

import (
	"context"
	"encoding/json"
	"fmt"
	"io"
	"sync"

	"wiki/internal/llm"
)

// NewJSONLRecorder returns an LLM recorder that writes one JSON object per call.
func NewJSONLRecorder(w io.Writer) llm.Recorder {
	return &jsonlRecorder{w: w}
}

type jsonlRecorder struct {
	mu sync.Mutex
	w  io.Writer
}

func (r *jsonlRecorder) Record(_ context.Context, rec llm.CallRecord) error {
	if r == nil || r.w == nil {
		return nil
	}
	raw, err := json.Marshal(rec)
	if err != nil {
		return fmt.Errorf("eval jsonl recorder: marshal call record: %w", err)
	}
	r.mu.Lock()
	defer r.mu.Unlock()
	if _, err := r.w.Write(append(raw, '\n')); err != nil {
		return fmt.Errorf("eval jsonl recorder: write call record: %w", err)
	}
	return nil
}
