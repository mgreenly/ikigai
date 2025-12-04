What is Ikigai

Amplification through agents, compounding over time

The Short Version

Ikigai helps you build intelligent agents that amplify what your team can do.

Instead of only building your product directly, you also build a system of agents that learns your codebase, your infrastructure, and your workflows. These agents don’t just autocomplete code — they become durable capabilities that understand your project and improve over time.

You invest in teaching and refining this system. At first, it might feel like a 2× multiplier. As the agents accumulate context and experience, that leverage grows: 5×, then 10×, then more. Your effort compounds because you’re not just building features; you’re building the capability to build and operate features.

The Problem

You have an ambitious idea: a new service, a complex platform, a real-world system. You can see where it needs to go. But there are only so many hours and only so many people.

The usual options are limiting:

Do everything yourself
Every feature, deployment, and on-call incident routes through you. You become the bottleneck.

Hire a larger team
Slow to spin up, expensive to maintain, and you spend more time coordinating than creating.

Glue together existing tools
You assemble a stack of services, each with its own dashboards, configs, and quirks. You spend a large fraction of your time wiring systems together instead of moving your core idea forward.

What if your project came with its own “operational brain”? Not a passive assistant that waits for instructions, but agents that understand your system, act within clear boundaries, ask for guidance when needed, and get better every week?

Core Concepts

Ikigai is a platform for building and running these agent systems. It introduces a few key ideas:

Project
A meta-project that represents the brains behind a real system (for example, “Videos.com Control Plane”).

Control Plane
The environment where your agents run. It knows about your repositories, environments, deployments, and policies.

Agents
Long-lived, specialized capabilities: deployment agents, incident agents, moderation agents, and so on. They act, observe, and improve.

Target System
The actual product or infrastructure you’re building and running. The control plane manages it; the agents work on and around it.

Ikigai is the place where all of this comes together.

How It Works
1. Create the Meta-Project

You start in the Ikigai terminal by creating a new project. If you’re building “Videos.com,” the project you create in Ikigai is not Videos.com itself. It’s the control plane for Videos.com: the collection of agents that help you build, deploy, monitor, and operate the real service.

2. Dual Building: Product and Agents Together

From day one, you build the target system and its agents in parallel.

Working in the control plane, you teach Ikigai where your code lives, how it’s built, how environments are structured, and what “healthy” looks like. You might:

Define how to deploy the video API

Show the agent how to run tests and smoke checks

Describe what counts as an incident

You’re not just writing the video player; you’re also shaping the agent that knows how to ship and maintain the video player. As the product grows, the agents accumulate more context and responsibility.

3. Running the Control Plane

Like any software, the control plane needs somewhere to run. You can:

Run it locally while you’re iterating, or

Deploy it to a server so agents can act continuously.

Sometimes, the control plane is the final interface (for internal tools, research projects, etc.). For externally-facing products, the control plane sits behind the scenes, managing the live system.

Where the agents run and what they manage are independent. You can run agents close to your infrastructure, or keep them local during early development.

4. Collaborative Development with Agents

Development becomes a conversation at the project level.

You define goals, specs, and constraints. Agents propose:

Code changes

Configuration updates

Test additions

Documentation edits

They open change sets; you review. You run tests together, inspect diffs, and approve or reject. When something is wrong, you don’t just patch the output — you refine the agent’s instructions or training context so it does better next time.

They propose. You approve. Their capabilities grow.

5. Bounded Autonomy, Compounding Leverage

Over time, more work shifts from direct execution to guided automation.

You define:

What agents are allowed to do automatically

What must always be reviewed

When they must escalate and ask you

As you refine these boundaries and improve the agents, tasks that once required hands-on effort start happening automatically: routine deployments, low-severity incidents, recurring maintenance, basic support triage.

Your attention moves up a level: from “how do I do this” to “what should we build next and how should agents handle it?”

6. Continuous Operation and Improvement

There’s no singular launch moment where everything flips from manual to automated. You gradually expand what the agents can handle.

New components can be introduced behind feature flags.

Agents learn to manage new workflows as you add them.

Operational knowledge accumulates as code, policies, and memories inside the control plane.

The result is a system that runs continuously and improves continuously. Every cycle of “agent acts → you review → you refine” makes the control plane more capable.

What Makes Ikigai Different

Most tooling either:

Helps you work faster in the moment, or

Blindly executes whatever you’ve explicitly scripted.

Ikigai is different in a few ways:

Contextful Agents, Not Stateless Tools
Agents maintain an evolving understanding of your project: codebases, environments, incidents, and past decisions.

Partners, Not Shortcuts
They propose meaningful, multi-step changes — not just snippets — and they know when to stop and ask for judgment.

Institutional Memory in Code
Decisions, gotchas, and lessons learned are captured inside the control plane, not lost in chat logs or tribal knowledge.

Bounded Autonomy
You decide where agents can act on their own and where they must seek approval. The system is designed for safety and oversight.

Ikigai agents behave less like macros and more like specialized teammates who share a common environment and history.

A Day with Ikigai (Example)

A typical day might look like:

Morning: You open the control plane and see a summary of overnight activity. The deployment agent shows two successful rollouts and one failed attempt it automatically rolled back and logged for review.

During the day: You describe a new feature. An agent proposes code changes, tests, and updates to configuration. You review, tweak, and approve.

Afternoon: The monitoring agent flags a performance regression, traces it to a specific change, and drafts a mitigation. You review the plan and let it execute.

End of day: You refine one agent’s operating rules based on what you saw. That small improvement applies to every future action it takes.

Your effort goes into directing and teaching, not just doing.

The Deeper Idea

Using Ikigai, you aren’t just building software. You’re building a persistent operational capability around your software.

Traditional development is linear: effort in, output out. When you stop working, progress stops.

Ikigai is designed to be compounding:

You invest effort into teaching and shaping agents.

Agents produce output and handle more of the day-to-day work.

You review and refine them based on what they did.

They come back stronger next time, with more context and better defaults.

An expert in a domain will produce far better agents than a novice. Ikigai doesn’t replace expertise; it amplifies it and makes it durable.

The goal is simple: free builders from the grind of operating everything they’ve built, so they can spend more time on the vision — the “reason for being” — that motivated the project in the first place.

The rest of the documentation dives into technical details: how agents are defined, how the control plane is structured, and how deployments and safety constraints work. The core idea, though, is this: build agents that learn your system, and let that learning compound over time.
