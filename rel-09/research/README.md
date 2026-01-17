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

| Document | Description |
|----------|-------------|
| `brave.md` | Brave Search API specification |
| `google.md` | Google Custom Search API specification |
| `all-providers-comparison.md` | Provider comparison showing why Brave and Google were selected |
| `html-to-markdown.md` | HTML to markdown conversion using libxml2 for web-fetch-tool |

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

**Search tools** (brave, google):
- API endpoint and authentication
- Request parameters
- Response format and fields
- Rate limits and quotas
- Error codes and responses
- Pagination

**Web-fetch tool**:
- HTML to markdown conversion using libxml2
- DOM tree traversal patterns
- Element-to-markdown mapping strategies

All findings are based on official documentation and web sources. Implementation decisions are in `../plan/`.
