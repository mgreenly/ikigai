# AI Providers

ikigai supports three AI providers. Each requires an API key set via environment variable or `credentials.json`.

| Provider | Environment Variable | Sign Up | Details |
|----------|---------------------|---------|---------|
| Anthropic | `ANTHROPIC_API_KEY` | [console.anthropic.com](https://console.anthropic.com/settings/keys) | [Anthropic models](anthropic.md) |
| OpenAI | `OPENAI_API_KEY` | [platform.openai.com](https://platform.openai.com/api-keys) | [OpenAI models](openai.md) |
| Google | `GOOGLE_API_KEY` | [aistudio.google.com](https://aistudio.google.com/app/apikey) | [Google models](gemini.md) |

## Reasoning Levels

ikigai uses four universal reasoning levels across all providers:

| Level | Meaning |
|-------|---------|
| `min` | Minimal or disabled reasoning |
| `low` | Light reasoning |
| `med` | Moderate reasoning |
| `high` | Maximum reasoning |

Each provider maps these levels differently — some use token budgets, others use effort strings. See the individual provider pages for exact mappings per model.

## Model Support Policy

Once ikigai adds support for a model, it remains supported until the provider deprecates it from their API.
