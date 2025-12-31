# First-Time Discovery

## Description

User asks a question requiring current information without having configured web search credentials. The tool returns a configuration error, and a dim yellow warning provides clear setup instructions. This demonstrates the discoverability benefit of always advertising search tools.

## Transcript

```text
> What's new in WebGPU?

tool_call: web_search_brave({"query": "WebGPU 2025 features", "count": 10})

tool_result: {"success": false, "error": "Web search requires API key configuration.\n\nBrave Search offers 2,000 free searches/month.\nGet your key: https://brave.com/search/api/\nAdd to: ~/.config/ikigai/credentials.json as 'web_search.brave.api_key'"}

⚠ Configuration Required

Web search needs an API key. Brave Search offers 2,000 free searches/month.

Get your key: https://brave.com/search/api/
Add to: ~/.config/ikigai/credentials.json

Example:
{
  "web_search": {
    "brave": {
      "api_key": "your-api-key-here"
    }
  }
}

I tried to search the web for current WebGPU information, but web search isn't configured yet. To enable it, you'll need to get a free Brave Search API key (2,000 searches per month) and add it to your credentials file as shown above.

Would you like me to help with anything else I can do without web access?
```

## Walkthrough

1. User asks question requiring current information
2. LLM calls `web_search_brave` tool (advertised when enabled, even without credentials)
3. Tool checks credentials, finds `web_search.brave.api_key` missing
4. Tool returns error response (see [Error Response](#error-response))
5. Tool emits `config_required` event (see [Config Event](#config-event))
6. Scrollback renders tool_call and tool_result in dim gray
7. Scrollback renders config_required event in dim yellow
8. LLM receives error response and explains situation to user
9. User learns web search feature exists and how to enable it

## Reference

### Error Response

```json
{
  "success": false,
  "error": "Web search requires API key configuration.\n\nBrave Search offers 2,000 free searches/month.\nGet your key: https://brave.com/search/api/\nAdd to: ~/.config/ikigai/credentials.json as 'web_search.brave.api_key'"
}
```

### Config Event

Event stored in database (not sent to LLM):

```json
{
  "kind": "config_required",
  "content": "⚠ Configuration Required\n\nWeb search needs an API key. Brave Search offers 2,000 free searches/month.\n\nGet your key: https://brave.com/search/api/\nAdd to: ~/.config/ikigai/credentials.json\n\nExample:\n{\n  \"web_search\": {\n    \"brave\": {\n      \"api_key\": \"your-api-key-here\"\n    }\n  }\n}",
  "data_json": "{\"tool\": \"web_search_brave\", \"credential\": \"api_key\", \"signup_url\": \"https://brave.com/search/api/\"}"
}
```
