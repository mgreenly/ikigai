## Overview

Configuration management for the ikigai application. Loads and parses a JSON configuration file containing OpenAI settings, server settings, and database parameters. Handles file creation with sensible defaults, tilde expansion for home directory references, and comprehensive validation of all configuration values.

## Code

```
function expand_tilde(path):
    if path does not start with tilde:
        return the path unchanged

    retrieve the HOME environment variable
    if HOME is not set:
        return an error indicating HOME is required for tilde expansion

    replace the tilde with the HOME directory path
    return the expanded path

function create_default_config(path):
    extract the directory from the config file path

    verify the directory exists
    if directory does not exist:
        create it with 0755 permissions
        if directory creation fails:
            return an error with the failure reason

    create a new JSON document with a talloc-based memory allocator
    create a root object in the document

    populate the root object with default values:
        openai_api_key: "YOUR_API_KEY_HERE"
        openai_model: "gpt-5-mini"
        openai_temperature: 1.0
        openai_max_completion_tokens: 4096
        openai_system_message: null
        listen_address: "127.0.0.1"
        listen_port: 1984
        max_tool_turns: 50
        max_output_size: 1048576 (1 MB)
        history_size: 10000

    write the document to the file with pretty-printing
    if write fails:
        return an error with the failure reason

    return success

function load_config(path):
    expand tilde in path to get the actual file path

    check if the config file exists
    if file does not exist:
        create a new config file with default values
        if creation fails:
            return the creation error

    load and parse the JSON file with a talloc-based memory allocator
    if parsing fails:
        return a parse error with details

    verify the JSON root is an object
    if not an object:
        return a parse error

    allocate a new config structure

    extract all configuration values from the JSON:
        openai_api_key (required, string)
        openai_model (required, string)
        openai_temperature (required, number)
        openai_max_completion_tokens (required, integer)
        openai_system_message (optional, string or null)
        listen_address (required, string)
        listen_port (required, integer)
        db_connection_string (optional, string)
        max_tool_turns (required, integer)
        max_output_size (required, integer)
        history_size (optional, integer, defaults to 10000)

    validate openai_api_key:
        must be present and a string

    validate openai_model:
        must be present and a string

    validate openai_temperature:
        must be present and a number
        must be in range 0.0 to 2.0
        if out of range:
            return a range error

    validate openai_max_completion_tokens:
        must be present and an integer
        must be in range 1 to 128000
        if out of range:
            return a range error

    validate openai_system_message:
        if present, must be null or a string

    validate listen_address:
        must be present and a string

    validate listen_port:
        must be present and an integer
        must be in range 1024 to 65535
        if out of range:
            return a range error

    validate db_connection_string:
        if present, must be a string
        empty strings are treated as "no database" (memory-only mode)

    validate max_tool_turns:
        must be present and an integer
        must be in range 1 to 1000
        if out of range:
            return a range error

    validate max_output_size:
        must be present and an integer
        must be in range 1024 to 104857600 bytes (100 MB)
        if out of range:
            return a range error

    validate history_size:
        if present, must be an integer in range 1 to INT32_MAX
        if out of range:
            return a range error
        if not present, use default value 10000

    copy all validated values into the config structure:
        allocate string storage for string fields
        handle optional fields appropriately
        convert numeric types to their final storage types

    return success with the populated config structure
```
