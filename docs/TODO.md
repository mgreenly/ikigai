
docs/backlog/readline-features.md

quality checks

Refactor

quality checks

Clean up docs

CHANGELOG

do rel-04

------------ REL-05 --------------------------------

separate credentials from config

primary focus sub-agents and concurrency.

enhance logging to support agent ids, session ids, etc...

can't change the system message because the config overwrites it when it loads


------------ NEW ideas --------------------------------

A /chain command that let's you start a series of prompts, each starting on fresh context after the successful completion of the previous step.

A /loop command that let's you start the same prompt if it is still running when it stops becuase of context exhastion.

Context should de-deuplicate
