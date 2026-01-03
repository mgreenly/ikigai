# Web Search API Providers - Comparison

Comprehensive comparison of web search API providers researched in December 2024.

## Key Finding

**No provider offers a public API with full search results that requires zero authentication.**

The only truly "no-auth" option is DuckDuckGo's Instant Answer API, which only returns knowledge graph results, not full search results.

## Provider Categories

### No API Key Required

| Provider | Type | Full Search | Reliability |
|----------|------|-------------|-------------|
| DuckDuckGo Instant Answer | Official API | No (knowledge graph only) | Medium |
| DuckDuckGo HTML Scraping | Unofficial | Yes | Low (fragile) |
| SearXNG (Self-hosted) | Open source | Yes (aggregated) | Medium-High |

### API Key Required - Generous Free Tier (>1000/month)

| Provider | Free Tier | Rate Limit | Paid Cost |
|----------|-----------|------------|-----------|
| Serper | 2,500/month | Not specified | $50 for 50k |
| Brave | 2,000/month | 1 req/sec | $5 per 1k |
| Exa | 2,000 one-time | Not specified | $5 per 1k |

### API Key Required - Limited Free Tier

| Provider | Free Tier | Paid Cost | Notes |
|----------|-----------|-----------|-------|
| Tavily | 1,000/month | Not specified | AI-optimized |
| Google Custom Search | 100/day (~3,000/month) | $5 per 1k | Requires API key + CX |
| SearchAPI | 100 one-time | $40/month | Trial only |
| ValueSERP | 100 one-time | Not specified | Trial only |
| Zenserp | 50/month | €23.99/month | Smallest ongoing |
| SerpAPI | 100/month | $75/month for 5k | Limited free |

### Requires Paid Subscription

| Provider | Minimum Cost | Notes |
|----------|--------------|-------|
| Perplexity | $20/month Pro | $5 API credit/month |
| YOU.com | Free tier exists | Poor documentation |

### Retired

| Provider | Status |
|----------|--------|
| Bing Search API | Retired August 11, 2025 |

## Provider Details

### Brave Search API

- **Endpoint**: `https://api.search.brave.com/res/v1/web/search`
- **Auth**: `X-Subscription-Token` header
- **Free tier**: 2,000/month, 1 req/sec
- **Signup**: api-dashboard.search.brave.com
- **Response**: JSON with structured results
- **Documentation**: Excellent
- **Reliability**: High

### Google Custom Search

- **Endpoint**: `https://customsearch.googleapis.com/customsearch/v1`
- **Auth**: API key + CX ID in query params
- **Free tier**: 100/day (~3,000/month)
- **Signup**: Requires Google Cloud project + search engine creation
- **Response**: JSON
- **Documentation**: Excellent
- **Reliability**: High

### Serper

- **Endpoint**: `https://serper.dev/search/search`
- **Auth**: Bearer token
- **Free tier**: 2,500/month (most generous)
- **Response time**: 1-3 seconds
- **Type**: Third-party Google SERP scraper

### Exa

- **Free tier**: 2,000 one-time (no expiration)
- **Features**: Semantic/neural search, AI-optimized
- **Response time**: 1.18 seconds (fastest)
- **Use case**: AI agents, RAG systems

### Tavily

- **Endpoint**: `https://api.tavily.com/search`
- **Auth**: Bearer token
- **Free tier**: 1,000/month
- **Features**: AI-generated answers, content extraction
- **Use case**: AI agents, LLM integrations

### DuckDuckGo

**Instant Answer API**
- **Endpoint**: `https://api.duckduckgo.com/`
- **Auth**: None
- **Results**: Knowledge graph only, not full search

**HTML Scraping**
- **Endpoint**: `https://html.duckduckgo.com/html/`
- **Auth**: None
- **Anti-scraping**: Aggressive (CAPTCHA, VQD, IP blocking)
- **Success rate**: 94% residential, 61% datacenter
- **Reliability**: Low

### SearXNG

- **Type**: Self-hosted meta-search
- **Auth**: None (self-hosted)
- **Sources**: Aggregates from 247 services
- **Deployment**: Docker, VPS (512MB RAM minimum)
- **Privacy**: Excellent

## Ranking by Free Tier

| Rank | Provider | Free Tier | Auth |
|------|----------|-----------|------|
| 1 | Google Custom Search | 100/day (~3k/month) | API key + CX |
| 2 | Serper | 2,500/month | API key |
| 3 | Brave | 2,000/month | API key |
| 4 | Exa | 2,000 one-time | API key |
| 5 | Tavily | 1,000/month | API key |
| 6 | SerpAPI | 100/month | API key |
| 7 | Zenserp | 50/month | API key |
| 8 | DuckDuckGo Scraping | Unlimited* | None |

*Unofficial, subject to blocking

## Ranking by Reliability

| Provider | Reliability | Notes |
|----------|-------------|-------|
| Brave | High | Official API, excellent docs |
| Google | High | Official API, well-documented |
| Serper | Medium-High | Third-party API |
| Tavily | Medium-High | AI-focused |
| Exa | Medium-High | AI-focused |
| SearXNG | Medium | Self-hosted, requires maintenance |
| DuckDuckGo Scraping | Low | Unofficial, fragile |

## Ranking by Setup Complexity

| Provider | Complexity | Steps |
|----------|------------|-------|
| DuckDuckGo Scraping | None | Just HTTP requests |
| Brave | Low | Signup → Copy API key |
| Serper | Low | Signup → Copy API key |
| Exa | Low | Signup → Copy API key |
| Tavily | Low | Signup → Copy API key |
| Google | Medium | Signup → Create CSE → Get key + CX |
| SearXNG | High | Deploy → Configure → Maintain |

## Scraping Comparison (Bing vs DuckDuckGo)

If scraping is required, Bing is superior to DuckDuckGo:

| Criterion | Bing | DuckDuckGo |
|-----------|------|------------|
| Anti-bot measures | Lenient | Aggressive (CAPTCHA, VQD) |
| HTML structure | Cleanest | Simple |
| Ad presence | Fewer | Minimal |
| Implementation | Simple | Complex |
| Success rate | High | Moderate (61% datacenter) |

See `scraping-comparison.md` for detailed analysis.

## Sources

- [Brave Search API](https://brave.com/search/api/)
- [Google Custom Search API](https://developers.google.com/custom-search/v1/overview)
- [Serper](https://serper.dev/)
- [Exa](https://exa.ai/)
- [Tavily](https://docs.tavily.com/)
- [SearXNG Documentation](https://docs.searxng.org/)
- [7 Free Web Search APIs for AI Agents - KDnuggets](https://www.kdnuggets.com/7-free-web-search-apis-for-ai-agents)
- [Best SERP API Comparison 2025](https://dev.to/ritzaco/best-serp-api-comparison-2025-serpapi-vs-exa-vs-tavily-vs-scrapingdog-vs-scrapingbee-2jci)
