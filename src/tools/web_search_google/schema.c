#include "schema.h"

const char *const SCHEMA_JSON = "{\"name\":\"web_search_google\",\"description\":\"Google search\",\"parameters\":{\"type\":\"object\",\"properties\":{\"query\":{\"type\":\"string\",\"minLength\":2},\"num\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":10,\"default\":10},\"start\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":91,\"default\":1},\"allowed_domains\":{\"type\":\"array\",\"items\":{\"type\":\"string\"}},\"blocked_domains\":{\"type\":\"array\",\"items\":{\"type\":\"string\"}}},\"required\":[\"query\"]}}";
