# Considerations

Candidate features and changes worth remembering. These are not planned or scheduled, but represent ideas that may be valuable to revisit in the future.

## Separate Credentials from Configuration

**Context**: Currently all configuration lives in `~/.config/ikigai/config.json`, including sensitive API keys mixed with non-sensitive settings like model names and temperature values.

**Consideration**: Split credentials into a separate file (e.g., `~/.config/ikigai/credentials.json`) that contains only sensitive values:
- API keys for various providers
- Database connection strings with passwords
- Any other secrets

**Benefits**:
- Users can share their config.json as examples without exposing credentials
- Documentation can show complete config examples without needing to redact
- Clearer separation between "settings I might tweak" vs "secrets I must protect"
- Easier to set appropriate file permissions on credentials file (0600)
- Better security hygiene by default

**Implementation notes**:
- Load credentials separately from config
- Fall back to checking config.json for backward compatibility
- Auto-migrate credentials on first run if found in config.json
