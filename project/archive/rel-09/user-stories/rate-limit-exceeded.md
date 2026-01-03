# Rate Limit Exceeded

## Description

User has exhausted their free search quota for the month. The LLM attempts to search, and the tool detects a 429 response from the API. A dim yellow warning explains the situation and provides actionable options. This demonstrates graceful handling of rate limits.

## Transcript

```text
> What's the latest on the Linux 6.8 kernel?

tool_call: web_search_brave({"query": "Linux 6.8 kernel features", "count": 10})

tool_result: {"success": false, "error": "Rate limit exceeded. You've used your free search quota (2,000/month).", "error_code": "RATE_LIMIT"}

⚠ Rate Limit Exceeded

You've used your free search quota (2,000 searches/month).

Options:
- Wait until your quota resets (resets monthly)
- Upgrade your plan at https://brave.com/search/api/pricing
- Configure Google Search as an alternative (100 free searches/day)

To add Google Search:
Get API key: https://developers.google.com/custom-search/v1/overview
Add credentials to ~/.config/ikigai/credentials.json:
{
  "web_search": {
    "google": {
      "api_key": "your-google-api-key",
      "engine_id": "your-search-engine-id"
    }
  }
}

Enable in ~/.config/ikigai/config.json:
{
  "web_search": {
    "google": {
      "enabled": true
    }
  }
}

I tried to search for the latest Linux 6.8 kernel information, but your Brave Search quota is exhausted for this month. You have a few options: wait until your monthly quota resets, upgrade to a paid plan, or configure Google Custom Search as an alternative (which offers 100 free searches per day).

Would you like me to help with anything else?
```

## Walkthrough

1. User asks question requiring current information
2. LLM calls `web_search_brave` tool
3. Tool checks credentials, finds `web_search.brave.api_key` present
4. Tool makes HTTP request to Brave Search API
5. API returns HTTP 429 (Rate Limit Exceeded)
6. Tool detects rate limit error
7. Tool returns error response (see [Error Response](#error-response))
8. Tool emits `config_required` event with actionable options (see [Config Event](#config-event))
9. Scrollback renders tool_call in dim gray
10. Scrollback renders tool_result in dim gray
11. Scrollback renders config_required event in dim yellow
12. LLM receives error response and explains situation with options
13. User learns about quota limits and alternatives

## Reference

### Error Response

```json
{
  "success": false,
  "error": "Rate limit exceeded. You've used your free search quota (2,000/month).",
  "error_code": "RATE_LIMIT"
}
```

### Config Event

Event stored in database (not sent to LLM):

```json
{
  "kind": "config_required",
  "content": "⚠ Rate Limit Exceeded\n\nYou've used your free search quota (2,000 searches/month).\n\nOptions:\n- Wait until your quota resets (resets monthly)\n- Upgrade your plan at https://brave.com/search/api/pricing\n- Configure Google Search as an alternative (100 free searches/day)\n\nTo add Google Search:\nGet API key: https://developers.google.com/custom-search/v1/overview\nAdd credentials to ~/.config/ikigai/credentials.json:\n{\n  \"web_search\": {\n    \"google\": {\n      \"api_key\": \"your-google-api-key\",\n      \"engine_id\": \"your-search-engine-id\"\n    }\n  }\n}\n\nEnable in ~/.config/ikigai/config.json:\n{\n  \"web_search\": {\n    \"google\": {\n      \"enabled\": true\n    }\n  }\n}",
  "data_json": "{\"tool\": \"web_search_brave\", \"error_type\": \"rate_limit\", \"alternative_tool\": \"web_search_google\"}"
}
```
