#include "auth_error.h"

#include <stdio.h>

void write_auth_error_json(void)
{
    printf("{\n");
    printf("  \"success\": false,\n");
    printf(
        "  \"error\": \"Web search requires API key configuration.\\n\\nBrave Search offers 2,000 free searches/month.\\nGet your key: https://brave.com/search/api/\\nAdd to: ~/.config/ikigai/credentials.json as 'web_search.brave.api_key'\",\n");
    printf("  \"error_code\": \"AUTH_MISSING\",\n");
    printf("  \"_event\": {\n");
    printf("    \"kind\": \"config_required\",\n");
    printf(
        "    \"content\": \"⚠ Configuration Required\\n\\nWeb search needs an API key. Brave Search offers 2,000 free searches/month.\\n\\nGet your key: https://brave.com/search/api/\\nAdd to: ~/.config/ikigai/credentials.json\\n\\nExample:\\n{\\n  \\\"web_search\\\": {\\n    \\\"brave\\\": {\\n      \\\"api_key\\\": \\\"your-api-key-here\\\"\\n    }\\n  }\\n}\",\n");
    printf("    \"data\": {\n");
    printf("      \"tool\": \"web_search\",\n");
    printf("      \"credential\": \"api_key\",\n");
    printf("      \"signup_url\": \"https://brave.com/search/api/\"\n");
    printf("    }\n");
    printf("  }\n");
    printf("}\n");
}
