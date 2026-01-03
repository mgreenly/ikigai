# DuckDuckGo Search

Technical specification for DuckDuckGo search access methods.

**Status**: Not recommended. See `scraping-comparison.md` - Bing is simpler for scraping.

## Overview

DuckDuckGo provides no official search API. Available access methods:

1. **Instant Answer API** - Knowledge graph results only (not full search)
2. **HTML Scraping** - Full search results via static HTML endpoint (unofficial)

## Instant Answer API

### Endpoint

```
https://api.duckduckgo.com/
```

### Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `q` | string | required | Search query |
| `format` | string | `xml` | Response format: `json` or `xml` |
| `no_html` | int | `0` | Strip HTML from responses (0 or 1) |
| `skip_disambig` | int | `0` | Skip disambiguation results (0 or 1) |
| `no_redirect` | int | `1` | Skip HTTP redirects (0 or 1) |

### Response

Returns structured instant answers for well-known entities (celebrities, places, definitions). **Does not return full search results.**

### Limitations

- No full search results - only knowledge graph
- Limited coverage - many queries return nothing
- No pagination
- No authentication required

### Example

```bash
curl "https://api.duckduckgo.com/?q=python&format=json&no_html=1"
```

## HTML Scraping (Unofficial)

### Endpoint

```
https://html.duckduckgo.com/html/
```

Static HTML version - server-rendered without JavaScript.

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `q` | string | Search query (URL encoded) |
| `kl` | string | Region code: `us-en`, `wt-wt` (no region), etc. |
| `kp` | int | Safe search: `-1` (off), `-2` (moderate), `1` (on) |
| `s` | int | Result offset for pagination (0, 30, 60, ...) |

### HTML Structure

- Container: element with ID `links`
- Results: elements with class `result`
- Fields: title (link text), URL (href), snippet (body text)

### Pagination

- Default: ~30 results per page
- Use `s` parameter for offset: `&s=0`, `&s=30`, `&s=60`

### Rate Limiting

| Constraint | Value |
|------------|-------|
| Reported limit | ~20 requests/second |
| Recommended | < 1 request/second with pauses |

### Anti-Bot Measures

- VQD value requirement (session token)
- CAPTCHA challenges
- IP-based rate limiting
- Success rate: ~61% from datacenter IPs, ~94% residential

### Alternative Backends

From `duckduckgo-search` Python library:

| Backend | Endpoint | Notes |
|---------|----------|-------|
| `html` | `html.duckduckgo.com` | Static HTML |
| `lite` | `lite.duckduckgo.com` | Lightweight version |
| `bing` | `www.bing.com` | Falls back to Bing |

## Comparison with Bing

| Criterion | DuckDuckGo | Bing |
|-----------|------------|------|
| Anti-bot | Aggressive | Lenient |
| VQD required | Yes | No |
| CAPTCHA | Common | Rare |
| Success rate | 61% datacenter | High |
| HTML structure | Simple | Cleanest |

**Recommendation**: Use Bing for scraping, not DuckDuckGo.

## Legal Considerations

DuckDuckGo's ToS does not explicitly permit or prohibit scraping. Third-party libraries include disclaimer: "for educational purposes only."

## Sources

- [DuckDuckGo Search Engine Results API - SerpApi](https://serpapi.com/duckduckgo-search-api)
- [duckduckgo-search PyPI](https://pypi.org/project/duckduckgo-search/)
- [How to Scrape DuckDuckGo - Bright Data](https://brightdata.com/blog/web-data/how-to-scrape-duckduckgo)
- [DuckDuckGo Instant Answer API - Postman](https://www.postman.com/api-evangelist/search/documentation/bdkqiym/duckduckgo-instant-answer-api)
