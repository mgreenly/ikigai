# Backlog Candidates

Items under consideration for future implementation.

---

## Google Provider Enhancements

### Google Search Grounding

Add support for the `google_search` tool that connects Gemini to real-time web data. Reduces hallucinations by grounding responses in current information and provides cited sources. Enabled via `tools: [{"google_search": {}}]`.

### URL Context Tool

Add support for the `url_context` tool that fetches and analyzes content from user-provided URLs. Supports web pages, PDFs, and images. Uses Google's index with live-fetch fallback for fresh content. Reduces token costs by referencing URLs instead of inlining content.

### Google Maps Grounding

Add support for the `google_maps` tool for location-aware responses. Provides access to 250M+ places with addresses, hours, ratings, and reviews. Returns context tokens for rendering interactive map widgets. Can combine with Google Search for richer location queries.

### Code Execution Tool

Add support for the `code_execution` tool that lets Gemini write and run Python code during responses. Model generates code, executes in Google's sandbox, uses output to continue reasoning, and can iterate. Eliminates arithmetic errors and grounds computational answers in actual execution. Supports standard library plus numpy/pandas. Can combine with Google Search in the same request.
