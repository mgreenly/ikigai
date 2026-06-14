package integrate

import "encoding/json"

// MergeSchema is the structured-output JSON schema merge's output is constrained
// to (design §4.4 / §6 / §6.1). It pins the rewritten pages (one per target
// subject — subject id + title + prose body + the §6.1 superseded list) and the
// stale-notes side channel merge surfaces while folding. ApplyMerge validates
// semantically on top of the shape check (write-set conformance: every output page
// is a manifest write-set page, and every write-set page is written; non-empty
// bodies).
var MergeSchema = json.RawMessage(`{
  "type": "object",
  "additionalProperties": false,
  "required": ["pages"],
  "properties": {
    "pages": {
      "type": "array",
      "items": {
        "type": "object",
        "additionalProperties": false,
        "required": ["subject", "title", "body"],
        "properties": {
          "subject": {"type": "string"},
          "title": {"type": "string"},
          "body": {"type": "string"},
          "superseded": {"type": "array", "items": {"type": "string"}}
        }
      }
    },
    "stale_notes": {
      "type": "array",
      "items": {
        "type": "object",
        "additionalProperties": false,
        "required": ["subject", "note"],
        "properties": {
          "subject": {"type": "string"},
          "note": {"type": "string"},
          "cites": {"type": "array", "items": {"type": "string"}}
        }
      }
    }
  }
}`)
