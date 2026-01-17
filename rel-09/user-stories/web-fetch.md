# Web Fetch

## Description

User asks LLM to fetch and analyze content from a URL. The web-fetch-tool requires no credentials and works immediately. It fetches the HTML content and converts it to markdown for the LLM to process. This demonstrates the simplicity of the fetch tool compared to search tools.

## Transcript

```text
> Can you summarize the content at https://example.com/rust-async-guide ?

tool_call: web_fetch({"url": "https://example.com/rust-async-guide"})

tool_result: {"success": true, "url": "https://example.com/rust-async-guide", "title": "Asynchronous Programming in Rust", "content": "# Asynchronous Programming in Rust\n\nRust's async/await syntax provides zero-cost abstractions for concurrent programming...\n\n## Key Concepts\n\n- **Futures**: Lazy computations that produce values\n- **async/await**: Syntax for writing async code\n- **Executors**: Runtime that drives futures to completion\n\n## Example\n\n```rust\nasync fn fetch_data() -> Result<String> {\n    let response = http::get(\"https://api.example.com\").await?;\n    Ok(response.text().await?)\n}\n```\n\nAsync Rust enables building high-performance concurrent applications without the overhead of traditional threading models."}

The article covers asynchronous programming in Rust, focusing on three key concepts:

1. **Futures** - Lazy computations that only execute when polled
2. **async/await syntax** - Ergonomic way to write asynchronous code
3. **Executors** - Runtime components that drive futures to completion

The guide emphasizes that Rust's async model provides zero-cost abstractions, enabling high-performance concurrent applications without traditional threading overhead. It includes practical examples showing how to use async/await for network requests.
```

## Walkthrough

1. User asks to fetch and analyze URL content
2. LLM calls `web_fetch` tool with URL (see [Request](#request))
3. Tool makes HTTP request to fetch HTML content
4. Tool parses HTML using libxml2
5. Tool converts HTML DOM to markdown format
6. Tool returns success response with markdown content (see [Response](#response))
7. Scrollback renders tool_call in dim gray
8. Scrollback renders tool_result in dim gray (truncated if very long)
9. LLM receives markdown content and analyzes/summarizes it
10. User gets synthesized answer without leaving the terminal

## Reference

### Request

Tool call arguments:

```json
{
  "url": "https://example.com/rust-async-guide"
}
```

### Response

Tool result:

```json
{
  "success": true,
  "url": "https://example.com/rust-async-guide",
  "title": "Asynchronous Programming in Rust",
  "content": "# Asynchronous Programming in Rust\n\nRust's async/await syntax provides zero-cost abstractions for concurrent programming...\n\n## Key Concepts\n\n- **Futures**: Lazy computations that produce values\n- **async/await**: Syntax for writing async code\n- **Executors**: Runtime that drives futures to completion\n\n## Example\n\n```rust\nasync fn fetch_data() -> Result<String> {\n    let response = http::get(\"https://api.example.com\").await?;\n    Ok(response.text().await?)\n}\n```\n\nAsync Rust enables building high-performance concurrent applications without the overhead of traditional threading models."
}
```

### Notes

- No credentials required - tool works immediately
- HTML automatically converted to markdown for LLM consumption
- Content is cleaned (scripts, styles removed during conversion)
- Tool handles common HTTP errors (404, 500, etc.) gracefully
- Markdown format includes headings, paragraphs, links, code blocks, lists
