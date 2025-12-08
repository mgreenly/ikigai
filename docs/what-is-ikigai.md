# What is Ikigai

**Amplification through Agents, Compounding over Time**

---

## The Short Version

Ikigai helps you build intelligent agents that amplify what a small team can do.

Instead of building your product directly, you build a system of agents that learns your codebase, your infrastructure, and your workflows. These agents don't just autocomplete code; they become durable capabilities that understand your project and improve it over time.

You invest in teaching and refining this system. At first, it might feel like a 2x multiplier but as the agents accumulate context and experience, that leverage grows: 5x, then 10x, then more. Your efforts compound because you're not just building features; you're building the capability to build and manage features.

---

## The Problem

You have an ambitious idea: a new service, a complex platform, a real-world system. You can see where it needs to go. But there are only so many hours and only so many people.

The usual options are limiting:

- **Do everything yourself**: Every feature, deployment, and on-call incident routes through you. You become the bottleneck.
- **Hire a larger team**: Slow to spin up, expensive to maintain, and you spend more time coordinating than creating.
- **Glue together existing tools**: You assemble a stack of services, each with its own dashboards, configs, and quirks. You spend a large fraction of your time wiring systems together instead of moving your core idea forward.

What if your project came with its own "operational brain"? Not a passive assistant that waits for instructions, but agents that understand your system, act within clear boundaries, ask for guidance when needed, and get better every week?

---

## How It Works

Ikigai is a platform for building and running these agent systems. It requires a shift in how you think about software development and the operation of software systems.

**1. The "Meta-Project"**

You use the Ikigai terminal to create a new project. Let's say you're building "Videos.com." The project you create in Ikigai isn't Videos.com itself. It's the *control plane* for Videos.com: the collection of agents that will help you build and operate the actual target service.

**2. Dual Building**

You build the target product and its agents simultaneously. This is the core concept.

Working in the Ikigai terminal, you teach your control plane where the Videos.com source code lives, how it's deployed, and how to check if it's healthy. You aren't just writing the code for the video player; you are writing the agent that knows *how* to deploy the video player.

As you build out the actual site, the agents build up their understanding of it. Over time, you add agents for content moderation, customer support triage, or scaling infrastructure. You are building the factory alongside the product.

**3. Deploying the Control Plane**

Like any software, this control plane needs somewhere to run. It can run locally on your laptop during development, or be pushed to a server to run continuously.

For some internal tools or research projects, the control plane *is* the final interface for users.

For projects like Videos.com, the control plane manages something external: a scaled web platform, a robo-taxi fleet, or manufacturing hardware. Where the agents run and what they manage are independent choices.

**4. Collaborative Development**

Development happens at the project level, not just the code level. You define specifications and direction. Agents act as developers to implement them.

Crucially, you review together. You and your agents validate code and run tests. When something is wrong, you don't just fix the output; you refine the agent's instructions so it understands better next time. They propose, you approve.

**5. You Guide, They Compound**

Your daily work shifts to interacting with the control plane. As you refine how the agents work, they get smarter about your specific domain. Tasks that once needed your direct involvement start happening automatically based on rules you've established. The ratio of what you put in to what you get out keeps improving.

**6. Continuous Operation**

There's no single "launch day." You build up the service piece by piece. When parts are ready, you expose them to users. Regular releases, maintenance, and operational overhead are handled through your agents, guided by you. The system runs continuously and improves continuously.

---

## What Kinds of Projects?

Ikigai isn't for one specific type of project. It's for any situation where a small team wants to operate at a larger scale:

- **A startup** building a new service, where a handful of developers need the operational capacity of a larger team
- **A research group** managing experiments, literature, and data pipelines that would otherwise require dedicated staff
- **A business** building internal tools (logistics, customer management, analytics) without hiring a full IT department
- **A consultancy** creating client-facing platforms that need to run reliably without constant attention

The common thread: you have something ambitious to build, limited people to build it, and you want your effort to compound over time rather than staying linear.

---

## What Makes This Different

You might be familiar with AI coding assistants, tools that help you write code faster. Ikigai is different. Those tools help *you* code. Ikigai helps you build *agents* that code, operate, and make decisions alongside you.

You might have heard of platforms that run automated workflows or container orchestration systems that manage infrastructure. Ikigai is different from those too. Those systems run what you've already built. Ikigai runs intelligent agents that participate in building and operating your project.

The key distinction: Ikigai agents are work partners. They consult with you. They bring you decisions that need human judgment and act on your guidance. They aren't just executing predefined steps. They understand context, make judgments within boundaries you set, and handle situations they weren't explicitly programmed for. And they improve as your project matures.

---

## The Deeper Idea

When you build an Ikigai project, you're not just building software. You're building a system that grows with you.

Traditional software development is linear: you put in effort, you get output, repeat. Ikigai development is compounding: you put in effort to build agents, those agents produce output, you improve the agents, they produce more output with less input from you.

Over time, your project develops its own operational capability. The agents become a kind of institutional knowledge. They understand how things work, why decisions were made, what to watch for. New challenges get handled by agents that have learned from past experience.

You stay in control. Agents recommend, you confirm. Agents act within boundaries you set. But the mundane work, the repetitive decisions, the operational overhead: that's handled. You focus on direction and judgment. The agents handle execution.

Ikigai amplifies expertise. The more you know about what you're building, the more effective your agents become. An expert in video systems will build better Videos.com agents than a novice. The agents extend what you already know how to do; they don't replace the need to know it.

---

Ikigai is something I'm building because I think it can help people do more with less.

*The rest of the documentation covers the technical details: how agents work, how the platform is structured, how deployment works. But the core idea is what you've just read: amplification through agents, compounding over time.*
