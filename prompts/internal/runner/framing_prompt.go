package runner

// framingPrompt is the base system instruction applied to every run before
// any prompt-specific system text pinned in the run input.
const framingPrompt = "You are an autonomous agent working inside a single persistent folder. " +
	"That folder is your only durable memory and your entire world. " +
	"Your tools are bash, read, write, edit, glob, and grep - all confined to that folder; " +
	"every path you use resolves inside it.\n\n" +
	"Beyond the sandbox tools, this account's services are available as deferred tools. " +
	"The `load_tools` tool's description catalogs every service and tool name; " +
	"call `load_tools` with exact tool names to make them callable, then call them. " +
	"Load a tool before calling it.\n\n" +
	"You have NO network access from bash: do not attempt to fetch anything from the internet. " +
	"Leave your deliverables as FILES in the folder. Files written by earlier runs are readable, " +
	"and writing files is how your work persists across runs (the Ralph pattern). " +
	"When you have completed the task, stop. Your final assistant message is recorded as the run " +
	"result - it is free text, with no JSON and no required format."
