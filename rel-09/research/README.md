# rel-09 Research

Technical specifications and API documentation for web search providers and content fetching tools.

## In-Scope Tools for rel-09

rel-09 implements three external tools for web search and content retrieval:

| Tool | Type | Description | Status |
|------|------|-------------|--------|
| `brave` | Search API | Brave Search API (requires API key) | Complete |
| `google` | Search API | Google Custom Search API (requires API key + CX) | Complete |
| `web-fetch` | Content Fetcher | Web content retrieval and processing | Complete |

## Research Documents

### In-Scope Documents

| Document | Description | Status |
|----------|-------------|--------|
| `brave.md` | Brave Search API specification | Complete |
| `google.md` | Google Custom Search API specification | Complete |
| `all-providers-comparison.md` | Provider comparison (Brave and Google in-scope) | Complete |

### Out-of-Scope Research (Preserved for Future Reference)

| Document | Description | Status |
|----------|-------------|--------|
| `duckduckgo.md` | DuckDuckGo (not in scope for rel-09) | Reference only |
| `tavily.md` | Tavily API (not in scope for rel-09) | Reference only |
| `scraping-comparison.md` | Scraping analysis (not in scope for rel-09) | Reference only |

## Key Findings

### rel-09 Architecture

rel-09 focuses on a clean external tool architecture with three tools:

1. **brave** - Brave Search API for web search
   - Free tier: 2,000/month
   - Rate limit: 1 req/sec
   - Requires API key

2. **google** - Google Custom Search API for web search
   - Free tier: 100/day (~3,000/month)
   - Requires API key + CX ID
   - High reliability

3. **web-fetch** - Web content fetching
   - Retrieves and processes web content
   - No authentication required

### Provider Comparison (In-Scope Only)

| Provider | Free Tier | Auth | Reliability |
|----------|-----------|------|-------------|
| Brave | 2,000/month | API key | High |
| Google CSE | 100/day (~3k/month) | API key + CX | High |

See `all-providers-comparison.md` for detailed comparison.

## Research Scope

For each in-scope tool, the research documents:
- API endpoint and authentication
- Request parameters
- Response format and fields
- Rate limits and quotas
- Error codes and responses
- Pagination

All findings are based on official documentation and external sources. Implementation decisions are in `../plan/`.

## Out-of-Scope Items

The following providers and approaches were researched but are **not in scope for rel-09**:

- **DuckDuckGo** - No official search API; scraping not recommended
- **Tavily** - Smaller free tier; added complexity not needed
- **Scraping approaches** - Bing/DuckDuckGo scraping not needed with official APIs
- **Other providers** - Serper, Exa, SearXNG, etc. not in scope

These research documents are preserved for future reference and potential use in later releases.
