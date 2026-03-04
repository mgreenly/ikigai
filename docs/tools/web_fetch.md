# web_fetch

## NAME

web_fetch - fetch content from a URL and return it as Markdown

## SYNOPSIS

```json
{"name": "web_fetch", "arguments": {"url": "..."}}
```

## DESCRIPTION

`web_fetch` retrieves the content at a URL and returns it as Markdown text. HTML pages are converted to Markdown using libxml2; other content types are returned as-is where possible.

It supports pagination via `offset` and `limit`, mirroring the interface of `file_read`. This is useful for long pages — fetch an initial chunk to orient yourself, then fetch subsequent chunks as needed.

## PARAMETERS

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `url` | string (URI) | yes | The URL to fetch. Must be a valid absolute URI. |
| `offset` | integer | no | Line number to start reading from (1-based). Omit to start from the beginning. |
| `limit` | integer | no | Maximum number of lines to return. Omit to return all content. |

## RETURNS

The page content converted to Markdown. Links are preserved as Markdown hyperlinks. If the request fails (network error, HTTP error, unsupported content type), an error message is returned.

## EXAMPLES

Fetch a documentation page:

```json
{
  "name": "web_fetch",
  "arguments": {
    "url": "https://www.postgresql.org/docs/current/sql-select.html"
  }
}
```

Fetch only the first 50 lines to get an overview:

```json
{
  "name": "web_fetch",
  "arguments": {
    "url": "https://talloc.samba.org/talloc/doc/html/libtalloc__tutorial_01.html",
    "limit": 50
  }
}
```

Fetch a subsequent page of content:

```json
{
  "name": "web_fetch",
  "arguments": {
    "url": "https://talloc.samba.org/talloc/doc/html/libtalloc__tutorial_01.html",
    "offset": 51,
    "limit": 100
  }
}
```

## NOTES

HTML-to-Markdown conversion is best-effort. Complex page layouts, JavaScript-rendered content, and non-HTML formats may not convert cleanly.

Use `offset` and `limit` for long pages to avoid overloading the context window. Combine with `web_search` to find the right URL before fetching.

## SEE ALSO

[web_search](web_search.md), [file_read](file_read.md), [Tools](../tools.md)
