# Brave vs Google Search APIs - Comparison

Comparison of the two search API providers selected for rel-09.

## Selected Providers

**Brave Search API** and **Google Custom Search API** were selected as the two search providers for rel-09. Both offer:
- Official, well-documented APIs
- Generous free tiers for typical usage
- High reliability
- Full web search results

Users can choose their preferred provider based on personal preference.

## Side-by-Side Comparison

| Feature | Brave Search API | Google Custom Search API |
|---------|------------------|--------------------------|
| **Free Tier** | 2,000 requests/month | 100 requests/day (~3,000/month) |
| **Rate Limit** | 1 request/second | Not specified |
| **Authentication** | API key only | API key + Search Engine ID (CX) |
| **Setup Complexity** | Low (single API key) | Medium (requires CSE creation) |
| **Response Format** | JSON | JSON |
| **Pagination** | `offset` parameter | `start` parameter |
| **Max Results/Request** | 20 | 10 |
| **Total Results Limit** | Unlimited | 100 per query |
| **Documentation Quality** | Excellent | Excellent |
| **Reliability** | High | High |

## Brave Search API

**Strengths:**
- Simpler authentication (API key only)
- Higher results per request (20 vs 10)
- No hard limit on total results per query
- Privacy-focused provider

**Setup:**
1. Register at api-dashboard.search.brave.com
2. Subscribe to Free AI tier
3. Copy API key

**Endpoint:**
```
GET https://api.search.brave.com/res/v1/web/search
```

**Authentication:**
```http
X-Subscription-Token: <API_KEY>
```

## Google Custom Search API

**Strengths:**
- Slightly larger free tier (100/day vs ~67/day)
- Google's search quality and index
- Well-established, mature API

**Setup:**
1. Create Google Cloud project
2. Enable Custom Search API
3. Get API key
4. Create Programmable Search Engine
5. Get Search Engine ID (CX)
6. Enable "Search the entire web" in settings

**Endpoint:**
```
GET https://customsearch.googleapis.com/customsearch/v1
```

**Authentication:**
```
?key=<API_KEY>&cx=<SEARCH_ENGINE_ID>
```

## Selection Rationale

Both providers were selected because:

1. **Official APIs** - No scraping, stable contracts
2. **Generous free tiers** - 2,000-3,000 requests/month covers typical usage
3. **High reliability** - Well-maintained, documented
4. **User choice** - Different users prefer different providers

Providing both gives users flexibility without adding implementation complexity (both follow the same external tool protocol).

## Sources

- [Brave Search API](https://brave.com/search/api/)
- [Brave Search API Documentation](https://api-dashboard.search.brave.com/app/documentation)
- [Google Custom Search JSON API](https://developers.google.com/custom-search/v1/overview)
- [Google Custom Search API Reference](https://developers.google.com/custom-search/v1/reference/rest/v1/cse/list)
