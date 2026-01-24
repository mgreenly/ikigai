Call chain:
  send_to_llm_() (repl_actions_llm.c:84)
    → ik_agent_get_provider() (agent_provider.c:97)
      → ik_provider_create() (providers/factory.c:62)
        → ik_credentials_load() (credentials.c:112)
          → fprintf(stderr, ...) ← BYPASSES ALTERNATE BUFFER!
