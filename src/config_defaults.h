#ifndef IK_CONFIG_DEFAULTS_H
#define IK_CONFIG_DEFAULTS_H

// Default configuration values
// Single source of truth for all config defaults

#define IK_DEFAULT_OPENAI_MODEL "gpt-5-mini"
#define IK_DEFAULT_OPENAI_TEMPERATURE 1.0
#define IK_DEFAULT_OPENAI_MAX_COMPLETION_TOKENS 4096
#define IK_DEFAULT_OPENAI_SYSTEM_MESSAGE "You are a personal agent and are operating inside the Ikigai orchestration platform."
#define IK_DEFAULT_LISTEN_ADDRESS "127.0.0.1"
#define IK_DEFAULT_LISTEN_PORT 1984
#define IK_DEFAULT_MAX_TOOL_TURNS 50
#define IK_DEFAULT_MAX_OUTPUT_SIZE 1048576
#define IK_DEFAULT_HISTORY_SIZE 10000
#define IK_DEFAULT_PROVIDER "openai"
#define IK_DEFAULT_DB_HOST "localhost"
#define IK_DEFAULT_DB_PORT 5432
#define IK_DEFAULT_DB_NAME "ikigai"
#define IK_DEFAULT_DB_USER "ikigai"

#endif // IK_CONFIG_DEFAULTS_H
