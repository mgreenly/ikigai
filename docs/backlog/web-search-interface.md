# Web Search Interface

## Summary

Abstract web search interface with multiple provider implementations, prioritizing zero-setup defaults.

## Motivation

- Web search is essential for AI agents to access current information
- Different users have different needs (free vs quality vs existing accounts)
- Abstract interface allows swapping providers without changing agent code

## Design

### Provider Priority

| Priority | Provider | Setup | Cost | Use Case |
|----------|----------|-------|------|----------|
| 1 (Default) | DuckDuckGo | None | Free | Zero-friction default |
| 2 | Brave | API key | Free tier | Privacy-focused, LLM-friendly |
| 3 | Tavily | API key | 1000 free/month | AI-optimized results |
| 4 | Google | API key + billing | Pay per use | Maximum coverage |

### Interface

```c
typedef struct {
    char *title;
    char *url;
    char *snippet;
    double score;        // relevance score if available
} ik_search_result_t;

typedef struct {
    ik_search_result_t *results;
    size_t count;
    char *answer;        // AI-generated summary (Tavily only)
    double response_time;
} ik_search_response_t;

// Abstract search function - dispatches to configured provider
res_t ik_web_search(void *parent, const char *query, int max_results);
```

### Configuration

```json
{
  "web_search": {
    "provider": "duckduckgo",
    "max_results": 5
  }
}
```

Provider-specific keys in credentials.json:
```json
{
  "brave_api_key": "...",
  "tavily_api_key": "...",
  "google_api_key": "...",
  "google_cx": "..."
}
```

### Provider Details

**DuckDuckGo (default)**
- No API key required
- HTML scraping or unofficial API
- Rate limiting considerations

**Brave Search API**
- `GET https://api.search.brave.com/res/v1/web/search`
- Header: `X-Subscription-Token: <key>`
- Free tier: 2000 queries/month

**Tavily**
- `POST https://api.tavily.com/search`
- Header: `Authorization: Bearer <key>`
- Free tier: 1000 queries/month
- Returns AI-generated answer alongside results

**Google Custom Search**
- `GET https://www.googleapis.com/customsearch/v1`
- Params: `key=<key>&cx=<cx>&q=<query>`
- 100 free/day, then $5/1000 queries

### Fallback Behavior

If configured provider fails:
1. Return error with provider name
2. Do NOT silently fall back to another provider
3. User must explicitly configure fallback chain if desired

## Rationale

- **DuckDuckGo default**: Only provider requiring zero setup
- **Four providers**: Covers free/paid spectrum and existing Google users
- **No silent fallback**: Explicit configuration prevents unexpected behavior/costs
- **Tavily inclusion**: Purpose-built for AI agents, returns structured summaries
