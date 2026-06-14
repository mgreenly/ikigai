package mcp

// toolDescriptors returns wiki's MCP tool surface (design §9 / the MCP product
// surface). P2 registers the full set as descriptors; the domain tools dispatch
// to a not-implemented stub until their owning phases land. The surface is:
//
//   - ingest_text / ingest_url — the write doors (P3).
//   - status                   — poll integration state by inbox id (P3).
//   - search / ask / timeline  — the read side (P10).
//   - health / reflection      — the two live cross-cutting tools (P2).
func toolDescriptors() []map[string]any {
	return []map[string]any{
		desc("ingest_text",
			"Ingest a block of text into the wiki. Returns a receipt (inbox id + sha256 + dup flag), not a job id; poll integration with the status verb. Fields: 'text' (required), optional 'title', 'source', 'tags' (array of strings).",
			obj(map[string]any{
				"text":   typ("string"),
				"title":  typ("string"),
				"source": typ("string"),
				"tags":   arr("string"),
			}, "text")),
		desc("ingest_url",
			"Fetch a URL, extract its text server-side, and ingest it. Returns the same receipt (inbox id + sha256 + dup flag) as ingest_text. Fields: 'url' (required), optional 'tags'.",
			obj(map[string]any{
				"url":  typ("string"),
				"tags": arr("string"),
			}, "url")),
		desc("status",
			"Poll the integration state of a previously-ingested item by its inbox id (the receipt's id). Returns the row's current state (pending|running|succeeded|crashed|dead) and any last error.",
			obj(map[string]any{"id": typ("string")}, "id")),
		desc("search",
			"Exact/known-item fetch over the wiki's pages (fast, keyword/FTS5, zero-LLM). Returns ranked WHOLE pages, registry-first (an exact name match pins rank 1). Use this when you already know which page you want; for any actual QUESTION use ask instead — do not assemble answers from search results yourself. Fields: 'query' (required), optional 'limit' (default 3, cap 10).",
			obj(map[string]any{
				"query": typ("string"),
				"limit": typ("integer"),
			}, "query")),
		desc("ask",
			"The default for any question: ask gets a cited answer synthesized over the wiki (a server-side RAG agent that retrieves and reasons). Answers come back cited (page subject id + title); do not assemble answers from search results yourself. Slower than search. Fields: 'question' (required).",
			obj(map[string]any{"question": typ("string")}, "question")),
		desc("timeline",
			"List event subjects in a date window (zero-LLM registry query). NOT a way to answer questions about a period — for that, use ask. Fields: optional 'from' and 'to' (ISO-8601 prefixes, e.g. '2024' or '2024-06'), optional 'limit'.",
			obj(map[string]any{
				"from":  typ("string"),
				"to":    typ("string"),
				"limit": typ("integer"),
			})),
		desc("lint_run",
			"Manually trigger a lint maintenance job (e.g. lint-dups) now, instead of waiting for its cron schedule. Accepts a trigger row the worker picks up; returns a receipt (inbox id), not a result — the job runs asynchronously. Field: 'job' (required — the lint job name).",
			obj(map[string]any{"job": typ("string")}, "job")),
		desc("health",
			"Health + diagnostics for the wiki service. Returns the fixed envelope (status, version, service, details) plus the authenticated caller's identity (owner_email, client_id). Takes no inputs.",
			obj(map[string]any{})),
		desc("reflection",
			"Self-describe wiki's edges in the event graph. With no arguments, returns the index {publishes:[{type,description}], subscribes:[{source,filter,description}]}. Pass 'event_type' (a published type) for its detail {type, description, schema, example}.",
			obj(map[string]any{
				"event_type": descTyp("string", "optional; a published event type to fetch the schema+example detail for"),
			})),
	}
}

func desc(name, description string, schema map[string]any) map[string]any {
	return map[string]any{"name": name, "description": description, "inputSchema": schema}
}

func obj(props map[string]any, required ...string) map[string]any {
	o := map[string]any{"type": "object", "properties": props}
	if len(required) > 0 {
		o["required"] = required
	}
	return o
}

func typ(t string) map[string]any { return map[string]any{"type": t} }

func arr(itemType string) map[string]any {
	return map[string]any{"type": "array", "items": map[string]any{"type": itemType}}
}

func descTyp(t, description string) map[string]any {
	return map[string]any{"type": t, "description": description}
}
