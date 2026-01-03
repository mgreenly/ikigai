## Overview

Provides helper functions for creating and serializing tool choice values in OpenAI API requests. Tool choice controls whether the model can, must, or must use a specific tool. This module supports four modes: auto (model decides), none (no tools), required (must use a tool), and specific (use a named tool).

## Code

```
function tool_choice_auto():
    create a tool choice in auto mode
    return it

function tool_choice_none():
    create a tool choice in none mode
    return it

function tool_choice_required():
    create a tool choice in required mode
    return it

function tool_choice_specific(parent, tool_name):
    validate parent context exists
    validate tool name is provided

    create a tool choice in specific mode
    allocate and store the tool name (owned by parent context)
    if allocation fails, panic
    return the tool choice

function tool_choice_serialize(json_doc, json_obj, key, choice):
    validate all parameters exist

    if choice mode is auto:
        add string "auto" to JSON object
        if add fails, panic

    else if choice mode is none:
        add string "none" to JSON object
        if add fails, panic

    else if choice mode is required:
        add string "required" to JSON object
        if add fails, panic

    else if choice mode is specific:
        validate tool name exists

        create JSON object for tool choice
        if creation fails, panic

        add field "type" with value "function" to tool choice object
        if add fails, panic

        create JSON object for function details
        if creation fails, panic

        add field "name" with tool name to function object
        if add fails, panic

        add function object to tool choice object as "function" field
        if add fails, panic

        add tool choice object to parent JSON object with specified key
        if add fails, panic

    else:
        panic on invalid mode
```
