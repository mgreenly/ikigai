# Provider Configuration Research

**Research Date:** December 14, 2025
**Researcher:** Claude Opus 4.5
**Purpose:** Document API key management, authentication patterns, and configuration best practices for ikigai's multi-provider support

---

## Summary

This research documents the standard environment variables, API key formats, authentication mechanisms, and error handling patterns across five AI providers (Anthropic, OpenAI, Google, xAI, Meta). Key findings:

1. **Environment Variables**: All providers have established conventions (`ANTHROPIC_API_KEY`, `OPENAI_API_KEY`, `GOOGLE_API_KEY`, `XAI_API_KEY`, `LLAMA_API_KEY`)
2. **Key Prefixes**: Most providers use identifiable prefixes (`sk-ant-` for Anthropic, `sk-` or `sk-proj-` for OpenAI, `xai-` for xAI)
3. **Auth Headers**: Anthropic uses `x-api-key` (unique), all others use `Authorization: Bearer`
4. **Rate Limit Headers**: All providers expose rate limit information via HTTP headers, though format varies
5. **Credentials Security**: All providers recommend 600 file permissions and storing keys in environment variables

---

## Configuration by Provider

### Anthropic (Claude)

| Aspect | Value | Notes | Confidence |
|--------|-------|-------|------------|
| **Env Var** | `ANTHROPIC_API_KEY` | Official SDK default | **Verified** |
| **Key Prefix** | `sk-ant-api03-` | 48 additional characters | **Verified** |
| **Key Format** | `^sk-ant-api03-[A-Za-z0-9_-]{48}$` | GitGuardian pattern | **Verified** |
| **Auth Header** | `x-api-key: {key}` | NOT `Authorization: Bearer`! | **Verified** |
| **Extra Headers** | `anthropic-version: 2023-06-01` | Required for all requests | **Verified** |
| **Content-Type** | `application/json` | Standard | **Verified** |
| **Keys URL** | https://console.anthropic.com/settings/keys | Direct link | **Verified** |
| **Bad Key Error** | HTTP 401 `authentication_error` | `{"type":"error","error":{"type":"authentication_error","message":"Invalid API key"}}` | **Verified** |
| **Rate Limit Headers** | `anthropic-ratelimit-requests-remaining`<br>`anthropic-ratelimit-tokens-remaining`<br>`retry-after` (on 429) | Headers show most restrictive limit | **Verified** |

**Sources:**
- [Anthropic SDK (GitHub)](https://github.com/anthropics/anthropic-sdk-python)
- [Anthropic | aider](https://aider.chat/docs/llms/anthropic.html)
- [Claude API Key Tester](https://trevorfox.com/api-key-tester/anthropic)
- [Rate limits - Claude Docs](https://docs.claude.com/en/api/rate-limits)
- [How to Get Your Claude API Key - Apideck](https://www.apideck.com/blog/how-to-get-your-claude-anthropic-api-key)

---

### OpenAI

| Aspect | Value | Notes | Confidence |
|--------|-------|-------|------------|
| **Env Var** | `OPENAI_API_KEY` | Official SDK default | **Verified** |
| **Key Prefix** | `sk-` (legacy)<br>`sk-proj-` (project keys)<br>`sk-svcacct-` (service account)<br>`sk-admin-` (admin keys) | Modern keys use project/account prefixes | **Verified** |
| **Key Format** | `^sk-(?:proj-\|svcacct-\|admin-)?[A-Za-z0-9_-]{20,74}T3BlbkFJ[A-Za-z0-9_-]{20,74}$` | Community regex (no official pattern) | **Likely** |
| **Auth Header** | `Authorization: Bearer {key}` | Standard OAuth2 format | **Verified** |
| **Extra Headers** | `Content-Type: application/json`<br>`OpenAI-Organization` (optional)<br>`OpenAI-Project` (optional) | Org/project headers for multi-tenant | **Verified** |
| **Keys URL** | https://platform.openai.com/api-keys | Dashboard link | **Verified** |
| **Bad Key Error** | HTTP 401 `invalid_api_key` | `{"error":{"message":"Incorrect API key provided...","type":"invalid_request_error","code":"invalid_api_key"}}` | **Verified** |
| **Rate Limit Headers** | `x-ratelimit-limit-requests`<br>`x-ratelimit-remaining-requests`<br>`x-ratelimit-reset-requests`<br>`x-ratelimit-limit-tokens`<br>`x-ratelimit-remaining-tokens`<br>`x-ratelimit-reset-tokens` | Comprehensive limits for requests + tokens | **Verified** |

**Sources:**
- [Developer quickstart - OpenAI](https://platform.openai.com/docs/quickstart)
- [Best Practices for API Key Safety - OpenAI](https://help.openai.com/en/articles/5112595-best-practices-for-api-key-safety)
- [Rate limits - OpenAI](https://platform.openai.com/docs/guides/rate-limits)
- [Gitleaks OpenAI Regex PR](https://github.com/gitleaks/gitleaks/pull/1780)

---

### Google (Gemini)

| Aspect | Value | Notes | Confidence |
|--------|-------|-------|------------|
| **Env Var** | `GOOGLE_API_KEY` (primary)<br>`GEMINI_API_KEY` (fallback) | `GOOGLE_API_KEY` takes precedence | **Verified** |
| **Key Prefix** | Unknown | No documented prefix pattern | **Assumed** |
| **Key Format** | Unknown | Google doesn't publish validation pattern | **Assumed** |
| **Auth Header** | Query param: `?key={api_key}`<br>OR<br>`Authorization: Bearer {key}` | Query param is primary method | **Verified** |
| **Extra Headers** | `Content-Type: application/json` | Standard | **Verified** |
| **Keys URL** | https://aistudio.google.com (click "Get API key") | Google AI Studio | **Verified** |
| **Bad Key Error** | HTTP 403 `PERMISSION_DENIED` | `{"error":{"code":403,"message":"Your API key was reported as leaked...","status":"PERMISSION_DENIED"}}` | **Verified** |
| **Rate Limit Headers** | No standard headers | Response body includes `retryDelay` field on 429 | **Verified** |
| **Rate Limit Response** | HTTP 429 `RESOURCE_EXHAUSTED` | `{"error":{"code":429,"status":"RESOURCE_EXHAUSTED"}}` with quota details | **Verified** |

**Sources:**
- [Using Gemini API keys - Google AI](https://ai.google.dev/gemini-api/docs/api-key)
- [Gemini API quickstart - Google AI](https://ai.google.dev/gemini-api/docs/quickstart)
- [Rate limits - Gemini API](https://ai.google.dev/gemini-api/docs/rate-limits)
- [How to Get FREE Google Gemini API Access - StephenWThomas](https://www.stephenwthomas.com/azure-integration-thoughts/how-to-get-free-google-gemini-api-access-step-by-step-guide-for-2025/)

---

### xAI (Grok)

| Aspect | Value | Notes | Confidence |
|--------|-------|-------|------------|
| **Env Var** | `XAI_API_KEY` | SDK default | **Verified** |
| **Key Prefix** | `xai-` | Confirmed by multiple sources | **Verified** |
| **Key Format** | `^xai-[A-Za-z0-9_-]+$` | Estimated pattern | **Likely** |
| **Auth Header** | `Authorization: Bearer {key}` | Standard OAuth2 format | **Verified** |
| **Extra Headers** | `Content-Type: application/json` | Standard | **Verified** |
| **Keys URL** | https://console.x.ai (API Keys section) | xAI Console | **Verified** |
| **Bad Key Error** | HTTP 401 `invalid_api_key` | `{"error":{"message":"Invalid authentication credentials","type":"invalid_request_error","code":"invalid_api_key"}}` | **Verified** |
| **Rate Limit Headers** | `x-ratelimit-limit-requests`<br>`x-ratelimit-remaining-requests`<br>`x-ratelimit-reset-requests` | Similar to OpenAI format | **Verified** |

**Sources:**
- [AI SDK Providers: xAI Grok](https://ai-sdk.dev/providers/ai-sdk-providers/xai)
- [How to Get Your Grok (XAI) API Key - Apideck](https://www.apideck.com/blog/how-to-get-your-grok-xai-api-key)
- [The Hitchhiker's Guide to Grok - xAI](https://docs.x.ai/docs/tutorial)
- [Consumption and Rate Limits - xAI](https://docs.x.ai/docs/key-information/consumption-and-rate-limits)

---

### Meta (Llama)

| Aspect | Value | Notes | Confidence |
|--------|-------|-------|------------|
| **Env Var** | `LLAMA_API_KEY` | Official client default | **Verified** |
| **Key Prefix** | Unknown | No documented prefix | **Assumed** |
| **Key Format** | Unknown | Meta doesn't publish pattern | **Assumed** |
| **Auth Header** | `Authorization: Bearer {key}` | Standard OAuth2 format | **Verified** |
| **Extra Headers** | `Content-Type: application/json` | Standard | **Verified** |
| **Keys URL** | https://llama.developer.meta.com (dashboard) | Meta Llama Developer Platform | **Verified** |
| **Bad Key Error** | HTTP 401 `authentication_error` | `{"error":{"message":"Invalid authentication credentials","type":"authentication_error","code":"invalid_api_key"}}` | **Likely** |
| **Rate Limit Headers** | No standard headers documented | SDK auto-retries on 429 | **Likely** |
| **Retry Logic** | `retry_after` field in error response | SDK default: 2 retries with exponential backoff | **Verified** |

**Sources:**
- [API keys - Llama Developer](https://llama.developer.meta.com/docs/api-keys/)
- [llama-api-client - PyPI](https://pypi.org/project/llama-api-client/)
- [How to get your Llama API key - Merge](https://www.merge.dev/blog/llama-api-key)
- [Rate Limits - Llama Developer](https://llama.developer.meta.com/docs/rate-limits/)

---

## Rate Limit Headers Comparison

| Provider | Limit Header(s) | Remaining Header(s) | Reset Header | Notes |
|----------|----------------|---------------------|--------------|-------|
| **Anthropic** | `anthropic-ratelimit-requests-limit`<br>`anthropic-ratelimit-tokens-limit` | `anthropic-ratelimit-requests-remaining`<br>`anthropic-ratelimit-tokens-remaining` | `retry-after` (on 429 only) | Shows most restrictive limit currently active |
| **OpenAI** | `x-ratelimit-limit-requests`<br>`x-ratelimit-limit-tokens` | `x-ratelimit-remaining-requests`<br>`x-ratelimit-remaining-tokens` | `x-ratelimit-reset-requests`<br>`x-ratelimit-reset-tokens` | Duration format (e.g., "6m0s") |
| **Google** | N/A (in response body) | N/A | `retryDelay` in error response | 429 error includes quota details in JSON |
| **xAI** | `x-ratelimit-limit-requests` | `x-ratelimit-remaining-requests` | `x-ratelimit-reset-requests` | Similar to OpenAI |
| **Meta** | N/A (SDK handles) | N/A | `retry_after` in error response | SDK auto-retries with backoff |

---

## credentials.json Recommendations

Based on research findings, the recommended format:

```json
{
  "anthropic": {
    "api_key": "sk-ant-api03-..."
  },
  "openai": {
    "api_key": "sk-proj-..."
  },
  "google": {
    "api_key": "..."
  },
  "xai": {
    "api_key": "xai-..."
  },
  "meta": {
    "api_key": "..."
  }
}
```

**File Permissions:** `600` (owner read/write only) - universally recommended by all providers

**Location:** Same directory as `config.json` (typically `~/.config/ikigai/` or similar)

---

## Environment Variable Summary

| Provider | Primary Env Var | SDK Auto-Load | Precedence |
|----------|----------------|---------------|------------|
| Anthropic | `ANTHROPIC_API_KEY` | Yes (Python, TypeScript) | Env var → credentials.json |
| OpenAI | `OPENAI_API_KEY` | Yes (Python, JS/TS, Go, C#) | Env var → credentials.json |
| Google | `GOOGLE_API_KEY` | Yes (Python, JS/TS) | `GOOGLE_API_KEY` > `GEMINI_API_KEY` |
| xAI | `XAI_API_KEY` | Yes (SDK) | Env var → credentials.json |
| Meta | `LLAMA_API_KEY` | Yes (Python, TypeScript) | Env var → credentials.json |

**Recommendation for ikigai:**
```
Environment Variable → credentials.json → Error
```

This matches industry standard practice and allows users flexibility while maintaining security.

---

## Validation Regex Patterns

For early key format validation (optional):

```c
// Anthropic
const char* ANTHROPIC_KEY_PATTERN = "^sk-ant-api03-[A-Za-z0-9_-]{48}$";

// OpenAI (supports multiple formats)
const char* OPENAI_KEY_PATTERN = "^sk-(?:proj-|svcacct-|admin-)?[A-Za-z0-9_-]{20,74}T3BlbkFJ[A-Za-z0-9_-]{20,74}$";

// xAI
const char* XAI_KEY_PATTERN = "^xai-[A-Za-z0-9_-]+$";

// Google, Meta: No published patterns
// Validation strategy: Attempt API call and handle errors
```

**Note:** Regex validation is optional. The authoritative validation is always the provider's API response.

---

## Error Code Mapping

### Bad/Invalid API Key

| Provider | HTTP Status | Error Type | Error Code |
|----------|-------------|------------|------------|
| Anthropic | 401 | `authentication_error` | N/A |
| OpenAI | 401 | `invalid_request_error` | `invalid_api_key` |
| Google | 403 | N/A | `PERMISSION_DENIED` |
| xAI | 401 | `invalid_request_error` | `invalid_api_key` |
| Meta | 401 | `authentication_error` | `invalid_api_key` |

**Mapping to ikigai error category:** `AUTH`

### Rate Limit Exceeded

| Provider | HTTP Status | Error Type | Error Code |
|----------|-------------|------------|------------|
| Anthropic | 429 | `rate_limit_error` | N/A |
| OpenAI | 429 | `rate_limit_error` | `rate_limit_exceeded` |
| Google | 429 | N/A | `RESOURCE_EXHAUSTED` |
| xAI | 429 | `rate_limit_error` | `rate_limit_exceeded` |
| Meta | 429 | `rate_limit_error` | N/A |

**Mapping to ikigai error category:** `RATE_LIMIT`

**Retry Strategy:** All providers recommend exponential backoff with jitter. Honor `retry-after` header when present.

### Server Errors

| Provider | HTTP Status | Category |
|----------|-------------|----------|
| Anthropic | 500, 502, 529 | `api_error`, `timeout_error`, `overloaded_error` |
| OpenAI | 500, 503 | `api_error`, `service_unavailable` |
| Google | 500, 503, 504 | `INTERNAL`, `UNAVAILABLE`, `DEADLINE_EXCEEDED` |
| xAI | 500, 503 | `server_error`, `service_unavailable` |
| Meta | 500, 502, 503 | Auto-retried by SDK |

**Mapping to ikigai error category:** `SERVER`

**Retry Strategy:** Auto-retry with backoff (2-3 attempts recommended)

---

## Key Acquisition URLs

| Provider | Console URL | Direct API Keys Link |
|----------|-------------|----------------------|
| Anthropic | https://console.anthropic.com | https://console.anthropic.com/settings/keys |
| OpenAI | https://platform.openai.com | https://platform.openai.com/api-keys |
| Google | https://aistudio.google.com | Click "Get API key" button in dashboard |
| xAI | https://console.x.ai | Navigate to API Keys section |
| Meta | https://llama.developer.meta.com | Click "+" to create API token |

**Important Notes:**
1. **Anthropic, xAI, Meta:** Keys shown only once at creation - must copy immediately
2. **OpenAI:** Requires payment method setup (no free trial credits in 2025)
3. **Google:** Free tier available without billing for limited usage
4. **All providers:** Require account creation and terms acceptance

---

## Key Permissions and Scopes

### Anthropic
- Keys are organization-scoped
- No granular permissions per key
- Separate workspaces for isolation

### OpenAI
- **2025 Model:** Project-scoped API keys and service accounts
- Multiple service accounts per project
- Keys can be project-specific or organization-wide
- Admin keys available for organization owners only

### Google
- Keys tied to Google Cloud Project
- No per-key scopes (project-level IAM controls)
- Free tier vs. paid tier determined by billing status

### xAI
- Customizable permissions per key:
  - `chat:write` - Full Grok access
  - `sampler:write` - Raw Grok-1 model sampling
- Keys can be restricted to specific capabilities

### Meta
- API keys during preview period (limited production features)
- No documented permission scopes yet

**Recommendation for ikigai:** Assume full API access for all keys. Per-key permissions are out of scope for rel-07.

---

## Key Rotation Support

| Provider | Rotation Support | Notes |
|----------|-----------------|-------|
| Anthropic | Manual | Create new key, update apps, delete old key |
| OpenAI | Manual | Multiple keys supported for zero-downtime rotation |
| Google | Manual | Multiple keys per project allowed |
| xAI | Manual | Multiple keys can be created |
| Meta | Manual | Multiple tokens supported |

**Best Practice:** All providers recommend rotating keys every 60-90 days for security.

**ikigai Implementation:** Not required for rel-07. Users manage rotation externally.

---

## File Permissions

**Universal Recommendation:** `600` (owner read/write only)

All providers emphasize:
- Never commit credentials to version control
- Store in `.env` files (added to `.gitignore`)
- Use environment variables in production
- Consider secret management services (AWS Secrets Manager, HashiCorp Vault) for enterprise

**ikigai `credentials.json` Recommendation:**
```bash
chmod 600 ~/.config/ikigai/credentials.json
```

Warn user if permissions are too open (e.g., world-readable).

---

## Additional Configuration Notes

### Logging Environment Variables

Some SDKs support debug logging via environment variables:

- **Anthropic:** `ANTHROPIC_LOG=debug`
- **OpenAI:** Various logging libraries (not standardized)
- **Meta:** `LLAMA_API_CLIENT_LOG=debug`

**ikigai:** May want to support `IKIGAI_LOG_LEVEL` for debugging provider requests.

### Base URLs

All providers use HTTPS and have stable base URLs:

| Provider | Base URL |
|----------|----------|
| Anthropic | `https://api.anthropic.com` |
| OpenAI | `https://api.openai.com` |
| Google | `https://generativelanguage.googleapis.com/v1beta/` |
| xAI | `https://api.x.ai/v1` |
| Meta | `https://api.llama.com` |

**OpenAI Compatibility:**
- **Google:** `https://generativelanguage.googleapis.com/v1beta/openai/`
- **Meta:** `https://api.llama.com/compat/v1`

---

## Missed Concepts and Open Questions

### 1. API Key Expiration
- **Finding:** No provider documents automatic key expiration
- **Implication:** Keys remain valid indefinitely unless manually revoked
- **Recommendation:** ikigai should not implement expiration logic

### 2. IP Whitelisting
- **Finding:** Not commonly supported at API level (may be available in enterprise tiers)
- **Implication:** Not needed for ikigai implementation

### 3. Usage Quotas vs Rate Limits
- **Finding:** Providers distinguish between:
  - **Rate limits:** Requests/tokens per minute/hour
  - **Quotas:** Daily/monthly caps or spending limits
- **Implication:** ikigai should handle both 429 (rate limit) and quota exceeded errors

### 4. Billing Requirements
- **Anthropic:** Requires "Build" plan + credits (free $5 in select regions)
- **OpenAI:** Requires payment method setup (no free trial in 2025)
- **Google:** Free tier available without billing
- **xAI:** Requires payment method setup
- **Meta:** Preview period (check current status)

**ikigai Documentation:** Should note billing requirements per provider

### 5. Regional Availability
- **Finding:** Provider APIs have different regional availability
- **Anthropic:** Free credits only in select regions
- **Google:** Free tier "not available in all regions"
- **Others:** Generally available globally

**ikigai:** Not our concern - users must verify eligibility

### 6. OAuth vs API Keys
- **Finding:** All five providers use static API keys, not OAuth flows
- **Implication:** No need for OAuth refresh token logic in rel-07

### 7. Streaming Authentication
- **Finding:** Same auth headers work for both streaming and non-streaming
- **Implication:** No special handling needed

### 8. Multi-Key Support
- **Question:** Should ikigai support multiple keys per provider?
- **Use Case:** Development vs. production keys, team accounts
- **Decision:** Defer to future release (one key per provider for rel-07)

---

## Implementation Checklist for ikigai

### Configuration Files

- [x] `credentials.json` structure defined
- [x] `config.json` provider sections defined
- [x] File permission validation (warn if not 600)
- [x] Environment variable precedence documented

### Per-Provider Modules

For each provider, implement:

- [x] Read `{PROVIDER}_API_KEY` environment variable
- [x] Fall back to `credentials.json` if env var not set
- [x] Construct correct auth headers (note Anthropic's `x-api-key`!)
- [x] Include required extra headers (e.g., `anthropic-version`)
- [x] Map HTTP error codes to ikigai error categories
- [x] Extract rate limit headers (if available)
- [x] Implement exponential backoff retry logic

### Optional Enhancements

- [ ] Validate API key format using regex (nice-to-have)
- [ ] Warn if key format doesn't match expected pattern
- [ ] Cache key validation results to avoid repeated auth errors
- [ ] Log rate limit headers for monitoring

### User Documentation

- [ ] Document environment variable names per provider
- [ ] Provide links to obtain API keys
- [ ] Explain `credentials.json` vs environment variables
- [ ] Show example of setting file permissions
- [ ] Note billing requirements per provider

---

## Sources

This research compiled information from official documentation, SDK repositories, and developer resources:

### Anthropic
- [Anthropic SDK - GitHub](https://github.com/anthropics/anthropic-sdk-python)
- [Rate limits - Claude Docs](https://docs.claude.com/en/api/rate-limits)
- [Anthropic Console](https://console.anthropic.com/settings/keys)

### OpenAI
- [Developer quickstart - OpenAI](https://platform.openai.com/docs/quickstart)
- [Best Practices for API Key Safety](https://help.openai.com/en/articles/5112595-best-practices-for-api-key-safety)
- [Rate limits - OpenAI](https://platform.openai.com/docs/guides/rate-limits)

### Google
- [Using Gemini API keys](https://ai.google.dev/gemini-api/docs/api-key)
- [Gemini API quickstart](https://ai.google.dev/gemini-api/docs/quickstart)
- [Rate limits - Gemini API](https://ai.google.dev/gemini-api/docs/rate-limits)

### xAI
- [The Hitchhiker's Guide to Grok](https://docs.x.ai/docs/tutorial)
- [Consumption and Rate Limits](https://docs.x.ai/docs/key-information/consumption-and-rate-limits)
- [xAI API Overview](https://docs.x.ai/docs/overview)

### Meta
- [API keys - Llama Developer](https://llama.developer.meta.com/docs/api-keys/)
- [llama-api-client - PyPI](https://pypi.org/project/llama-api-client/)
- [Llama API Overview](https://llama.developer.meta.com/docs/overview/)

### Community Resources
- [Gitleaks OpenAI Regex PR](https://github.com/gitleaks/gitleaks/pull/1780)
- [GitGuardian Claude Key Detector](https://docs.gitguardian.com/secrets-detection/secrets-detection-engine/detectors/specifics/claude_api_key)

---

**Document Version:** 1.0
**Last Updated:** December 14, 2025
**Next Review:** Before rel-07 implementation phase
