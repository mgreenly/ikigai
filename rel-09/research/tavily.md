# Tavily Search API

Technical specification for Tavily Search API.

**Status**: Researched but excluded from rel-08.

**Exclusion rationale:**
- Smaller free tier (1,000 vs Brave's 2,000/month)
- Slower responses (content extraction overhead)
- Added complexity (POST/JSON vs simple GET)
- AI-optimization features not essential for terminal coding agent

**Official Documentation**: https://docs.tavily.com/

## Overview

Tavily is a search API purpose-built for AI agents and LLMs. Unlike traditional search APIs that return raw results, Tavily performs content extraction and filtering in a single API call.

## Key Features

### AI-Generated Answers

Tavily can generate a direct answer using an LLM:

```json
{
  "answer": "Linux terminal is a text-based interface...",
  "results": [...]
}
```

Answer options:
- `include_answer: false` - No answer (default)
- `include_answer: true` or `"basic"` - Quick answer
- `include_answer: "advanced"` - Detailed answer (slower)

### Content Extraction

Unlike traditional APIs that return snippets, Tavily:
1. Searches up to 20 sources per query
2. Scrapes full page content
3. Uses AI to extract query-relevant content
4. Filters out ads, navigation, boilerplate
5. Returns cleaned, focused content

### Token Efficiency

Tavily claims 40% fewer tokens than raw SERP data for equivalent information.

## API Specification

### Endpoint

```
POST https://api.tavily.com/search
```

### Authentication

```http
Authorization: Bearer <API_KEY>
```

### Request Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `query` | string | required | Search query |
| `search_depth` | string | `"basic"` | `"basic"` or `"advanced"` |
| `include_answer` | bool/string | `false` | `false`, `true`, `"basic"`, or `"advanced"` |
| `include_images` | bool | `false` | Include images |
| `include_raw_content` | bool | `false` | Include raw HTML |
| `max_results` | int | `5` | Results count (1-20) |
| `include_domains` | list | `[]` | Domains to include |
| `exclude_domains` | list | `[]` | Domains to exclude |
| `topic` | string | `"general"` | `"general"` or `"news"` |

### Response Format

```json
{
  "answer": "AI-generated answer (if requested)",
  "query": "original query",
  "response_time": 2.45,
  "images": ["url1", "url2"],
  "results": [
    {
      "title": "Result title",
      "url": "https://example.com/page",
      "content": "Extracted relevant content",
      "raw_content": "Full HTML (if requested)",
      "score": 0.95,
      "published_date": "2025-01-15"
    }
  ]
}
```

### Response Fields

| Field | Type | Description |
|-------|------|-------------|
| `answer` | string | LLM-generated answer (optional) |
| `query` | string | Original query |
| `response_time` | float | Response time in seconds |
| `images` | list | Query-related image URLs |
| `results` | list | Ranked search results |
| `results[].title` | string | Page title |
| `results[].url` | string | Page URL |
| `results[].content` | string | Extracted content |
| `results[].score` | float | Relevance score (0-1) |
| `results[].published_date` | string | Publication date |

## Rate Limits

### Free Tier

- 1,000 queries/month
- No credit card required

### Paid Pricing

- Not publicly documented
- Usage-based pricing

## Comparison with Other APIs

| Feature | Tavily | Brave | Serper |
|---------|--------|-------|--------|
| Purpose | AI/LLM agents | General search | SERP scraping |
| Answer generation | Yes | No | No |
| Content extraction | Yes (AI-filtered) | No (snippets) | No (snippets) |
| Multi-source | Yes (up to 20) | No | No |
| Free tier | 1,000/month | 2,000/month | 2,500/month |
| Speed | Slower (extraction) | Fast | Fast |

## When to Use Tavily

**Good for:**
- AI agents needing synthesized answers
- RAG systems
- Research tools requiring deep content

**Not ideal for:**
- Traditional search UI
- High volume (smaller free tier)
- Simple link lookups

## Sources

- [Tavily - The Web Access Layer for AI Agents](https://www.tavily.com/)
- [Tavily Docs](https://docs.tavily.com/)
- [Tavily Search API Reference](https://docs.tavily.com/documentation/api-reference/endpoint/search)
- [How to Add Real-Time Web Search to Your LLM Using Tavily - freeCodeCamp](https://www.freecodecamp.org/news/how-to-add-real-time-web-search-to-your-llm-using-tavily/)
