# rel-08: Web Search

Search the web from your conversations. Ask about current events, recent documentation, or anything beyond the model's training cutoff.

## Features

**Web Search Tools**
- `web_search_brave` - Search using Brave Search API (2,000 queries/month free)
- `web_search_google` - Search using Google Custom Search API (100 queries/day free)
- Enabled tools always advertised to the model
- Helpful setup guidance if API keys not configured
- Hierarchical configuration structure supports multiple providers

**Discoverable Setup**
- Tools advertised even without API keys
- First use shows clear configuration instructions
- One-time setup in `~/.config/ikigai/credentials.json`

**Rich Search Capabilities**
- Filter by freshness (past day/week/month/year)
- Country and language preferences
- Safe search controls
- Pagination for deep results

**Implementation Phases**
- **Phase 1**: Brave Search - Complete, end-to-end functionality
- **Phase 2**: Google Search - Follow proven pattern from Phase 1

## User Experience

**Default configuration:**
- Only Brave Search enabled by default
- Simpler setup (single API key)
- Google available but disabled (user can enable in config)

**With API keys configured:**
- Ask questions requiring current information
- Model calls enabled search tool automatically
- Results appear in dimmed text (tool calls and responses)
- Model synthesizes answers from search results

**Without API keys (first time):**
- Model attempts to use search tool
- Tool returns configuration error
- Dim yellow warning shows:
  - What's needed (API key)
  - Where to get it (signup URL)
  - Where to add it (credentials.json path)
- Model explains the requirement
- User configures once, works forever

This makes web search discoverable - you learn the feature exists when you need it.

**See detailed scenarios:**
- [First-Time Discovery](user-stories/first-time-discovery.md) - Learning about web search through use
- [Successful Search](user-stories/successful-search.md) - Normal operation with configured credentials
- [Rate Limit Exceeded](user-stories/rate-limit-exceeded.md) - Handling quota exhaustion gracefully

## Configuration

**Get API Keys:**
- Brave Search: https://brave.com/search/api/ (2,000 free searches/month)
- Google Custom Search: https://developers.google.com/custom-search/v1/overview (100 free/day)

**Add to credentials.json:**

`~/.config/ikigai/credentials.json`:
```json
{
  "web_search": {
    "brave": {
      "api_key": "your-brave-api-key"
    },
    "google": {
      "api_key": "your-google-api-key",
      "engine_id": "your-search-engine-id"
    }
  }
}
```

**Configuration preferences:**

`~/.config/ikigai/config.json`:
```json
{
  "web_search": {
    "brave": {
      "enabled": true,
      "default_count": 10
    },
    "google": {
      "enabled": false,
      "default_count": 10
    }
  }
}
```

**Default behavior:**
- Brave Search enabled (simpler setup, higher quota)
- Google Search disabled (user can enable if needed)
- Only enabled tools are advertised to the model

After adding credentials, search tools work immediately in your next conversation.
