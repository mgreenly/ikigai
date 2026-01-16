# rel-09: Web Tools

Search and fetch from the web in your conversations. Ask about current events, recent documentation, or anything beyond the model's training cutoff.

## Features

**Three Web Tools**
- `web-search-brave-tool` - Search using Brave Search API
- `web-search-google-tool` - Search using Google Custom Search API
- `web-fetch-tool` - Fetch URL and convert to markdown

**Key Capabilities**
- Rich search with filters (freshness, domains, language, safe search)
- Pagination for deep results
- HTML to markdown conversion
- Tools follow standard external tool protocol
- Each tool manages its own credentials

## User Experience

**With credentials configured:**
- Ask questions requiring current information or web content
- Model calls appropriate web tool automatically
- Results synthesized into natural answers

**First-time discovery:**
- Tools always advertised to LLM (even without credentials)
- On first use, tool returns helpful setup instructions
- User configures credentials once, works forever
- Two credential options: environment variables or config file

**Web fetch tool:**
- No credentials required
- Works immediately for any URL
- Returns clean markdown for LLM processing

**See detailed scenarios:**
- [First-Time Discovery](user-stories/first-time-discovery.md) - Learning about web search through use
- [Successful Search](user-stories/successful-search.md) - Normal operation with configured credentials
- [Rate Limit Exceeded](user-stories/rate-limit-exceeded.md) - Handling quota exhaustion gracefully

## Value

Makes web content accessible during conversations without requiring users to leave the CLI or manually paste content. Tools are discoverable - users learn features exist when they try to use them.
