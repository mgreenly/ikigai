# Google Custom Search

## Description

User with Google Custom Search credentials configured asks a question requiring current information. This demonstrates using Google as an alternative search provider. Unlike Brave (which requires only an API key), Google requires both an API key and a Search Engine ID (CX).

## Transcript

```text
> What are the latest Rust language features in 2026?

tool_call: web_search_google({"query": "Rust language features 2026", "num": 10})

tool_result: {"success": true, "results": [{"title": "Rust 1.85 Release Notes", "url": "https://blog.rust-lang.org/2026/01/15/Rust-1.85.0.html", "snippet": "Rust 1.85 introduces lifetime refinement, allowing more precise lifetime bounds in generic code."}, {"title": "Rust 2026 Edition Planning", "url": "https://rust-lang.github.io/rfcs/3501-edition-2026.html", "snippet": "The 2026 edition will include improvements to async trait syntax and enhanced pattern matching capabilities."}, {"title": "Type System Improvements in Rust", "url": "https://without.boats/blog/rust-types-2026/", "snippet": "New trait solver enables better type inference and more expressive generic bounds."}], "count": 3}

Based on the latest developments, Rust in 2026 has several significant new features:

1. **Lifetime Refinement** (Rust 1.85) - Allows more precise lifetime bounds in generic code, improving ergonomics
2. **Async Trait Syntax** (2026 Edition) - Enhanced syntax for async traits making async Rust more ergonomic
3. **Enhanced Pattern Matching** - More powerful pattern matching capabilities in the 2026 edition
4. **New Trait Solver** - Enables better type inference and more expressive generic bounds

These improvements focus on making Rust's advanced features more approachable while maintaining zero-cost abstractions.
```

## Walkthrough

1. User asks question requiring current information
2. LLM calls `web_search_google` tool with query (see [Request](#request))
3. Tool checks credentials, finds both `web_search.google.api_key` and `web_search.google.engine_id` present
4. Tool makes HTTP request to Google Custom Search API
5. Tool receives search results from API
6. Tool returns normalized response with multiple results (see [Response](#response))
7. Scrollback renders tool_call in dim gray
8. Scrollback renders tool_result in dim gray
9. LLM receives results and synthesizes answer based on snippets
10. User sees both the raw search results and the LLM's interpretation

## Reference

### Request

Tool call arguments:

```json
{
  "query": "Rust language features 2026",
  "num": 10
}
```

### Response

Tool result:

```json
{
  "success": true,
  "results": [
    {
      "title": "Rust 1.85 Release Notes",
      "url": "https://blog.rust-lang.org/2026/01/15/Rust-1.85.0.html",
      "snippet": "Rust 1.85 introduces lifetime refinement, allowing more precise lifetime bounds in generic code."
    },
    {
      "title": "Rust 2026 Edition Planning",
      "url": "https://rust-lang.github.io/rfcs/3501-edition-2026.html",
      "snippet": "The 2026 edition will include improvements to async trait syntax and enhanced pattern matching capabilities."
    },
    {
      "title": "Type System Improvements in Rust",
      "url": "https://without.boats/blog/rust-types-2026/",
      "snippet": "New trait solver enables better type inference and more expressive generic bounds."
    }
  ],
  "count": 3
}
```

### Credential Configuration

Google Custom Search requires two credentials in `~/.config/ikigai/credentials.json`:

```json
{
  "web_search": {
    "google": {
      "api_key": "AIzaSyDdI0hCZtE6vySjMm-WEfRq3CPzqKqGg9g",
      "engine_id": "a12b3c4d5e6f7g8h9"
    }
  }
}
```

**Getting credentials:**
1. **API Key**: https://developers.google.com/custom-search/v1/overview
2. **Search Engine ID (CX)**: https://programmablesearchengine.google.com/controlpanel/create
   - Create new search engine
   - Enable "Search the entire web"
   - Copy the Search Engine ID from Overview page

### Notes

- Google requires **two credentials** (api_key + engine_id) vs Brave's single API key
- Free tier: 100 searches/day (~3,000/month)
- Parameter naming differs: Google uses `num` instead of Brave's `count`
- Both providers return normalized JSON format (title, url, snippet)
- Users can configure both providers and choose based on preference
