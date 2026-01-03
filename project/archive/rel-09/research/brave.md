# Brave Search API

Technical specification for Brave Search API based on official documentation.

**Official Documentation**: https://brave.com/search/api/
**API Dashboard**: https://api-dashboard.search.brave.com/

## Overview

Brave Search API is the official REST API for Brave Search. It provides web search results with proper authentication, rate limiting, and structured JSON responses.

## Authentication

### API Key Registration

1. Register at: https://api-dashboard.search.brave.com/register
2. Subscribe to Free AI tier (no payment required)
3. Get API key from dashboard

### Request Authentication

Include API key in header:

```http
X-Subscription-Token: <API_KEY>
```

## Endpoints

### Web Search (Primary)

```
GET https://api.search.brave.com/res/v1/web/search
```

### Other Endpoints

```
GET https://api.search.brave.com/res/v1/images/search      # Image search
GET https://api.search.brave.com/res/v1/videos/search      # Video search
GET https://api.search.brave.com/res/v1/news/search        # News search
GET https://api.search.brave.com/res/v1/suggest/search     # Autocomplete
GET https://api.search.brave.com/res/v1/spellcheck/search  # Spell check
```

## Request Parameters

### Required

| Parameter | Type | Description |
|-----------|------|-------------|
| `q` | string | Search query (max 400 chars, 50 words) |

### Optional

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `country` | string | `"us"` | Country code (ISO 3166-1 alpha-2) |
| `search_lang` | string | `"en"` | Search language code |
| `ui_lang` | string | `"en-US"` | UI language (BCP 47) |
| `count` | int | `20` | Number of results (1-20) |
| `offset` | int | `0` | Result offset for pagination |
| `safesearch` | string | `"moderate"` | `"off"`, `"moderate"`, `"strict"` |
| `freshness` | string | - | `"pd"` (day), `"pw"` (week), `"pm"` (month), `"py"` (year) |
| `text_decorations` | bool | `true` | Enable text decorations in results |
| `spellcheck` | bool | `true` | Enable spell check |
| `result_filter` | string | - | Filter: `"web"`, `"news"`, `"videos"`, etc. |
| `goggles` | string | - | Apply Goggles (custom reranking) |
| `units` | string | `"metric"` | `"metric"` or `"imperial"` |
| `extra_snippets` | bool | `false` | Include additional snippets |
| `summary` | bool | `false` | Include AI-generated summary |

### Query Constraints

- Cannot be empty
- Max 400 characters
- Max 50 words
- Violating these returns HTTP 422

### Example Request

```bash
curl -s --compressed \
  "https://api.search.brave.com/res/v1/web/search?q=linux+terminal&count=5" \
  -H "Accept: application/json" \
  -H "Accept-Encoding: gzip" \
  -H "X-Subscription-Token: BSA_API_KEY_HERE"
```

## Response Format

### Top-Level Structure

```json
{
  "type": "search",
  "query": { ... },
  "web": { ... },
  "mixed": { ... },
  "news": { ... },
  "videos": { ... },
  "locations": { ... }
}
```

### Web Results Object

```json
{
  "web": {
    "type": "search",
    "results": [ ... ],
    "family_friendly": true
  }
}
```

### Individual Result

```json
{
  "title": "Linux Terminal Tutorial",
  "url": "https://example.com/linux-terminal",
  "description": "Learn how to use the Linux terminal...",
  "page_age": "2024-01-15T00:00:00.000Z",
  "page_fetched": "2024-06-01T12:00:00.000Z",
  "language": "en",
  "family_friendly": true,
  "is_source_local": false,
  "is_source_both": false,
  "profile": { ... },
  "thumbnail": { ... },
  "deep_results": { ... },
  "meta_url": { ... }
}
```

### Query Metadata

```json
{
  "query": {
    "original": "linux terminal",
    "altered": null,
    "country": "us",
    "show_strict_warning": false,
    "is_navigational": false,
    "is_news_breaking": false,
    "spellcheck_off": false,
    "bad_results": false,
    "should_fallback": false,
    "more_results_available": true
  }
}
```

## Rate Limits

### Free AI Tier

| Limit Type | Value |
|------------|-------|
| Per-second | 1 request/sec |
| Per-month | 2,000 requests/month |
| Cost | $0 (free) |

### Paid Tiers

| Tier | Per-second | Per-month | Cost |
|------|------------|-----------|------|
| Base AI | 20 req/sec | 20M | $5 per 1,000 |
| Pro AI | 50 req/sec | Unlimited | $9 per 1,000 |

### Rate Limit Headers

Response headers include:

```http
X-RateLimit-Limit: 1, 2000
X-RateLimit-Policy: 1;w=1, 2000;w=2592000
X-RateLimit-Remaining: 1, 1543
X-RateLimit-Reset: 1, 1419704
```

**Header format:**
- `X-RateLimit-Limit`: `<per_sec>, <per_month>`
- `X-RateLimit-Policy`: `<limit>;w=<window_seconds>, ...`
- `X-RateLimit-Remaining`: `<remaining_sec>, <remaining_month>`
- `X-RateLimit-Reset`: `<reset_sec_seconds>, <reset_month_seconds>`

Only successful requests (HTTP 200) count against quota.

## Error Handling

### HTTP Status Codes

| Code | Meaning | Cause |
|------|---------|-------|
| 200 | Success | Request processed |
| 400 | Bad Request | Invalid parameters |
| 401 | Unauthorized | Missing/invalid API key |
| 403 | Forbidden | Account issue |
| 404 | Not Found | Invalid endpoint |
| 422 | Unprocessable Entity | Query validation failed |
| 429 | Too Many Requests | Rate limit exceeded |
| 500 | Internal Server Error | Brave server error |
| 503 | Service Unavailable | Brave service down |

### Error Response Format

```json
{
  "error": {
    "code": 422,
    "message": "Query too long (max 400 characters)"
  }
}
```

## Pagination

Use `offset` parameter:

```
# First page (results 0-19)
count=20&offset=0

# Second page (results 20-39)
count=20&offset=20

# Third page (results 40-59)
count=20&offset=40
```

Check `query.more_results_available` to detect if more pages exist.

## Sources

- [Brave Search API | Brave](https://brave.com/search/api/)
- [Brave Search - API Documentation](https://api-dashboard.search.brave.com/app/documentation)
- [Brave Search API Guides](https://brave.com/search/api/guides/)
- [Brave Search API Response Headers](https://api-dashboard.search.brave.com/app/documentation/web-search/response-headers)
- [Introducing AI Grounding with Brave Search API](https://brave.com/blog/ai-grounding/)
