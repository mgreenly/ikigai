# Web Search Scraping - Service Comparison

## Executive Summary

**Research Question**: If we're going to scrape a non-documented but public API, which service is best?

**Answer**: **Bing** is the best scraping target, followed by DuckDuckGo. Both are significantly better than Google.

## Evaluation Criteria

1. **HTML Structure Simplicity** - How easy to parse
2. **Ad Presence & Identifiability** - Fewer ads, easier to filter
3. **Anti-Scraping Measures** - CAPTCHA, bot detection, IP blocking
4. **Rate Limiting** - How lenient
5. **HTML Stability** - How often structure changes
6. **Legal/ToS** - Terms of service stance on scraping

## Detailed Comparison

### 1. Bing

**HTML Structure**: ⭐⭐⭐⭐⭐ (Excellent)
- "Cleaner HTML structure" than Google
- "More stable search data"
- Straightforward CSS selectors
- Works well with simple BeautifulSoup parsing

**Ad Presence**: ⭐⭐⭐⭐ (Good)
- **Fewer ads than Google**
- "Bing tends to show fewer ads at the top of search results"
- Organic listings get more space and attention
- Easier to identify and filter sponsored content

**Anti-Scraping**: ⭐⭐⭐⭐ (Good)
- "Fewer anti-bot headaches than Google"
- Less aggressive bot detection
- More tolerant of automated access
- "AI-powered tools can adapt" to minor HTML changes

**Rate Limiting**: ⭐⭐⭐ (Moderate)
- Not explicitly documented
- Less restrictive than Google
- Multiple scraping services report success

**HTML Stability**: ⭐⭐⭐⭐ (Good)
- "Bing loves to tweak its HTML" (occasional changes)
- But overall more stable than Google
- Changes less frequent and less dramatic

**Legal/ToS**: ⚠️ (Gray Area)
- Official Bing Search API was retired August 11, 2025
- ToS generally prohibit scraping (standard boilerplate)
- Enforcement appears minimal for reasonable use

**Result Quality**: ⭐⭐⭐⭐ (Good)
- "Fewer companies actively target Bing for SEO"
- "Less influenced by aggressive keyword-stuffing or content farms"
- "More likely to get genuinely informative pages"

**Verdict**: **Best scraping target overall**
- Cleaner structure than DuckDuckGo
- Fewer ads than both DuckDuckGo and Google
- Less aggressive anti-bot measures
- Stable HTML structure

### 2. DuckDuckGo (html.duckduckgo.com)

**HTML Structure**: ⭐⭐⭐⭐ (Good)
- Static HTML version specifically designed for simplicity
- "Lightweight HTML"
- "Straightforward parsing"
- No JavaScript required
- Clean CSS selectors

**Ad Presence**: ⭐⭐⭐⭐⭐ (Excellent)
- **Minimal ads** - fewest of any major search engine
- "DuckDuckGo offers a very clean interface with minimal ads"
- Ads are non-personalized, easier to identify
- Can remove ads with `k1=-1` parameter
- Best ad-to-content ratio

**Anti-Scraping**: ⭐⭐ (Challenging)
- **CAPTCHA challenges** - "Select all squares containing a duck"
- Both html.duckduckgo.com and lite.duckduckgo.com trigger CAPTCHA
- **VQD value sensitivity** - "Wrong vqd value leads to IP block list"
- **1-hour IP cooldown** after triggering bot detection
- "Flags repetitive patterns in query timing, header inconsistencies, or IP rotation"
- Residential IPs: 94% success rate
- Datacenter IPs: 61% success rate

**Rate Limiting**: ⭐⭐ (Strict)
- Informal ~20 requests/second limit
- Sensitive to request patterns
- IP-based blocking with sliding window
- Requires 1-hour cooldown period
- "No formal documentation available"

**HTML Stability**: ⭐⭐⭐⭐⭐ (Excellent)
- html.duckduckgo.com is specifically static
- Minimal changes over time
- Designed for lightweight clients
- Highly predictable structure

**Legal/ToS**: ⚠️ (Gray Area)
- No official API for full search results
- Instant Answer API exists but limited
- Python libraries exist (duckduckgo-search) but noted "not affiliated"
- "Educational purposes only, not for commercial use"

**Unique Challenges**:
- **No personalization** = consistent results (good for testing)
- **Bot detection** = requires proper User-Agent, headers
- **VQD value handling** = complex session management
- **CAPTCHA** = major barrier for automated access

**Verdict**: **Good but challenging**
- Best ad situation (minimal/none)
- Excellent HTML structure
- BUT: Aggressive anti-scraping makes it difficult
- Requires sophisticated bot evasion techniques

### 3. DuckDuckGo (lite.duckduckgo.com)

**HTML Structure**: ⭐⭐⭐⭐⭐ (Excellent)
- Even simpler than html.duckduckgo.com
- "Lightweight version"
- Minimal markup

**Anti-Scraping**: ⭐⭐ (Challenging)
- Same CAPTCHA challenges as html version
- Same VQD sensitivity
- Same IP blocking behavior
- No advantage over html version

**Verdict**: **No benefit over html.duckduckgo.com**
- Same anti-scraping measures
- Simpler HTML but both are already simple
- Use html.duckduckgo.com instead

### 4. Google

**HTML Structure**: ⭐ (Poor)
- "Volatile, highly personalized"
- Complex, dynamic structure
- Heavy JavaScript usage
- Difficult CSS selectors

**Ad Presence**: ⭐⭐ (Poor)
- Most ads of any search engine
- Ads blend with organic results
- Harder to identify and filter
- "Advanced anti-bot systems"

**Anti-Scraping**: ⭐ (Very Difficult)
- "Protected by advanced anti-bot systems"
- Aggressive CAPTCHA
- Sophisticated fingerprinting
- IP banning
- JavaScript challenges

**Rate Limiting**: ⭐ (Strict)
- Very strict
- Quick to ban IPs
- Requires rotating proxies

**HTML Stability**: ⭐ (Poor)
- Frequent changes
- A/B testing variations
- Personalization makes structure unpredictable

**Legal/ToS**: ⚠️ (Explicitly Prohibited)
- ToS explicitly prohibit scraping
- Google actively enforces
- Legal risks higher

**Verdict**: **Worst option - avoid**
- Most complex structure
- Most aggressive anti-scraping
- Highest legal risk
- "Raises cost, legal risk, and maintenance burden"

### 5. Startpage

**HTML Structure**: ⭐⭐⭐ (Moderate)
- Uses Google results
- Likely similar complexity to Google
- Limited documentation available

**Ad Presence**: ⭐⭐⭐ (Moderate)
- "Ad-light interface"
- Fewer than Google
- Non-personalized
- "Ad presence varies by query type"

**Anti-Scraping**: ⭐⭐⭐ (Unknown)
- Limited information available
- Acts as Google proxy
- May inherit Google's anti-scraping

**Legal/ToS**: ⚠️ (Unclear)
- As Google proxy, unclear legal stance
- Privacy-focused but may restrict scraping

**Verdict**: **Not recommended**
- Limited technical information
- Unclear benefits over Bing or DuckDuckGo
- Possibly inherits Google complexity

### 6. Brave Search (Web Interface Scraping)

**Note**: Brave has an official API (recommended), but we're evaluating web scraping here.

**HTML Structure**: ⭐⭐⭐ (Moderate)
- Selectors: `.snippet` for results
- `.ml-15` for Next button
- `offset` parameter for pagination
- Requires BeautifulSoup/similar

**Ad Presence**: ⭐⭐⭐⭐ (Good)
- Privacy-focused
- Fewer ads than Google/Bing
- Clean interface

**Anti-Scraping**: ⭐⭐⭐ (Moderate)
- Has official API, so scraping discouraged
- Unknown enforcement level
- Likely moderate protection

**Verdict**: **Use official API instead**
- Official API available (2,000/month free)
- Scraping is unnecessary and discouraged
- No reason to scrape when API exists

## Detailed Anti-Scraping Analysis

### DuckDuckGo Anti-Bot Mechanisms

1. **CAPTCHA Challenges**
   - Image-based: "Select all squares containing a duck"
   - Triggers on automated access
   - Both html and lite versions affected

2. **VQD Value Tracking**
   - Query-specific token
   - Wrong value → IP block list
   - Requires session management
   - Sliding 1-hour window

3. **IP Reputation**
   - Residential IPs: 94% success
   - Datacenter IPs: 61% success
   - Clear preference for residential

4. **Request Pattern Analysis**
   - "Flags repetitive patterns in query timing"
   - "Header inconsistencies"
   - "IP rotation anomalies"
   - Minimal JavaScript reliance (network-level detection)

5. **Cooldown Periods**
   - 1-hour IP block after triggering detection
   - Sliding window (behavior-based)

### Bing Anti-Bot Mechanisms

1. **Lenient Detection**
   - "Fewer anti-bot headaches than Google"
   - Less aggressive than competitors

2. **Adaptation Tolerance**
   - "AI-powered tools can adapt automatically"
   - More forgiving of variations

3. **Unknown Specifics**
   - Limited public documentation
   - Appears to focus on extreme abuse only

## Ad Identification & Filtering

### Bing Ads

**Structure**:
- Clearly marked sponsored results
- Distinct from organic results
- "Fewer ads at top of results"
- More space for organic content

**Filtering Strategy**:
- CSS selectors for sponsored sections
- Look for "Ad" or "Sponsored" labels
- Usually in separate containers

### DuckDuckGo Ads

**Structure**:
- "Minimal ads"
- "Non-personalized" (based on query only)
- Can disable with `k1=-1` parameter

**Filtering Strategy**:
- Fewest ads to filter
- Clear separation from organic
- Parameter-based removal available

### Google Ads

**Structure**:
- Blend with organic results
- Harder to identify programmatically
- Multiple ad formats

**Filtering Strategy**:
- Complex, requires sophisticated detection
- Frequently changing selectors

## Scraping Success Factors

### Critical Requirements

1. **User-Agent Header**
   - Must appear as real browser
   - `Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36...`
   - DuckDuckGo particularly sensitive

2. **Request Headers**
   - Accept, Accept-Language, Accept-Encoding
   - Referer (sometimes)
   - Cookie handling

3. **IP Address Type**
   - Residential > Datacenter
   - DuckDuckGo: 94% vs 61% success rate
   - May require proxy rotation

4. **Request Pacing**
   - DuckDuckGo: < 1 req/sec recommended
   - Bing: More tolerant
   - Add random delays

5. **Session Management** (DuckDuckGo only)
   - VQD value tracking
   - Cookie persistence
   - Complex implementation

## Implementation Complexity

### Bing (Simplest)

```c
// 1. Build URL with query
sprintf(url, "https://www.bing.com/search?q=%s", encoded_query);

// 2. Set User-Agent header
curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0...");

// 3. GET request
curl_easy_perform(curl);

// 4. Parse HTML with simple selectors
// - Result containers: .b_algo
// - Title: h2 a
// - URL: cite
// - Snippet: .b_caption p

// 5. Filter ads
// - Look for .b_ad class or similar
```

**Complexity**: Low
**Maintenance**: Low (stable structure)

### DuckDuckGo (Complex)

```c
// 1. Build URL
sprintf(url, "https://html.duckduckgo.com/html/?q=%s", encoded_query);

// 2. Set comprehensive headers
curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0...");
// ... many other headers

// 3. Handle VQD value
// - Extract from previous response
// - Include in subsequent requests
// - Manage session state

// 4. Implement backoff on CAPTCHA
// - Detect CAPTCHA page
// - Cooldown for 1 hour
// - Rotate IP if available

// 5. Parse HTML
// - Result containers: .result
// - Ads: minimal, can use k1=-1

// 6. Handle errors
// - 202 Ratelimit responses
// - IP blocking
// - CAPTCHA challenges
```

**Complexity**: High
**Maintenance**: Medium (stable HTML but complex anti-bot)

## Recommendation

### Primary: Bing

**Why**:
1. ✅ **Cleanest HTML structure** - "cleaner than Google"
2. ✅ **Fewest anti-bot measures** - "fewer headaches"
3. ✅ **Fewer ads** - better organic content ratio
4. ✅ **Stable structure** - less frequent changes
5. ✅ **Simple implementation** - standard requests work
6. ✅ **Good result quality** - less SEO spam

**Tradeoffs**:
- ❌ Official API retired (Aug 2025)
- ❌ ToS prohibits scraping (standard, minimal enforcement)
- ❌ Still unofficial/unsupported

**Implementation**: Low complexity, high success rate

### Alternative: DuckDuckGo (html.duckduckgo.com)

**Why**:
1. ✅ **Minimal ads** - best ad situation
2. ✅ **Static HTML** - very stable
3. ✅ **Privacy-focused** - no personalization
4. ✅ **Simple structure** - when you can access it

**Tradeoffs**:
- ❌ **Aggressive anti-scraping** - CAPTCHA, VQD, IP blocking
- ❌ **Complex session management** - VQD value handling
- ❌ **1-hour cooldowns** - after triggering detection
- ❌ **IP type sensitive** - datacenter IPs only 61% success

**Implementation**: High complexity, moderate success rate

### Not Recommended: Google

**Why Not**:
- ❌ Most complex HTML structure
- ❌ Most aggressive anti-bot
- ❌ Highest legal risk
- ❌ Frequent structure changes
- ❌ Most ads to filter

## Final Answer to Original Question

**Best service for scraping: Bing**

**Reasoning**:
1. Bing offers the best balance of:
   - **Simplicity** (clean HTML)
   - **Accessibility** (fewer anti-bot measures)
   - **Quality** (fewer ads, good results)
   - **Stability** (predictable structure)

2. DuckDuckGo is a close second, but only if you can handle:
   - CAPTCHA challenges
   - VQD session management
   - IP-based blocking
   - 1-hour cooldown periods

3. For ikigai rel-08:
   - **Don't scrape at all** - use Brave's official API
   - **If forced to scrape**: Implement Bing, not DuckDuckGo
   - **Complexity vs. benefit**: Bing wins for maintainability

## Implementation Recommendation for rel-08

### Option 1: No Scraping (Recommended)

Use Brave's official API:
- 2,000/month free
- Official, stable, supported
- No anti-bot issues
- Clear rate limits
- Structured JSON responses

### Option 2: Bing Scraping (If absolutely needed)

If scraping is required for zero-API-key fallback:

```c
// Simple Bing scraping implementation
1. GET https://www.bing.com/search?q=<query>
2. User-Agent: Standard browser string
3. Parse HTML: .b_algo containers
4. Filter ads: .b_ad class
5. Extract: title (h2 a), URL (cite), snippet (.b_caption p)
```

**Advantages**:
- Simple implementation
- High success rate
- Fewer anti-bot issues

**Disadvantages**:
- Still unofficial
- ToS technically prohibit it
- Could break on structure changes

### Option 3: DuckDuckGo Scraping (Not recommended)

Only if Bing fails and official APIs unavailable:

```c
// Complex DuckDuckGo implementation
1. Manage VQD values across requests
2. Implement CAPTCHA detection and backoff
3. Handle IP blocking with 1-hour cooldowns
4. Use residential IPs or proxies
5. Sophisticated error handling
```

**Conclusion**: Too complex for marginal benefit over Bing.

## Sources

- [Top Google Alternatives for Web Scraping in 2025 | ScrapingAnt](https://scrapingant.com/blog/google-web-scraping-alternatives)
- [How to Scrape DuckDuckGo SERP Data: 4 Proven Methods - Bright Data](https://brightdata.com/blog/web-data/how-to-scrape-duckduckgo)
- [How to Scrape Bing Search Results in 2025: A Simple Guide](https://thunderbit.com/blog/scrape-bing-search-results-guide)
- [Ultimate Guide to Scraping Bing - Scrape.do](https://scrape.do/blog/bing-scraping/)
- [DuckDuckGo Engines — SearXNG Documentation](https://docs.searxng.org/dev/engines/online/duckduckgo.html)
- [Search Engine Scraping Tutorial - ScrapingBee](https://www.scrapingbee.com/blog/search-engine-scraping/)
- [DuckDuckGo vs. Google: An In-Depth Comparison](https://www.searchenginejournal.com/google-vs-duckduckgo/301997/)
- [Detailed tests of search engines - LibreTechTips](https://libretechtips.gitlab.io/detailed-tests-of-search-engines-google-startpage-bing-duckduckgo-metager-ecosia-swisscows-searx-qwant-yandex-and-mojeek/)
- [duckduckgo blocked by CAPTCHA · Issue #3927](https://github.com/searxng/searxng/issues/3927)
- [duckduckgo-search Python library](https://pypi.org/project/duckduckgo-search/)
