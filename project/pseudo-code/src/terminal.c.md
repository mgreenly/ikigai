## Overview

The terminal module manages raw mode and alternate screen buffer operations for terminal-based UI. It handles initialization of the terminal in raw mode with CSI u extended key support, terminal size queries, and cleanup to restore the original terminal state.

## Code

```
function probe_csi_u_support(tty file descriptor):
    send CSI u query escape sequence to terminal

    set up file descriptor set for reading
    set 100ms timeout
    wait for response with select()

    if no response (timeout or error):
        return false (no CSI u support)

    read response from terminal
    if read fails:
        return false

    check if response matches ESC[?...u pattern
    if pattern found:
        return true (CSI u is supported)

    return false


function initialize terminal (context, output context pointer):
    validate inputs (context and output pointer not null)

    open /dev/tty for reading and writing
    if open fails:
        return error (IO error)

    allocate terminal context structure
    store file descriptor in context

    retrieve current terminal settings
    if retrieval fails:
        close file descriptor
        return error (IO error)

    save original terminal settings for later restoration

    configure raw mode:
        disable input processing flags (BRKINT, ICRNL, INPCK, ISTRIP, IXON)
        disable output processing (OPOST)
        enable 8-bit character size (CS8)
        disable echo, canonical mode, extended input, signal generation
        set minimum read to 1 character
        set read timeout to 0 (blocking)

    apply raw mode settings immediately
    if application fails:
        close file descriptor
        return error (IO error)

    flush any stale input queued before raw mode was entered
    if flush fails:
        restore original terminal settings
        close file descriptor
        return error (IO error)

    send alternate screen enter sequence to terminal
    if send fails:
        restore original terminal settings
        close file descriptor
        return error (IO error)

    probe for CSI u support in terminal
    if supported:
        enable CSI u with flag 9 (disambiguate keys + report all keys)
        if enable fails:
            mark CSI u as unsupported (non-critical failure)

    query terminal for current size (rows and columns)
    if query fails:
        exit alternate screen buffer
        restore original terminal settings
        close file descriptor
        return error (IO error)

    store terminal dimensions in context

    return success with terminal context


function cleanup terminal (context):
    validate context is not null

    if CSI u was enabled:
        send CSI u disable sequence to terminal

    send alternate screen exit sequence to terminal

    restore original terminal settings

    flush any remaining input in buffer

    close terminal file descriptor

    note: caller is responsible for freeing the context


function get terminal size (context, rows output, cols output):
    validate all inputs (context and output pointers not null)

    query terminal for window size
    if query fails:
        return error (IO error)

    update stored terminal dimensions

    copy dimensions to output parameters

    return success
```
