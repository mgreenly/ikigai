# rel-08 Research

Technical specifications and API documentation for web search providers.

## Research Documents

| Document | Description | Status |
|----------|-------------|--------|
| `all-providers-comparison.md` | Comparison of ALL search API providers | Complete |
| `brave.md` | Brave Search API specification | Complete |
| `google.md` | Google Custom Search API specification | Complete |
| `duckduckgo.md` | DuckDuckGo (scraping - not recommended) | Complete |
| `tavily.md` | Tavily API (researched, excluded) | Complete |
| `scraping-comparison.md` | Scraping analysis (Bing vs DDG) | Complete |
| `bing.md` | Bing scraping specification | Pending |
| `web-fetch.md` | Web content fetching | Pending |

## Key Findings

### No Zero-Auth Provider with Full Results

No provider offers a public API with full search results that requires zero authentication. The only "no-auth" option is DuckDuckGo's Instant Answer API, which returns knowledge graph results only, not full search.

### Provider Comparison Summary

| Provider | Free Tier | Auth | Reliability |
|----------|-----------|------|-------------|
| Brave | 2,000/month | API key | High |
| Google CSE | 100/day (~3k/month) | API key + CX | High |
| Serper | 2,500/month | API key | Medium-High |
| Tavily | 1,000/month | API key | Medium-High |
| DuckDuckGo Scraping | Unlimited* | None | Low |

*Unofficial, subject to blocking

See `all-providers-comparison.md` for full analysis.

### Bing vs DuckDuckGo for Scraping

If scraping is required, Bing is superior to DuckDuckGo:

| Criterion | Bing | DuckDuckGo |
|-----------|------|------------|
| Anti-bot measures | Lenient | Aggressive |
| HTML structure | Cleanest | Simple |
| Success rate | High | Moderate |
| Implementation | Simple | Complex |

See `scraping-comparison.md` for detailed analysis.

## Research Scope

For each search provider, document:
- API endpoint and authentication
- Request parameters
- Response format and fields
- Rate limits and quotas
- Error codes and responses
- Pagination

All findings are based on official documentation and external sources. Implementation decisions are in `release/plan/`.
