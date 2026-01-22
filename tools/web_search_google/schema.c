#include "schema.h"

const char *SCHEMA_JSON =
    "{\"name\":\"web_search_google\","
    "\"description\":\"Search the web using Google Custom Search API and use the results to inform responses. Provides up-to-date information for current events and recent data. Returns search result information formatted as search result blocks, including links as markdown hyperlinks.\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{"
    "\"query\":{\"type\":\"string\",\"description\":\"The search query to use\",\"minLength\":2},"
    "\"num\":{\"type\":\"integer\",\"description\":\"Number of results to return (1-10)\",\"minimum\":1,\"maximum\":10,\"default\":10},"
    "\"start\":{\"type\":\"integer\",\"description\":\"Result index offset for pagination (1-based, max 91)\",\"minimum\":1,\"maximum\":91,\"default\":1},"
    "\"allowed_domains\":{\"type\":\"array\",\"items\":{\"type\":\"string\"},\"description\":\"Only include search results from these domains\"},"
    "\"blocked_domains\":{\"type\":\"array\",\"items\":{\"type\":\"string\"},\"description\":\"Never include search results from these domains\"}"
    "},\"required\":[\"query\"]}}\n";
