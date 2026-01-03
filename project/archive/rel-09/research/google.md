# Google Custom Search API

Technical specification for Google Custom Search JSON API based on official documentation.

**Official Documentation**: https://developers.google.com/custom-search/v1/overview
**API Reference**: https://developers.google.com/custom-search/v1/reference/rest/v1/cse/list
**Search Engine Control Panel**: https://programmablesearchengine.google.com/controlpanel/create

## Overview

Google Custom Search JSON API (Programmable Search Engine API) provides programmatic access to Google search results. It requires both an API key and a Custom Search Engine ID (CX).

## Authentication

### Two Credentials Required

1. **API Key** - Identifies application to Google
2. **Search Engine ID (CX)** - Identifies which Programmable Search Engine to use

### Getting API Key

1. Go to [Custom Search JSON API Introduction](https://developers.google.com/custom-search/v1/introduction)
2. Click "Get a Key"
3. Select existing project or create new one
4. Copy the generated API Key (format: `AIzaSy...`)

### Getting Search Engine ID (CX)

1. Go to [Programmable Search Engine Control Panel](https://programmablesearchengine.google.com/controlpanel/create)
2. Click "Add" to create new search engine
3. Select "Search the entire web" for full web search
4. Complete reCAPTCHA and click "Create"
5. Find **Search Engine ID** on Overview page under "Basic" section (format: `fc1234567890123456789`)

**Important**: Enable "Search the entire web" in Search Features settings for full web search results.

## Endpoint

```
GET https://customsearch.googleapis.com/customsearch/v1
```

Alternative:
```
GET https://www.googleapis.com/customsearch/v1
```

URL length limit: 2048 characters.

## Request Parameters

### Required

| Parameter | Type | Description |
|-----------|------|-------------|
| `key` | string | API key |
| `cx` | string | Search Engine ID |
| `q` | string | Search query |

### Common Optional

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `num` | integer | `10` | Results per page (1-10) |
| `start` | integer | `1` | Result index offset (1-100) |
| `safe` | string | - | Safe search: `"active"` or `"off"` |
| `lr` | string | - | Language restriction (e.g., `"lang_en"`) |
| `gl` | string | - | Geolocation boost (e.g., `"us"`) |
| `dateRestrict` | string | - | Date filter: `"d[N]"`, `"w[N]"`, `"m[N]"`, `"y[N]"` |
| `sort` | string | - | Sort expression (e.g., `"date"`) |
| `filter` | string | `"1"` | Duplicate filter: `"0"` (off) or `"1"` (on) |

### Advanced Optional

| Parameter | Type | Description |
|-----------|------|-------------|
| `exactTerms` | string | Phrase that must appear in all results |
| `excludeTerms` | string | Terms to exclude from results |
| `orTerms` | string | Additional terms (OR logic) |
| `hq` | string | Terms to append (AND logic) |
| `siteSearch` | string | Restrict to/exclude specific domain |
| `siteSearchFilter` | string | `"i"` (include) or `"e"` (exclude) |
| `fileType` | string | Restrict to file extension (e.g., `"pdf"`) |
| `rights` | string | Licensing filter (Creative Commons) |

### Image Search

| Parameter | Type | Description |
|-----------|------|-------------|
| `searchType` | string | `"image"` for image search |
| `imgSize` | enum | `huge`, `icon`, `large`, `medium`, `small`, `xlarge`, `xxlarge` |
| `imgType` | enum | `clipart`, `face`, `lineart`, `stock`, `photo`, `animated` |
| `imgColorType` | enum | `color`, `gray`, `mono`, `trans` |
| `imgDominantColor` | enum | `black`, `blue`, `brown`, `gray`, `green`, `orange`, `pink`, `purple`, `red`, `teal`, `white`, `yellow` |

### Constraints

- `num`: Maximum 10 results per request
- `start`: Maximum 91 (to get results 91-100)
- Total results limited to 100 per query
- All parameter values must be URL-encoded

### Example Request

```bash
curl "https://customsearch.googleapis.com/customsearch/v1?key=YOUR_API_KEY&cx=YOUR_CX_ID&q=linux+terminal&num=5"
```

## Response Format

### Top-Level Structure

```json
{
  "kind": "customsearch#search",
  "url": { ... },
  "queries": { ... },
  "context": { ... },
  "searchInformation": { ... },
  "items": [ ... ]
}
```

### Search Information

```json
{
  "searchInformation": {
    "searchTime": 0.234,
    "formattedSearchTime": "0.23",
    "totalResults": "1250000",
    "formattedTotalResults": "1,250,000"
  }
}
```

### Items Array (Results)

```json
{
  "items": [
    {
      "kind": "customsearch#result",
      "title": "Linux Terminal Tutorial",
      "htmlTitle": "<b>Linux</b> <b>Terminal</b> Tutorial",
      "link": "https://example.com/linux-terminal",
      "displayLink": "example.com",
      "snippet": "Learn how to use the Linux terminal with this guide...",
      "htmlSnippet": "Learn how to use the <b>Linux</b> <b>terminal</b>...",
      "cacheId": "abc123xyz",
      "formattedUrl": "https://example.com/linux-terminal",
      "htmlFormattedUrl": "https://example.com/<b>linux</b>-<b>terminal</b>",
      "pagemap": { ... }
    }
  ]
}
```

### Result Fields

| Field | Type | Description |
|-------|------|-------------|
| `kind` | string | Always `"customsearch#result"` |
| `title` | string | Page title (plain text) |
| `htmlTitle` | string | Title with `<b>` tags for matches |
| `link` | string | Full URL |
| `displayLink` | string | Abbreviated domain |
| `snippet` | string | Text snippet (plain text) |
| `htmlSnippet` | string | Snippet with `<b>` tags |
| `cacheId` | string | Google cache identifier |
| `formattedUrl` | string | Display URL |
| `htmlFormattedUrl` | string | Display URL with highlighting |
| `pagemap` | object | Structured data (metatags, etc.) |

### Pagination (queries object)

```json
{
  "queries": {
    "request": [{
      "totalResults": "1250000",
      "searchTerms": "linux terminal",
      "count": 10,
      "startIndex": 1
    }],
    "nextPage": [{
      "totalResults": "1250000",
      "searchTerms": "linux terminal",
      "count": 10,
      "startIndex": 11
    }]
  }
}
```

## Rate Limits

### Free Tier

| Limit | Value |
|-------|-------|
| Per-day | 100 queries |
| Per-month | ~3,000 queries |
| Cost | $0 (free) |

### Paid Tier

| Limit | Value |
|-------|-------|
| Per-day maximum | 10,000 queries |
| Cost | $5 per 1,000 queries |

Quota resets daily at midnight Pacific Time.

No rate limit headers returned in response.

## Error Handling

### HTTP Status Codes

| Code | Meaning | Cause |
|------|---------|-------|
| 200 | Success | Request processed |
| 400 | Bad Request | Invalid parameters |
| 401 | Unauthorized | Missing or invalid API key |
| 403 | Forbidden | API not enabled, billing issue, or quota exceeded |
| 429 | Too Many Requests | Rate limit exceeded |
| 500 | Internal Server Error | Google server error |
| 503 | Service Unavailable | Service temporarily down |

### Error Response Format

```json
{
  "error": {
    "code": 403,
    "message": "The request is missing a valid API key.",
    "errors": [
      {
        "message": "The request is missing a valid API key.",
        "domain": "global",
        "reason": "forbidden"
      }
    ],
    "status": "PERMISSION_DENIED"
  }
}
```

### Common Error Reasons

| Reason | HTTP Code | Meaning |
|--------|-----------|---------|
| `dailyLimitExceeded` | 403 | 100/day free quota exhausted |
| `userRateLimitExceeded` | 429 | Too many requests too quickly |
| `quotaExceeded` | 403 | General quota exceeded |
| `keyInvalid` | 400 | Invalid API key format |
| `accessNotConfigured` | 403 | Custom Search API not enabled |

## Pagination

Use `start` parameter:

```
# First page (results 1-10)
num=10&start=1

# Second page (results 11-20)
num=10&start=11

# Third page (results 21-30)
num=10&start=21
```

Maximum: `start=91` with `num=10` (results 91-100).

Check `queries.nextPage` to detect if more pages exist.

## Limitations

- Maximum 100 results per query (across all pages)
- Maximum 10 results per request
- Results are a subset of full Google index
- Does not include Google.com features (Oneboxes, real-time results, etc.)

## Sources

- [Custom Search JSON API: Introduction | Google for Developers](https://developers.google.com/custom-search/v1/introduction)
- [Custom Search JSON API Overview | Google for Developers](https://developers.google.com/custom-search/v1/overview)
- [Custom Search JSON API Reference | Google for Developers](https://developers.google.com/custom-search/v1/reference/rest/v1/cse/list)
- [Use REST to Invoke the API | Google for Developers](https://developers.google.com/custom-search/v1/using_rest)
- [Search Response Object | Google for Developers](https://developers.google.com/custom-search/v1/reference/rest/v1/Search)
- [Programmable Search Engine ID | Google Support](https://support.google.com/programmable-search/answer/12499034?hl=en)
- [Programmable Search Engine vs Google.com | Google Support](https://support.google.com/programmable-search/answer/70392?hl=en)
