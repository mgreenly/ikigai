# web_search

## NAME

web_search - search the web and return results

## SYNOPSIS

```json
{"name": "web_search", "arguments": {"query": "..."}}
```

## DESCRIPTION

`web_search` queries the Brave Search API and returns a list of search results with titles, URLs, and descriptions. Use it to find current information, look up documentation, or discover URLs to fetch with `web_fetch`.

Results include links formatted as Markdown hyperlinks. Pagination is supported via `offset` to retrieve additional result pages.

## PARAMETERS

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `query` | string | yes | The search query. Minimum 2 characters. |
| `count` | integer | no | Number of results to return (1–20). Default: 10. |
| `offset` | integer | no | Result offset for pagination. Default: 0. |
| `allowed_domains` | array of strings | no | Restrict results to these domains only. |
| `blocked_domains` | array of strings | no | Exclude results from these domains. |

## RETURNS

Search result blocks containing titles, URLs (as Markdown hyperlinks), and snippet descriptions for each result. Returns up to `count` results starting at `offset`.

## EXAMPLES

Basic search:

```json
{
  "name": "web_search",
  "arguments": {
    "query": "talloc hierarchical memory allocator tutorial"
  }
}
```

Search with domain restriction:

```json
{
  "name": "web_search",
  "arguments": {
    "query": "PostgreSQL LISTEN NOTIFY",
    "allowed_domains": ["postgresql.org"]
  }
}
```

Fetch more results for a broad query:

```json
{
  "name": "web_search",
  "arguments": {
    "query": "C11 thread local storage examples",
    "count": 20
  }
}
```

Get the next page of results:

```json
{
  "name": "web_search",
  "arguments": {
    "query": "jujutsu vcs workflow",
    "count": 10,
    "offset": 10
  }
}
```

## NOTES

Requires a Brave Search API key configured in the environment. If the key is missing or invalid, the tool returns an authentication error.

Use `allowed_domains` to focus results on authoritative sources (e.g., official documentation sites). Use `blocked_domains` to filter out content farms or paywalled sites that pollute results.

Follow a search with `web_fetch` to read the full content of a promising result.

## SEE ALSO

[web_fetch](web_fetch.md), [Tools](../tools.md)
