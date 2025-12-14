separate credentials from config

can't change the system message because the config overwrites it when it loads

add a /tool command that shows the tool's available

A /chain command that let's you start a series of prompts, each starting on fresh context after the successful completion of the previous step.

A /loop command that let's you start the same prompt if it is still running when it stops becuase of context exhastion.

Context should de-deuplicate

skill file that are dynamic, when read they actually return up-todate info.

A command that extract all the related context of a given prompt from the history of an agent and record it to a structured memory document that can be referenced.  Example, you've been discussing topic Y and you say /store Y to document X.  This then marks all releative memory as hiden and removes it from the active context.  Sort of like selective compatction.  Thoughts...
