# Multi-View Terminal

## Summary

Ikigai should evolve from a chat-first terminal into a multi-view terminal UI for an orchestration system. Conversation remains the default view, but it becomes one view among many.

## Core Idea

Ikigai is not just a terminal for talking to an agent. It is the user interface for a broader agent orchestration system. The terminal should let users switch between views that expose different parts of system state.

Examples:
- Conversation view
- Agents view
- Tasks view
- Files view
- Diffs view
- Jobs view
- Documents view
- Tools view
- Queue / inbox view
- Context / memory view

This addresses several UX gaps in a unified way:
- better visibility into sub-agents and process trees
- better navigation across files, diffs, and recent work
- better observability for background jobs and tool execution
- reduced transcript overload by separating inspection from conversation
- clearer access to orchestration state beyond the chat stream

## Terminology

Use **views** as the primary term.

Preferred phrasing:
- Ikigai offers many views
- switch to the agents view
- switch to the conversation view
- switch to the tasks view

Avoid leading with **modes** because it suggests vim-like insert/normal mode behavior, keybinding traps, and hard modal state. The concept is closer to navigable views over shared system state.

## Product Positioning

Useful one-line framing:

**A terminal UI for agent orchestration, where every part of the system has its own view.**

Earlier narrower variant that still works for coding-focused messaging:

**A terminal where agents, files, tasks, and diffs each have their own view.**

## Design Direction

The interaction model should stay conversation-first, but make side inspection fast and keyboard-friendly. Views should help users understand the system, not replace the simplicity of chatting.

A good model is:
- conversation as the default home
- fast switching into other views
- easy return to conversation
- object-centric navigation between related things like agents, files, jobs, and diffs

Views are a way to make Ikigai's deeper orchestration architecture visible and usable rather than flattening everything into one transcript.
