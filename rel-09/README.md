# rel-09: Web Tools

Search and fetch from the web in your conversations. Ask about current events, recent documentation, or anything beyond the model's training cutoff.

## Features

**Two Search Tools (User's Choice of Provider)**
- `web-search-brave-tool` - Search using Brave Search API
- `web-search-google-tool` - Search using Google Custom Search API

Both search tools provide identical capabilities:
- Rich search with filters (freshness, domains, language, safe search)
- Pagination for deep results
- Require API credentials (configured once via environment variables or config file)
- Always advertised to LLM; return setup instructions if credentials not configured

**Web Fetch Tool**
- `web-fetch-tool` - Fetch any URL and convert HTML to markdown
- No credentials required
- Works immediately out of the box
- Returns clean markdown suitable for LLM processing

**All tools:**
- Follow standard external tool protocol
- Each manages its own credentials independently

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
- [Successful Search](user-stories/successful-search.md) - Normal operation with Brave Search
- [Google Custom Search](user-stories/google-search.md) - Using Google as alternative provider
- [Web Fetch](user-stories/web-fetch.md) - Fetching and analyzing URL content
- [Rate Limit Exceeded](user-stories/rate-limit-exceeded.md) - Handling quota exhaustion gracefully

## Value

Makes web content accessible during conversations without requiring users to leave the CLI or manually paste content. Tools are discoverable - users learn features exist when they try to use them.
