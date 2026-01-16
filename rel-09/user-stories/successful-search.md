# Successful Search

## Description

User with configured credentials asks a question requiring current information. The LLM calls the search tool, receives multiple results, and synthesizes an answer. This demonstrates the normal operation of web search.

## Transcript

```text
> What's new in WebGPU?

tool_call: web_search_brave({"query": "WebGPU 2025 features", "count": 10})

tool_result: {"success": true, "results": [{"title": "WebGPU Lands in Chrome 113", "url": "https://chromestatus.com/feature/6213121689518080", "snippet": "WebGPU is now available by default in Chrome 113, bringing high-performance 3D graphics and compute to the web."}, {"title": "New Compute Shader Features in WebGPU", "url": "https://gpuweb.github.io/gpuweb/", "snippet": "The latest WebGPU specification adds support for async compute pipelines and improved shader debugging capabilities."}, {"title": "WebGPU Tutorial 2025", "url": "https://webgpufundamentals.org/", "snippet": "Complete guide to WebGPU covering the new features released in 2025 including compute shaders and advanced rendering."}], "count": 3}

Based on recent developments, WebGPU added several major features in 2025. Chrome 113 enabled WebGPU by default, making high-performance 3D graphics widely available. The specification now includes async compute pipelines and improved shader debugging capabilities, significantly enhancing the development experience.
```

## Walkthrough

1. User asks question requiring current information
2. LLM calls `web_search_brave` tool with query (see [Request](#request))
3. Tool checks credentials, finds `web_search.brave.api_key` present
4. Tool makes HTTP request to Brave Search API
5. Tool receives search results from API
6. Tool returns normalized response with multiple results (see [Response](#response))
7. Scrollback renders tool_call in dim gray
8. Scrollback renders tool_result in dim gray
9. LLM receives results and synthesizes answer based on snippets
10. User sees both the raw search results and the LLM's interpretation

## Reference

### Request

Tool call arguments:

```json
{
  "query": "WebGPU 2025 features",
  "count": 10
}
```

### Response

Tool result:

```json
{
  "success": true,
  "results": [
    {
      "title": "WebGPU Lands in Chrome 113",
      "url": "https://chromestatus.com/feature/6213121689518080",
      "snippet": "WebGPU is now available by default in Chrome 113, bringing high-performance 3D graphics and compute to the web."
    },
    {
      "title": "New Compute Shader Features in WebGPU",
      "url": "https://gpuweb.github.io/gpuweb/",
      "snippet": "The latest WebGPU specification adds support for async compute pipelines and improved shader debugging capabilities."
    },
    {
      "title": "WebGPU Tutorial 2025",
      "url": "https://webgpufundamentals.org/",
      "snippet": "Complete guide to WebGPU covering the new features released in 2025 including compute shaders and advanced rendering."
    }
  ],
  "count": 3
}
```
