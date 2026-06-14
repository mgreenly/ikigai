package integrate

import "encoding/json"

// CompileSchema is the structured-output JSON schema the compile call constrains
// the model to (design §5). Compile targets EXTRACT'S output schema directly —
// subjects with typed/kinded names, aliases, claims, occurred_at — with TWO §5
// deltas on the claim shape: each claim carries an explicit per-claim "cites"
// array (the inbox ids of the events it rests on — events are presented with
// their ids visible, design §5), and occurred_at is resolved from event payloads.
// There is NO prose-digest artifact; compile emits the same subjects[] envelope
// extract does, entering the shared pipeline at resolve.
//
// ParseCompile still validates semantically (closed-set type, non-empty name,
// claim-bearing, every claim cited) on top of the schema's shape check.
var CompileSchema = json.RawMessage(`{
  "type": "object",
  "additionalProperties": false,
  "required": ["subjects"],
  "properties": {
    "subjects": {
      "type": "array",
      "items": {
        "type": "object",
        "additionalProperties": false,
        "required": ["type", "kind", "name", "aliases", "claims"],
        "properties": {
          "type": {"type": "string", "enum": ["entity", "event", "concept"]},
          "kind": {"type": "string"},
          "name": {"type": "string"},
          "aliases": {"type": "array", "items": {"type": "string"}},
          "claims": {
            "type": "array",
            "items": {
              "type": "object",
              "additionalProperties": false,
              "required": ["text", "cites"],
              "properties": {
                "text": {"type": "string"},
                "cites": {"type": "array", "items": {"type": "string"}}
              }
            }
          },
          "occurred_at": {"type": "string"}
        }
      }
    }
  }
}`)
