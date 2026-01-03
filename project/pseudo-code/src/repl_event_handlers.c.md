## Overview

This file handles events and I/O multiplexing for the REPL. It manages terminal input processing, HTTP request/response handling for LLM agents, tool execution completion, spinner timeouts, and scroll detection. The module coordinates between the select-based event loop and various async operations including network requests and background tool execution.

## Code

```
function calculate_select_timeout_ms(repl, curl_timeout_ms):
    collect all timeout sources:
        - spinner animation needs update every 80ms if visible
        - check if any agent is executing a tool, need 50ms polling if so
        - get scroll detector timeout if scroll detection is active
        - include curl's own timeout requirement

    find the minimum positive timeout from all sources

    if all timeouts are inactive (no pending operations):
        return default 1000ms timeout
    else:
        return the smallest timeout value


function setup_fd_sets(repl, read_fds, write_fds, exc_fds):
    clear all three file descriptor sets

    add terminal file descriptor to read set
    track max file descriptor number

    for each agent:
        ask the HTTP client to add its file descriptors to the sets
        track the highest file descriptor seen

    return the maximum file descriptor number and success


function handle_terminal_input(repl, terminal_fd):
    try to read one byte from terminal

    if read failed with signal interrupt:
        return success (ignore and retry)

    if read reached EOF (user closed terminal):
        signal that REPL should exit
        return success

    record current timestamp for performance tracking

    parse the byte through the input state machine

    process the resulting input action

    if we recognized a known action (not unknown/incomplete):
        render the new frame to show changes

    return success


function persist_assistant_msg(repl):
    if database not configured or session not active:
        return early

    build a JSON object with assistant response metadata:
        - the LLM model name (if available)
        - token usage count (if available)
        - finish reason from LLM (if available)

    insert a message record into the database:
        - session ID
        - message UUID
        - "assistant" as the role
        - response content
        - metadata JSON

    cleanup allocated strings

    ignore database errors (just log)


function handle_agent_request_error(repl, agent):
    format the HTTP error message with "Error: " prefix

    append the error message to the agent's scrollback display

    cleanup the error message from the agent context

    if there's a partial assistant response, discard it

    reset the response buffer


function handle_agent_request_success(repl, agent):
    if we received assistant response text:
        create a new assistant message in the conversation
        add it to the conversation history
        persist it to the database

    cleanup the response buffer

    if there's a pending tool call:
        start executing the tool
        return (this will continue the loop when tool completes)

    if the conversation indicates more tool calls are needed:
        increment the tool iteration counter
        submit the next request to the LLM

    if no tool calls pending and conversation complete:
        agent transitions to idle


function submit_tool_loop_continuation(repl, agent):
    check if we've hit the maximum tool turn limit

    create a new request to the LLM with callbacks:
        - streaming callback to handle partial responses
        - completion callback to handle response finished
        - flag indicating if this is the final allowed turn

    if request submission failed:
        append error message to scrollback
        transition agent to idle
        cleanup error
    else:
        mark that HTTP request is in flight


function process_agent_curl_events(repl, agent):
    if agent has an HTTP request in flight:
        perform one iteration of HTTP processing (read/write/parse)
        collect any completed HTTP responses

        check if the HTTP request just completed (was running, now idle):
            if there was an HTTP error:
                handle the error (display it)
            else:
                handle the success (process response)

            check the agent state again

            if agent is still waiting for LLM (no tool started):
                transition to idle

        if this is the current agent:
            render the frame to show any changes


function handle_curl_events(repl):
    for each agent:
        process its curl/HTTP events

    if there's a current agent (focused agent):
        if it's not in the agents array:
            process its events separately
            (handles edge case where current differs from managed agents)

    return success


function handle_agent_tool_completion(repl, agent):
    mark tool execution as complete and collect results

    if the conversation needs more tool calls:
        increment the tool iteration counter
        submit the continuation request to LLM
    else:
        transition agent to idle

    if this is the currently focused agent:
        render the frame to update the display


function handle_tool_completion(repl):
    delegate to handle_agent_tool_completion for the current agent


function calculate_curl_min_timeout(repl):
    find the minimum timeout needed by all agents' HTTP clients

    iterate through each agent:
        ask the HTTP client for its timeout requirement
        track the minimum positive timeout

    return the minimum timeout (or -1 if no timeout needed)


function handle_select_timeout(repl):
    if spinner is currently visible:
        advance the spinner animation frame
        render the updated frame

    if scroll detection is active:
        get current time
        check if scroll timeout has fired

        if timeout indicates scroll up:
            execute arrow-up action
            render frame
        else if timeout indicates scroll down:
            execute arrow-down action
            render frame

    return success


function poll_tool_completions(repl):
    check each agent's background tool thread:
        lock the agent's state mutex
        read the agent state and completion flag
        unlock mutex

        if tool execution is in progress and thread has completed:
            handle the tool completion (process results)

    if we have a current agent but no agents array:
        perform the same check on current agent directly

    return success
```
