# The ACME CRM Field Guide: Talking to Your CRM

*This CRM has no screen. There is no app to open, no form to fill out, no dashboard to learn — you run the whole thing by talking to your assistant in plain English. Say what you mean, however you'd naturally say it, and the right thing happens.*

*What follows is one continuous story: you, an ACME sales rep, turning a coyote with a dream into a paying customer on a $48,000 deal. Ride along start to finish, or jump to "Say It Your Way" and the cheatsheet at the back whenever you just need to look something up.*

## Table of Contents

1. [Talking to Your CRM](#1-talking-to-your-crm)
2. [Meet the Cast](#2-meet-the-cast)
3. [What Your CRM Remembers](#3-what-your-crm-remembers)
4. [What You Can Ask For](#4-what-you-can-ask-for)
5. [Act I–II: First Spark to First Contact](#5-act-iii-first-spark-to-first-contact)
6. [Act III: Qualifying & Opening the Deal](#6-act-iii-qualifying--opening-the-deal)
7. [Act IV: Working the Deal](#7-act-iv-working-the-deal)
8. [Act V: Negotiation & the Close](#8-act-v-negotiation--the-close)
9. [Act VI: Everyday Operations](#9-act-vi-everyday-operations)
10. [Say It Your Way](#10-say-it-your-way)
11. [Cheatsheet, Field Reference, Gotchas & FAQ](#11-cheatsheet-field-reference-gotchas--faq)

---

## 1. Talking to Your CRM

Here is the big idea, and it is genuinely the whole trick: **you don't learn the CRM. You just talk to your assistant, and your assistant knows the CRM.**

There is no app to open, no form to fill out, no twelve-tab spreadsheet that becomes load-bearing for your entire business until the day it does not. You connect the CRM to Claude once, and from then on you say things like "log that I called Wile E. about the rocket-skates order" and the right thing happens. Your assistant figures out which moves to make. You never have to.

You also never have to speak the computer's dialect. Say "$48,000" and it stores the exact cents. Say "call him at (310) 555-0147" and it files a clean, dialable number. Say "bump him up to a real opportunity" and it knows that is a specific rung on a ladder, with an exact name the system recognizes. You talk like a person; the assistant handles the units, the formatting, and the precise vocabulary.

And there is no magic phrase to memorize. "Log that I called Wile E.," "note that Wile E. and I just talked," and "add a call to his history" all land in exactly the same place. There is just what you mean, said however you would naturally say it.

You will also never juggle a record number. Every record does have a unique ID under the hood — an opaque string the system generates — but you will never see one or type one. You say "the Coyote Pursuit deal," and your assistant keeps track of which exact record that is.

Behind the scenes your assistant has a tiny, fixed set of moves — find things, pull up the full picture of one thing, save a change, log what happened, tidy up, confirm who you are — and it picks the right one for what you asked. You will meet that handful of moves properly in "What You Can Ask For." This section is just the map. It is short on purpose.

### It is a real sales CRM, not an address book

This is not a digital address book — not a place to dump names and phone numbers. It remembers the whole shape of your business relationships: the companies you deal with, the actual people inside them, the deals in motion, the things you owe folks, and a running history of every call, note, email, and meeting. Anything you would reasonably expect a good salesperson to remember about a relationship, you can ask the CRM to remember instead. We will walk through each kind of thing it tracks in "What Your CRM Remembers."

### What "talking to it" actually looks like

You speak English. The assistant translates. A few representative trades:

- **You:** "What's going on with Wile E. lately?"
  **Assistant (behind the curtain):** looks him up, then pulls his full card — Wile E. himself plus his company, his open deals, his recent interactions, and the open tasks you owe him, all in one shot.
  **You see:** a tidy summary of where things stand.

- **You:** "Log that I had a call with Wile E. about the Coyote Pursuit renewal."
  **Assistant:** finds Wile E., then adds a call to his timeline.
  **You see:** "Logged a call on Wile E.'s timeline." Done.

- **You:** "Bump the Coyote Pursuit deal to the proposal stage."
  **Assistant:** saves that change on the deal, setting its stage to proposal. (It will not try to set the deal's won/lost/open status by hand — that is computed from the stage automatically, so it knows to leave it alone.)

Notice you never said a tool name, an ID, or a field name. You said the *intent*. That is the entire user interface.

### It is forgiving

You can act boldly, because the CRM is built to catch you:

- **Deleting is a soft hide, not a shredder.** When you delete something, it disappears from every search and card, but nothing is actually destroyed — the underlying record is preserved (an undelete would bring it back), and it never shows up later as a broken, dangling link. Deleting a person tidies up the details *they* own, like their emails and phones, but it will not go knock down a deal just because that person was on it. "Delete the duplicate" is a safe sentence.
- **It catches you before you create the same person twice.** If the assistant tries to add a contact whose primary email matches one you already have — or a company that matches one you already have by domain (or, when there is no domain, by name) — the CRM stops and says, in effect, "I think this already exists; here is the one I found," and your assistant updates that one instead of spawning a twin. (If you truly want a second, you can tell it to go ahead anyway.)
- **Your history stays honest.** Logged calls and notes are kept, not quietly rewritten. If you logged the wrong thing, the move is to delete that one entry and log a fresh one — so your record of what actually happened stays trustworthy.

You will see all of this in action later; for now, just know the safety net is there.

### Your "is this thing on?" test

The very first time you connect, ask your assistant something like *"check that you're connected to my CRM and tell me who I am."* It will run the one move whose entire job is to report back the account it sees you as — no input needed. If that comes back with your email, the whole chain — your assistant, the connection, the login, the service — is wired up correctly and you are ready to go. If it does not, that is your signal to look at the connection before you start trusting it with real data.

That is the whole mental model: plain English on your end, a CRM that remembers the shape of your business on the other. The rest of this guide follows one sale from start to finish — you, an ACME sales rep, turning Wile E. Coyote from a newsletter signup into a paying customer on a $48,000 deal. You will meet the full cast next.

**How to read this guide:** skim it straight through to ride along with the story, or jump to "Say It Your Way" and the cheatsheet at the back whenever you just need to look something up.

---

## 2. Meet the Cast

Most CRM tutorials introduce you to "Customer A" and "Account 17," and your eyes glaze over by row three. We're going to do this differently: you'll learn this CRM the way you'll actually use it — by running one real (well, real-ish) sale from first hello to signed deal.

The setting is **ACME Corporation**, and you are its sales rep. The customer is a coyote with a dream and a credit limit. Let's do introductions.

### You: the ACME sales desk

**ACME Corporation** sells anvils, rockets, rocket-powered roller skates, giant magnets, earthquake pills, instant tunnels, and assorted gear that is wildly impractical and somehow always back-ordered. You work the sales desk. Your whole job is keeping track of who wants what, what you promised, and what's about to close — which is exactly what a CRM is *for*. You won't be clicking around a screen to do it; you'll just tell your assistant what's going on, and it remembers.

### The prospect: Wile E. Coyote

**Wile E. Coyote** is the hero of our story and ACME's most devoted buyer-to-be. He is a relentless optimist with one professional goal (the Road Runner) and an apparently bottomless gear budget. His title at his own company is **Chief Pursuit Officer**, which tells you everything. You can reach him at **wile.e@coyotepursuit.example**.

Wile E. is how you'll learn the **lifecycle ladder** — the four rungs a relationship climbs as it gets more serious:

> **subscriber → lead → opportunity → customer**

He starts at the bottom rung — a brand-new name who just signed up for ACME's newsletter — and, over the course of this guide, climbs all four rungs to become a paying customer. Watching one contact make that whole journey is the fastest way to understand what those words mean. (Spoiler: it ends well for him for once.)

### The company: Coyote Pursuit Industries

**Coyote Pursuit Industries** (CPI) is Wile E.'s outfit, single-mindedly devoted to catching one specific bird. Its web domain is **coyotepursuit.example**. People belong to companies in a CRM, so once Wile E. gets serious you'll link him to CPI — and from then on, pulling up the company shows you everyone there and every deal in flight. One company, one tidy file.

### The approver: Yosemite Sam

A deal isn't real until someone who controls the checkbook says yes. At CPI, that's **Yosemite Sam** — **VP, Budgets & Varmint Control**, gruff, short-fused, and deeply skeptical of any line item. Reach him at **sam@coyotepursuit.example**.

Sam doesn't appear until the deal is well underway, and he teaches an important lesson: a single deal can have **more than one person attached, each with a role.** Wile E. is your enthusiastic inside guy (your *champion*); Sam is the **decision maker** who has to approve the spend. You'll add Sam to the deal later *without* knocking Wile E. off it — and you'll do it just by saying so. "Add Sam," "loop in Sam," "Sam's the approver on this one" — they all land in the same place. You don't memorize a command; you describe what you mean, and the assistant works out the rest.

### The newsletter crowd

Wile E. isn't your only subscriber. Rounding out the mailing list:

- **Daffy Duck** — loud, litigious, and the one you'll *accidentally* log a phone call on — and then un-log, because mistakes are fixable here just by saying so (he stars in our "oops, fix that" lesson later)
- **Porky Pig** — stutters through every voicemail
- **Elmer Fudd** — be vewy quiet, he's a buyer
- **Marvin the Martian** — still waiting on his earth-shattering kaboom
- **Foghorn Leghorn** — talks, I say, talks a *lot*

They're mostly background extras, but they earn their keep: they all share one label — they're on the **newsletter** — which is how you'll learn to **find a group** ("who's on the newsletter?") rather than one person at a time.

### The cast, at a glance

| Who | Role in the story | What they teach |
|---|---|---|
| **You / ACME** | The sales rep running the desk | The CRM is your shared memory |
| **Wile E. Coyote** | Star prospect → customer | The lifecycle ladder: subscriber → lead → opportunity → customer |
| **Coyote Pursuit Industries** | Wile E.'s company | Linking a person to a company |
| **Yosemite Sam** | The budget approver | One deal, many people, different roles |
| **The newsletter crowd** | Other subscribers | Finding a *group* — and fixing a mistake (Daffy's mis-logged call) |
| **The Rocket-Skates & Anvil Bundle** | The $48k deal at the center | Deal stages (lead → qualified → proposal → negotiation → won/lost); status is derived, never set |

### The deal at the center of it all

Everything builds toward one sale: the **Coyote Pursuit Industries — Rocket-Skates & Anvil Bundle**, worth **$48,000**, targeted to close around the end of July 2026.

Like Wile E. himself, the deal climbs a ladder — but deals get their *own* set of rungs, called **stages:**

> **lead → qualified → proposal → negotiation → won** (or **lost**)

Careful — this is a *different* ladder from Wile E.'s. The lifecycle ladder tracks the *person* (is this relationship getting serious?); stages track the *deal* (how close is this one sale to done?). They even share the word "lead," but they're answering different questions.

Our deal opens at *qualified* and works its way up to *won*. And here's the nice part: you'll never have to mark a deal *open*, *won*, or *lost* yourself — the CRM reads that straight off the stage. Anything before the finish line reads *open*; move it to *won* and it counts as won; mark it *lost* and it reads lost. That's the whole bookkeeping. (More on that in the next two sections; for now, just enjoy the suspense.)

### The arc, in one breath

Here's the whole movie, no spoilers spared, so you can see where the next few sections are headed:

A coyote signs up for the **newsletter**. You realize he's actually interested, so he becomes a **lead** and gets linked to his company. A discovery call goes well — now he's an **opportunity**, and you open a **$48,000 deal**. His budget-holder, Sam, joins the conversation. You send a proposal, haggle through a negotiation, and finally mark it **won**. Wile E. becomes a paying **customer**, and the anvils ship.

That's it. That's a CRM: a memory that follows a relationship from "who's this?" to "thanks for your business." Everyone you just met is about to walk through it one step at a time — and you'll run the whole thing just by talking. Let's get connected.

---

## 3. What Your CRM Remembers

Here's the thing nobody tells you about a CRM: it isn't a filing cabinet, it's a **memory**. You don't "put a record in." You tell your assistant something true about your business, and it remembers — so that three weeks from now, when you ask "who haven't I talked to since the demo?", the answer is already there.

Your CRM remembers **five kinds of things**. The way you *talk* to it never grows past a handful of moves (that's the next section), and that stays true no matter how the CRM grows up. New capabilities later — products, quotes, support tickets — mostly arrive as new details on these five, not as a new pile of stuff to learn. So if you understand these five, you understand the shape of the whole thing. That's a genuinely small number to keep in your head, and that's the point.

Everything below is something you'd *say* to your assistant in plain English. You never type field names. We're just showing you what's in there so you know what you can ask for.

---

### The five things it remembers

**1. Organizations** — the companies.
The account, the company, the logo on the invoice. An organization needs a **name**, and ideally a **domain** (like `coyotepursuit.example`). The domain isn't decoration — it's how the CRM recognizes a company it already knows. Save the same company twice with the same domain and it won't make a clumsy duplicate; it'll stop and say *"I already have this one."* The website domain is the company's fingerprint.

**2. Contacts** — the people.
The humans you actually deal with. A contact can hold a **first and last name**, a friendly **display name** (don't bother typing one — it'll build a sensible one from the name or email you gave), a **job title**, the **organization** they work at, and as many **emails** and **phone numbers** as the person actually has. One email is the primary and one phone is the primary; the rest are just kept on file, because real people have more than one number and more than one address.

Two more things live on a contact, and they're where a *list* of names quietly becomes a *funnel*:

- **Lifecycle** — where this person sits on the road to becoming a customer: `subscriber → lead → opportunity → customer`. New contacts start as a **lead** unless you say otherwise. This is the single field that lets you ask "show me everyone who's an opportunity but hasn't closed" and get a real answer instead of a shrug.
- **Tags** — plain labels you stick on people. And here's the one fact in this whole section worth remembering: **your monthly newsletter audience is just a tag.** There's no separate "mailing list" feature, no second app to sync. Tag someone `newsletter` and they're on the list; pull the tag and they're off it. The CRM keeps the guest list; somebody else runs the mailroom. It owns *who* gets the newsletter — it does **not** send it. That's a different service's job, and the moment you change who's tagged, the service that does the sending hears about it.

**3. Deals** — the money you're chasing.
An active shot at a sale. A deal has a **name**, the **organization** it's with, an **amount**, a **currency** (dollars unless you say otherwise), an **expected close date**, and the **people involved** — each tagged with their role, like `champion` or `decision_maker`, because "who's actually pushing for this internally" is the question that wins quarters.

A deal also has a **stage**, and it walks a fixed path:

`lead → qualified → proposal → negotiation → won → lost`

New deals start at **lead**. You move a deal by changing its stage. Whether it then counts as **open**, **won**, or **lost** — its *status* — is decided for you. You can *ask* for it ("show me my open deals"), but you never set it directly, and neither does your assistant: a deal in `negotiation` is open because negotiation isn't an ending; a `won` deal is won because won is. **The stage is the truth; the status is just the stage's shadow.**

**4. Tasks** — the things you owe.
Your follow-ups. A task has a **title**, an optional **due date**, and a **status** that is gloriously simple: `open` or `done`. New tasks are **open**. You finish one by telling your assistant it's done — there's no checkbox ritual, you just say it. A task can point at a contact, a company, or a deal ("call back about the CPI renewal"), or it can be a free-floating reminder. Notice the one thing a task does *not* have: an **owner**. There's no "assign to." This is your box, your business — every task is yours by definition, so the CRM doesn't waste a field asking *whose*. (It still quietly records *who did what* for its own audit trail — that just isn't part of the task itself.)

**5. Interactions** — what actually happened.
The timeline. Every note, call, email, and meeting, stamped with when it occurred. This is the entry that turns a contacts list into a *relationship* — it's the difference between "I have her number" and "we talked Tuesday, she's worried about price, she's deciding by Friday." It's also the single most common thing you'll add, because business is mostly *talking to people* and then *remembering you did*.

Interactions come in four flavors — `note`, `call`, `email`, `meeting` — and every one is hung on a subject: a contact, a company, or a deal. Pull up that contact later and the recent history is right there, attached, no digging.

One rule you'll meet eventually, so meet it now: **interactions are append-only.** The history is a ledger, not a whiteboard. You don't edit "had a call" into "had a meeting" after the fact — you delete the wrong entry and log a fresh one. It feels strict until the first time you're glad nobody could quietly rewrite what was said in March.

---

### What happens when you forget something

When you tell your assistant to forget something, the CRM **soft-deletes** it. The record stops showing up in everything — searches, cards, lists — but it isn't shredded off the disk. Two consequences worth knowing:

- **Deleting is shallow and considerate.** Delete a contact and you take their emails, phones, and tags down with them (those belonged to the person). But the CRM does **not** go on a rampage — deleting a company doesn't vaporize every deal and person attached to it, and deleting a person doesn't erase the deals they were part of. Those just quietly stop pointing at a name that's now hidden. No surprise demolitions; you remove what you asked to remove and nothing more.
- **A deleted thing reads as gone, not as broken.** A deal that mentions a now-deleted contact doesn't show a dangling ghost — the missing piece is simply hidden from view, and the link underneath is left intact rather than snapped. One honest caveat: from where you sit, deleting is one-way. There's no "undo" or "undelete" in the assistant's toolkit today. Because nothing is truly shredded, an operator could in principle dig a record back out — but that's a data-safety property under the hood, not a button you (or your assistant) can press.

---

### One last reassurance

You will never need to memorize a field name, a stage spelling, or which tag means "newsletter." That's your assistant's job. You speak; it figures out which of the five things you meant and what to do with it. This section exists so that when the answer comes back, you *recognize* it — so "she's a lead with the newsletter tag and one open deal in negotiation" reads like a sentence about your business, not a database dump.

Five things to remember. A small, fixed handful of ways to talk to them (covered next). That's the whole memory. Now go fill it with something true.

---

## 4. What You Can Ask For

Here's the part where most software hands you a menu of buttons and wishes you luck. We're not going to do that, because there is no menu. Under the hood your CRM can do exactly **six things** — and the whole point of this section is that **you never have to call any of them by name.**

You describe what you want. Your assistant picks the right move. The six "things" below are really just the six shapes your intent can take: *find something, see everything about it, save or change it, remove it, jot down what happened,* and *check you're connected.* You already think in these shapes. You just didn't know they had names.

So read this less like a manual and more like a tour of what's possible. You won't be quizzed.

### The six things, in plain English

#### 1. Find or list things

You ask for something by *describing* it, and the assistant goes and finds it — one record or a whole list. This is how almost every request starts, because before the agent can do anything to the Coyote Pursuit deal, it has to *find* the Coyote Pursuit deal.

**You might say:**
> • "Pull up everyone on the newsletter."
> • "Find Wile E. Coyote."
> • "What deals are still open?"

A couple of honest notes so nothing surprises you. The search matches the words you actually used against the text it can see — a person's name, email, or phone; a deal's name; the wording of a task or a logged note. It's matching text, not reading your mind, so a typo can throw it off. When that happens the agent simply rephrases and tries again; you'll rarely notice. And when there's a long list, it comes back a page at a time — just say *"show me the next page"* and it keeps going.

*(Under the hood: `ikigenba_crm_search`.)*

#### 2. See everything about one thing — the "card"

This is the showstopper. You point at one record and say *"show me everything,"* and instead of a single line of facts you get back a **card**: the thing itself **plus** everything connected to it, gathered in a single shot.

Ask about Wile E. and the card brings his contact details, his company, his open deals, his recent timeline, and his open tasks — all at once. Ask about the deal and you get the company, every participant and their role, the recent back-and-forth, and the open to-dos. No clicking around to assemble the picture; it arrives assembled.

**You might say:**
> • "Give me the full picture on Wile E. Coyote."
> • "Show me everything on the Coyote Pursuit deal."
> • "What's the story with Coyote Pursuit Industries?"

*(Under the hood: `ikigenba_crm_get`.)*

#### 3. Save or change something — including reminders

Add a new contact, company, deal, or task — or update one that already exists. Same idea either way: you say what should be true, and the assistant makes it true. You don't tell it "create" versus "update"; it figures out whether Wile E. already exists and updates him rather than making a second copy.

This is also where the agent quietly does the precision work for you. You say **"(310) 555-0147"** and it stores a proper phone number. You say **"$48,000"** and it stores the exact amount. You say **"bump him up to an opportunity"** and it knows that's a lifecycle move. You speak human; it handles the formats.

**You might say:**
> • "Add Wile E. Coyote to our newsletter list."
> • "Set his title to Chief Pursuit Officer and add his phone."
> • "Move him up to a lead."

A reminder lives here too, because a reminder is just a **task** — a follow-up with a due date so the next step doesn't slip. Saving one, or marking it done, is the same move as saving anything else:

> • "Remind me to book a discovery call with Wile E. next week."
> • "Add a to-do: send the proposal by Friday."
> • "Mark the discovery-call task done."

A task can stand alone or be pinned to a contact, a company, or a deal — so *"send the proposal"* rides along with the very deal it's about.

One friendly thing worth knowing: if you try to add someone who's already in there — say you re-add Wile E. with the same email — the CRM **catches the duplicate** and the assistant offers to update the existing record instead of making a second copy. That's a feature, not a scolding.

*(Under the hood: `ikigenba_crm_save` — a contact, company, deal, and task are all just things you save.)*

#### 4. Remove something

Ask to *"get rid of that test task"* or *"remove the old company record"* and the assistant takes it out of view. You'll reach for this rarely, but it's a first-class move like the others — nothing special to learn. It's gentle by design: removed records drop out of the picture rather than being shredded, and we cover exactly how that behaves in the final reference section.

**You might say:**
> • "Get rid of that test task."
> • "Remove the duplicate company record."

*(Under the hood: `ikigenba_crm_delete`.)*

#### 5. Log a touch — write down what happened

Every call, email, meeting, or note becomes one entry on a timeline. This is the most common thing you'll do in any CRM, because the whole game is *remembering what was said.*

**You might say:**
> • "Log that I demoed the Rocket-Powered Roller Skates (the rocket skates, for short) to Wile E. today."
> • "Add a note: they want a volume discount on anvils."
> • "Record a call with Coyote Pursuit about the Giant Magnet."

Timeline entries are **write-once** — you don't edit a logged call; if you got one wrong, the agent removes it and logs a fresh, correct one. (You'll see exactly that move in a later act, when a logged call gets pinned to the wrong contact.)

*(Under the hood: `ikigenba_crm_log`.)*

#### 6. Confirm you're connected

The quick "are we actually wired up?" check. The assistant reports back **who you're connected as**, which is handy the very first time you use the CRM — or any time you're not sure the line is live.

**You might say:**
> • "Am I connected to the CRM?"
> • "Who am I logged in as?"

*(Under the hood: `ikigenba_crm_health`.)*

### The agent chains these for you

Here's the move that makes the whole thing feel effortless: **a single sentence from you is usually several of these steps stitched together.**

When you say *"log a discovery call on Wile E. and remind me to send the proposal,"* the assistant first **finds** Wile E., then **logs** the call on his timeline, then **saves** the reminder. You said one thing. It did three. You never saw the seams.

This is also why **you never type an ID.** Every record has a hidden, computer-generated identifier (a long opaque code — a ULID, if you're curious), and the assistant keeps track of which one is which. You refer to things the way you'd refer to them out loud — *"the coyote,"* *"that $48k deal,"* *"the buyer over at Coyote Pursuit"* — and the agent quietly maps your words to the right record before it acts.

### The whole toolkit, at a glance

You'll never need this table to *use* the CRM — you just talk. But if you like seeing the gears, here's how a natural request lines up with what happens behind the scenes:

| What you want | Something you might say | Behind the scenes |
|---|---|---|
| Find or list things | "Find Wile E." · "Who's on the newsletter?" · "What deals are open?" | `ikigenba_crm_search` |
| See everything about one thing | "Show me everything on the Coyote Pursuit deal." | `ikigenba_crm_get` |
| Save or change something (incl. reminders) | "Add Wile E. as a subscriber." · "Bump him up to a lead." · "Remind me to book a discovery call." · "Mark that task done." | `ikigenba_crm_save` |
| Remove something from view | "Get rid of that test task." | `ikigenba_crm_delete` |
| Write down what happened | "Log that I demoed the rocket skates today." | `ikigenba_crm_log` |
| Check you're connected | "Am I connected? Who am I logged in as?" | `ikigenba_crm_health` |

Six verbs. Five kinds of record (you met them in the last section). An endless number of ways to say what you want. That's the entire system — and from here on, the rest of this guide just watches it work, one ACME sale at a time.

---

## 5. Act I–II: First Spark to First Contact

Time to put the CRM to work. From here on, the guide follows one story — ACME selling to Wile E. Coyote — and each step is a **beat**. Each beat shows you what **you say** (in your own words), **what happens** (in plain English, with a peek under the hood for the curious), and — when there's a reply worth seeing — **what you get back**. Most beats also include **say it your way**: proof that there's no magic phrasing to memorize.

You bring the intent. The agent brings the precision. Let's go.

---
#### First, are we even connected?

**You say:**
> "Am I connected to the CRM? Who does it think I am?"

**What happens:** Your assistant pings the CRM and reports back who you're signed in as — your email and which app is talking to it. Think of it as a handshake: it proves the whole chain (your assistant → the CRM → your sign-in) is wired up before you trust it with real data. Nothing is created or changed. *(Under the hood: `ikigenba_crm_health` — the one tool that just confirms the connection.)*

**You get back:**
> "You're connected as **you@acme.example**. All set."

**Say it your way:**
> • "Quick check — is the CRM online?"
> • "Who am I logged in as?"
> • "Make sure you can reach our CRM."

If that comes back clean, you're ready. If it doesn't, that's your cue to fix the connection — not your data.

---
#### Wile E. signs up for the newsletter

Our story starts the way most sales stories do: someone raises their hand. Wile E. Coyote dropped his email into ACME's newsletter box. That's the first spark — let's capture it.

**You say:**
> "Add Wile E. Coyote to our newsletter list — his email's wile.e@coyotepursuit.example."

**What happens:** Your assistant creates a new **contact** for Wile E. You gave a name and one email, so that's all it stores for now — it sets his email as the primary, tags him **newsletter**, and — reading "newsletter signup" as exactly that — places him on the bottom rung of the relationship ladder: **subscriber**. You never named that rung; the agent picked it from what you said. *(Under the hood: `ikigenba_crm_save` creating a contact `‹wile-e›`, lifecycle `subscriber`, tag `newsletter`.)*

**Say it your way:**
> • "Put the coyote on the mailing list — wile.e@coyotepursuit.example."
> • "New subscriber: Wile E. Coyote, wile.e@coyotepursuit.example."
> • "Sign Wile E. up for our monthly newsletter."

Notice what you *didn't* do: you didn't fill in a form, pick a "lifecycle" from a dropdown, or worry about whether the email needed lowercasing. You described a person who wants the newsletter. The agent filled in the exact words and tidied the email for you.

---
#### Who's on the newsletter, anyway?

Before we court Wile E., let's see the crowd he just joined. A subscriber list is your warmest pool of future leads, so it's worth a look.

**You say:**
> "Show me everyone on the newsletter list."

**What happens:** Your assistant pulls up every contact tagged **newsletter** and hands you a tidy list. It's filtering on that exact tag — not guessing, not ranking — just "give me the people with this label, newest first." *(Under the hood: `ikigenba_crm_search` over contacts, filtered to `tag: newsletter`.)*

**You get back:**
> "6 subscribers, newest first: **Wile E. Coyote**, **Daffy Duck**, **Porky Pig**, **Elmer Fudd**, **Marvin the Martian**, and **Foghorn Leghorn**."

**Say it your way:**
> • "Who's getting our newsletter?"
> • "List the mailing-list folks."
> • "Pull up all the newsletter subscribers."

A small note for later: when you search by name or email, it matches on the actual text you give it — a precise "find," not a mind reader. (A tag like **newsletter** is an exact label filter, not a fuzzy search.) If you fumble a name, the agent simply rephrases and tries again on your behalf, so you rarely notice.

---
#### Set up Wile E.'s company

Wile E. doesn't operate alone — he runs an outfit. Logging the company now means everything he buys later hangs off a real account, not a lone name floating in space.

**You say:**
> "Create a company called Coyote Pursuit Industries — their website's coyotepursuit.example."

**What happens:** Your assistant creates an **organization** named *Coyote Pursuit Industries* with that domain. Before it saves, it quietly checks that you don't already have a company with the same website or name — no accidental duplicates. *(Under the hood: `ikigenba_crm_save` creating an organization `‹coyote-pursuit›`.)*

**Say it your way:**
> • "Add Coyote Pursuit Industries as an account."
> • "New company: Coyote Pursuit Industries, coyotepursuit.example."

The company's on the books — but in the CRM, Wile E. and Coyote Pursuit Industries have never met. Next beat we'll introduce them, and give Wile E. a promotion while we're at it.

---
#### Promote Wile E. and round out his details

Here's where the relationship gets real. Wile E. replied to the newsletter asking about rocket-powered roller skates — that makes him more than a subscriber. He's a genuine **lead** now. And while we're updating him, let's attach him to his company, record his title, and add the phone number he rattled off.

**You say:**
> "Wile E. is at Coyote Pursuit Industries — he's their Chief Pursuit Officer, reach him at (310) 555-0147. He's a real lead now, not just a newsletter sub."

**What happens:** Your assistant updates Wile E.'s existing contact — it doesn't make a second one. In one go it links him to **Coyote Pursuit Industries**, sets his title to *Chief Pursuit Officer*, stores the phone number, and bumps his lifecycle from **subscriber** up to **lead**. You said "(310) 555-0147"; it saved a clean, fully-qualified number for you. And everything you *didn't* mention — his newsletter tag, his email — stays exactly as it was. *(Under the hood: `ikigenba_crm_save` updating `‹wile-e›`: `org_id` → `‹coyote-pursuit›`, `title`, a phone, lifecycle → `lead`.)*

**Say it your way:**
> • "Link Wile E. to Coyote Pursuit, title Chief Pursuit Officer, phone (310) 555-0147, and move him up to a lead."
> • "Bump the coyote to a lead — he's CPI's Chief Pursuit Officer, his cell is (310) 555-0147."

A couple of things worth pausing on:

- **You moved him up a rung just by saying so.** "He's a real lead now" became the exact lifecycle value **lead**. You never have to know the official ladder words — you describe the change, the agent picks the right term.
- **Adding the phone didn't erase anything.** When you add to a list — phones, emails, tags — the agent merges the new item in with whatever's already there, so "also add his phone" never wipes out the rest. You say "add"; it does the bookkeeping. Wile E.'s phone list now holds the new number, and his newsletter tag is untouched.

---
#### Log the first real conversation

A CRM's most valuable habit is also its simplest: writing down what was said. Wile E. and you just had your first real exchange — capture it before it evaporates.

**You say:**
> "Log that I emailed Wile E. today — he wants a demo of the Rocket-Powered Roller Skates."

**What happens:** Your assistant adds an **email** entry to Wile E.'s timeline with your note, stamped with today's date. From now on, his history shows this first touch — and it'll keep growing as you work the relationship. *(Under the hood: `ikigenba_crm_log` appending an `email` interaction on `‹wile-e›`.)*

**Say it your way:**
> • "Note that I reached out to the coyote about a roller-skates demo."
> • "Add to Wile E.'s history: emailed him, he's interested in the rocket skates."

One thing about timeline entries: they're a permanent record — written down, not edited later. (If you ever log the wrong thing, you remove that entry and write a fresh one — we'll do exactly that in Act V.) For now, just enjoy how little ceremony it took to remember something important.

---
#### Don't let it slip — set a follow-up

A lead with no next step is a lead you'll forget. Close out Act II by giving yourself a nudge.

**You say:**
> "Remind me to book a discovery call with Wile E. — let's say by next Friday."

**What happens:** Your assistant creates a **task** titled "Book discovery call," attaches it to Wile E. so you know who it's about, and sets the due date you described. It starts out **open**; it'll stay on your radar until you mark it done. *(Under the hood: `ikigenba_crm_save` creating a task linked to `‹wile-e›`, with a due date.)*

**Say it your way:**
> • "Set a to-do: schedule a discovery call with the coyote, due Friday."
> • "Follow-up for me — get Wile E. on a discovery call before the weekend."

---

**Where we stand.** In a handful of plain-English sentences, you've taken Wile E. from a name in a newsletter box to a tracked **lead** at a real company — *Coyote Pursuit Industries* — with his title, his phone, a first conversation on his timeline, and a follow-up task waiting for you. You never typed an ID, never picked a form field, never spelled "subscriber" or "lead" for the computer.

Next up, Act III: you make that discovery call, decide the coyote's the real thing — a genuine **opportunity** — tick that follow-up task off your list, and open ACME's first honest-to-goodness deal. (Rocket skates don't sell themselves.)

---

## 6. Act III: Qualifying & Opening the Deal

When we left off, Wile E. Coyote was a **lead** with a single open to-do on your plate: *book a discovery call.* This act is where a polite "thanks for the newsletter" turns into actual money on the table. You'll log the call that changes everything, promote Wile E. up a rung, check that follow-up off your list, and — the big one — open a real **deal** worth **$48,000**.

Same rhythm as before: you say it like a human, your assistant does the bookkeeping. Watch how little you have to spell out.

---
#### The discovery call goes well

**You say:**
> "Just got off a great call with Wile E. He demoed the Rocket-Powered Roller Skates and asked whether the Giant Magnet is Road-Runner-rated. Log it."

**What happens:** Your assistant adds a **call** to Wile E.'s timeline, with your notes as the body and the time set to now. His timeline now reads like a little story: newsletter signup, first touch, and now a real discovery call. *(Under the hood: `ikigenba_crm_log` adds a `call` interaction to `‹wile-e›`.)*

**Say it your way:**
> • "Note that I talked to the coyote today — he wants to know if the magnet works on roadrunners."
> • "Log a call with Wile E. about the roller-skates demo."
> • "Add a call to Wile E.'s history: demoed the skates, asked about the Giant Magnet."

A quick reminder from the last act: this is a *logged touch*, not a note you can rewrite later. Timelines are append-only — they record what actually happened, so they never change under you. (Log one on the wrong person? Don't worry — we fix exactly that in Act V.)

---
#### Wile E. is more than a lead now

**You say:**
> "That call was promising — bump Wile E. up to a real opportunity."

**What happens:** Your assistant moves Wile E. one rung up the relationship ladder, from **lead** to **opportunity**. Nothing else about his record changes — same email, same phone, same company, same newsletter tag. You changed one thing, so one thing moved. *(Under the hood: `ikigenba_crm_save` updates the contact `‹wile-e›`, setting `lifecycle` to `opportunity`. On an update, only the fields you mention change; the rest are left exactly as they were.)*

Notice you never said the word **"opportunity"** like it was a magic command — you said *"a real opportunity,"* and *"bump him up,"* and the agent knew which rung of the ladder you meant. That ladder, if you want the map: **subscriber → lead → opportunity → customer.** You speak in your own words; the agent picks the exact valid value.

**Say it your way:**
> • "Wile E.'s gone from a maybe to a real prospect — promote him."
> • "Move the coyote up to opportunity."
> • "He's serious now. Upgrade his status."

---
#### Cross off the discovery call

**You say:**
> "I booked that discovery call — actually, I already had it. Mark that task done."

**What happens:** Your assistant finds your open *"book a discovery call"* task and flips it to **done**. It also quietly stamps *when* it was finished, so your history of what-got-done-when stays accurate without you tracking a single date. *(Under the hood: `ikigenba_crm_save` updates the task with `status: done`; `done_at` records the completion time.)*

Completing a task really is that small — there's no separate "complete" command to learn. **Finishing something is just saving it as done.** You describe which task ("that discovery call") rather than hunting for an ID; the agent matches it to the open one and closes it out.

---
#### Open the Coyote Pursuit deal
*The centerpiece — everything so far has been a relationship; this is the sale.*

Up to now you've been remembering a *relationship.* A **deal** is where you start tracking a specific *sale* — a named opportunity to win money, with a dollar figure, a target close date, and the people involved.

**You say:**
> "Let's open a deal for Coyote Pursuit Industries — the Rocket-Skates & Anvil Bundle, about $48,000, aiming to close by the end of July. Wile E.'s our champion on it, and it's already qualified."

**What happens:** Your assistant creates a new **deal** named *Coyote Pursuit Industries — Rocket-Skates & Anvil Bundle*, ties it to CPI, sets the amount to **$48,000**, targets a close date of **July 31, 2026** (it read "end of July" and picked the concrete date), opens it at the **qualified** stage, and lists Wile E. as a participant in the role of **champion**. One sentence from you; a fully-formed deal record. *(Under the hood: `ikigenba_crm_save` creates a `deal` linked to org `‹coyote-pursuit›`, with `amount_cents: 4800000`, `currency: USD`, `close_date: 2026-07-31` (a bare date, no time of day), `stage: qualified`, and a participant `{‹wile-e›, role: champion}`. Friendly handle: `‹rocket-skates-deal›`.)*

**You get back:**
> *Coyote Pursuit Industries — Rocket-Skates & Anvil Bundle* — **$48,000 USD**, stage **qualified** (status: **open**), closing Jul 31, 2026. Champion: Wile E. Coyote.

*(That tidy line is the deal's "card" — pull it up any time with a plain "show me the Rocket-Skates deal," and the agent fills in the resolved names and full context, like Wile E.'s champion role, from the live record.)*

**Say it your way:**
> • "New deal: CPI wants the rocket-skates-and-anvil bundle, roughly 48 grand, closing end of July. Wile E.'s pushing it internally."
> • "Start tracking a sale to Coyote Pursuit — $48k bundle, qualified, Wile E. as champion, close target July 31."
> • "Open the Rocket-Skates & Anvil deal for CPI at $48,000."

Three things just quietly happened on your behalf, and each one is the whole philosophy of this CRM in miniature:

- **You said "$48,000." The system stores `4800000`.** Money is kept as a whole number of cents so the math never drifts a penny — but you'd never want to do that conversion in your head. You speak dollars; the agent handles the units. (So if you ever spot `4800000` in the raw record — relax, that's your $48,000, just counted in pennies.)
- **You said "Wile E.'s our champion."** "Champion" is a *role* on the deal — the person inside the customer's world who's rooting for you. You named the person by description and gave their role in plain English; the agent attached them. (In the next act you'll add a second person to this same deal — a different role, a different job to do.)
- **You said "it's already qualified" — not "set status to open."** You set the *stage*; the deal's **status** is worked out from it automatically. A deal that's `qualified` is simply **open** — neither won nor lost yet. You never set status by hand; it always follows the stage. The stage ladder, for reference: **lead → qualified → proposal → negotiation → won/lost.**

---

**Where the story stands now.** Wile E. has climbed from lead to **opportunity**. The discovery call is on his timeline, and the task that prompted it is checked off. And ACME has a live, **$48,000** deal — the *Rocket-Skates & Anvil Bundle* — sitting at the **qualified** stage with Wile E. as its champion.

You did all of that by describing what happened in a handful of plain sentences. Next act, the deal gets real: a second decision-maker enters — a new role on the very same deal — and you'll watch the CRM hand you the whole picture of a deal — the people, the history, the open work — in a single ask.

---

## 7. Act IV: Working the Deal

A contact in your address book is a hello. A *deal* is a story with money at the end of it. This act is where the CRM stops being a Rolodex and starts being a pipeline: you'll run the **Coyote Pursuit Industries** deal through a working session — log a meeting, bring a new player to the table, move the deal up a stage, and leave yourself a breadcrumb so future-you actually sends the proposal. Then comes the payoff: you ask one plain question and get the *entire* deal — people, history, open work — handed back in a single answer.

Same rhythm as before. Watch how few moves it takes — the CRM never grows new verbs; you just keep saying the same handful of things to different nouns.

> **Where we left off.** Coming out of Act III, the **Rocket-Skates & Anvil Bundle** is a live deal against **Coyote Pursuit Industries**, sitting at the **qualified** stage, with **Wile E. Coyote** on it as the champion — the guy on the inside who wants this to happen. We're about to find out he can't actually sign anything. (He never can. His whole career is wanting something he can't close on — he orders from the catalog; someone else signs the check.)

---
#### Log the meeting — on the deal itself

In a real CRM the single most common thing you do is write down that something happened. Here, that something is a meeting.

**You say:**
> "Log a meeting on the Coyote Pursuit deal: talked to Wile E., he loves the rocket skates but says the budget call isn't his — there's a guy named Sam who signs off on anything that goes *boom*."

**What happens:** Your assistant adds a **meeting** to the *deal's* timeline, with your note as the body and the time set to now. Notice you hung it on the deal, not on a person — and you never had to say "this is a deal, not a contact." You named what it's about; the agent worked out where the note belongs. *(Under the hood: `ikigenba_crm_log`, kind `meeting`, on the deal `‹rocket-skates-deal›`.)*

**Say it your way:**
> • "Note on the CPI deal: Wile E.'s sold, but Sam holds the purse strings."
> • "Add a meeting to the rocket-skates deal — turns out Sam's the money guy."

A timeline entry is always one of four kinds — **note, call, email, meeting** — and the agent picks the right one from how you describe it ("talked to," "met with," "emailed"). As in the earlier acts, an entry is a permanent record: if you ever log the wrong thing, you remove it and write a fresh one rather than editing it in place. (We'll do exactly that in Act V.)

---
#### A new name turns up — add Yosemite Sam

The meeting surfaced a name you don't have yet. A new person is a new **contact**.

**You say:**
> "Add a contact: Yosemite Sam, the rootinest, tootinest check-signer this side of the Pecos. Email sam@coyotepursuit.example, he's at Coyote Pursuit and his title's VP of Budgets."

**What happens:** Your assistant creates a new **contact** for Sam, links him to **Coyote Pursuit Industries**, records his title, and files his email as the primary one (tidied to lowercase, as always). Because this is a brand-new person, the CRM first makes sure nobody with that email is already on file — no accidental second Sam. *(Under the hood: `ikigenba_crm_save` creating a contact, wired to the company `‹coyote-pursuit›`.)*

**Say it your way:**
> • "New contact at CPI: Yosemite Sam, sam@coyotepursuit.example, VP of Budgets."
> • "Add the money man — Yosemite Sam over at Coyote Pursuit."

One subtle thing: you called Sam "the decision maker," but that part *didn't* land on his contact. A person isn't a decision-maker in the abstract — they're a decision-maker *on a particular deal*. That role lives on the relationship, which is exactly the next beat.

---
#### Put Sam on the deal — without dropping Wile E.

This is the move clunkier tools get wrong. You want to *add* Sam to the deal while *keeping* Wile E. on it.

**You say:**
> "Put Sam on the Coyote Pursuit deal as the decision maker. Keep Wile E. on as champion."

**What happens:** Your assistant updates the deal so its roster is now both people — **Wile E. Coyote** as champion and **Yosemite Sam** as decision-maker. You said "add Sam"; the agent re-sends the *whole* roster, both names, so nobody falls off. *(Under the hood: `ikigenba_crm_save` updating the deal's participants — the list it sends becomes the complete roster.)*

**Say it your way:**
> • "Add Sam as the money guy, leave Wile E. where he is."
> • "Sam's the decision maker on the CPI deal now — Wile E. stays champion."

> **The one rule worth knowing here.** A deal's participants (and a contact's tags, back in Act I) behave as *sets*: whatever list the agent sends becomes the complete list. Send only Sam and you'd quietly knock Wile E. off his own deal. That's why, when you say "add Sam," a good assistant first reads who's already on the deal and then sends everyone. **You** just say "add" or "drop"; the agent does the bookkeeping so nothing falls off by accident.

---
#### Move it up a stage — qualified → proposal

Sam's at the table and the meeting went well. Time to move the deal forward. A deal's stage is just another thing you change by saying so.

**You say:**
> "Move the Coyote Pursuit deal to the proposal stage."

**What happens:** Your assistant advances the deal from **qualified** to **proposal**. *Only* the stage changes — the roster from the last beat, the amount, the close date, all stay exactly as they were, because that's all you mentioned. And you didn't touch the deal's *status*: it's still **open** (neither won nor lost), and it stays that way on its own until the stage becomes won or lost. *(Under the hood: `ikigenba_crm_save`, setting the deal's `stage` to `proposal`.)*

**Say it your way:**
> • "Bump the rocket-skates deal to proposal."
> • "We're at the proposal stage now."

You name the *stage*; the CRM keeps the deal's open/won/lost status in lockstep with it, so there's never a status for you to set by hand. (More on that relationship in the appendix.)

---
#### Leave yourself a breadcrumb — the "send proposal" task

A stage change is a promise to do something. The thing here is *send the proposal*, and the way you keep that from evaporating is a **task** — linked to the deal so it shows up in context, not in some orphaned to-do void.

**You say:**
> "Remind me to send Coyote Pursuit the proposal by next Friday."

**What happens:** Your assistant creates a **task** titled "Send proposal to Coyote Pursuit Industries," attaches it to the deal, and sets the due date. It starts out **open**, so it rides along on the deal until you close it. *(Under the hood: `ikigenba_crm_save` creating a task linked to `‹rocket-skates-deal›`, with a due date.)*

**Say it your way:**
> • "To-do: get the proposal out to CPI by Friday, and tie it to the deal."
> • "Follow-up on the rocket-skates deal — send the proposal this week."

When you eventually send it, you'll close this the same easy way you close any task: just say "mark it done." (There's no "assigned to" here — it's your CRM, so every task is already yours.)

---
#### The payoff — "show me the whole deal"

Five small updates. Now collect the interest. Ask for the deal — by name, not by ID — and the assistant hands back not just the deal but *everything attached to it*, in one shot. You don't fetch the company, then the people, then the history, then the tasks. You ask once.

**You say:**
> "Pull up the Coyote Pursuit deal."

**What happens:** Your assistant looks up the deal and returns its full **card** — the deal plus all its context, assembled in a single call. *(Under the hood: `ikigenba_crm_get` on `‹rocket-skates-deal›`.)*

**You get back** — one card, fully composed:

- **The deal itself** — *Coyote Pursuit Industries — Rocket-Skates & Anvil Bundle*, stage **proposal**, status **open**, with its **$48,000** amount and its July 31, 2026 close date.
- **The company** — **Coyote Pursuit Industries**, attached inline. (Notice the deal's *name* is the rocket-skates bundle; the company's name is the account it's against — two different things.)
- **Both participants, with their roles** — **Wile E. Coyote** as champion and **Yosemite Sam** as decision-maker. The full roster you set two beats ago, neither one dropped.
- **The recent timeline** — newest-first, led by the **meeting** you logged at the top of this act. Up to the last 20 entries ride along; the full history is one "show me everything" away (we'll do that in Act VI).
- **The open tasks** — your **"Send proposal to Coyote Pursuit Industries,"** still open, due Friday, staring back at you.

> **Why this is the act's whole point.** You created two kinds of record, logged history, set a roster, advanced a stage, and scheduled a follow-up — all by describing what happened. Then one plain question paid it all back as *context*, not as five separate lookups you have to staple together yourself. That's the promise: ask about a thing, and you get the thing *and* everything around it.

---

**Where the story stands now.** The Rocket-Skates & Anvil Bundle is at **proposal**, Yosemite Sam is at the table as decision-maker beside Wile E. the champion, the meeting is on the deal's timeline, and a "send proposal" task is waiting. The only thing between you and a closed deal is actually sending the thing — which is sitting right there in your open tasks.

In **Act V**, you close it: log the negotiation, fix a mistake without rewriting history, flip the deal to **won**, and watch a long-suffering coyote finally become a paying customer.

---

## 8. Act V: Negotiation & the Close

This is the act you've been working toward. The proposal is out, both players are on the deal — Wile E. Coyote as your champion and Yosemite Sam as the money man — and now the haggling begins. By the end of this act you'll have logged a tense negotiation call, fixed a mistake without ever rewriting history, flipped the deal to **won**, and watched a long-suffering coyote graduate from "opportunity" to "paying customer."

When we left off, the Rocket-Skates & Anvil Bundle was sitting at stage **proposal** for **$48,000**, with a "send proposal" task open. Let's bring it home.

---
#### Sam pushes back — log the negotiation call

**You say:**
> "Log a call on the Rocket-Skates deal: Sam wants a volume discount on the anvils before he'll sign off. He's pushing hard."

**What happens:** Your assistant adds a **call** to the deal's timeline, with your note about the discount written into the body. Notice you put this one on the *deal*, not on a person — the negotiation is about the deal as a whole, so that's where it belongs. *(Under the hood: `ikigenba_crm_log`, kind `call`, subject `‹rocket-skates-deal›`.)*

**Say it your way:**
> • "Add a call to the Coyote Pursuit deal — Sam's holding out for a bulk anvil discount."
> • "Note on the deal: budget approver wants a price break before he approves."
> • "Logged a call with Yosemite Sam, he's negotiating on the anvil pricing."

---
#### Move the deal into negotiation

**You say:**
> "We're officially negotiating now — move the deal to the negotiation stage."

**What happens:** Your assistant advances the deal one rung up the stage ladder, from **proposal** to **negotiation**. The deal's *status* stays **open** — it's still in play, neither won nor lost — and you didn't have to touch the status at all, because status follows the stage automatically. *(Under the hood: `ikigenba_crm_save`, updating the deal's `stage` to `negotiation`.)*

**Say it your way:**
> • "Bump the deal to negotiation."
> • "We're past the proposal — Sam and I are hashing out terms now."
> • "Set the Rocket-Skates deal to negotiating."

You only ever name the *stage* — "negotiation," "we're negotiating," "haggling stage." The CRM keeps the deal's open/won/lost status in lockstep with it, so there's nothing for you to set there. (More on the stage-and-status relationship in the appendix.)

---
#### Oops — that call landed on the wrong duck

Here's the most useful thing in this whole act, and it starts with a mistake. You meant to log a follow-up call on Wile E., but it landed on **Daffy Duck** instead. These things happen — and the fix is painless. Here's how you put it right.

First, what went wrong:

**You say:**
> "Log a call on Daffy: Wile E. checked in to see if Sam's warming up to the discount."

**What happens:** Your assistant dutifully adds that **call** to **Daffy Duck's** timeline — exactly as asked. The trouble is, that note belongs on Wile E., not Daffy. *(Under the hood: `ikigenba_crm_log`, kind `call`, subject `‹daffy-duck›` — the wrong subject.)*

Now you catch it. Here's the important part: **a logged touch can't be edited.** You can't reach in and "move" it or "fix the name." Interactions are *append-only* — once written, the entry itself never changes. That sounds restrictive, but it's exactly what makes a timeline trustworthy: a history that can be quietly rewritten isn't really a history. So the way you correct a logged touch is to **remove the wrong one and log a fresh one** on the right subject.

**You say:**
> "That call should've been on Wile E., not Daffy. Remove it from Daffy and put it on the coyote instead."

**What happens:** Your assistant does two things in sequence. First it **removes the mistaken call from Daffy's timeline** — that entry disappears from view. Then it **logs the call again**, this time on Wile E. Coyote, with the same note. Daffy's timeline is clean, Wile E.'s timeline is correct, and you described the fix in one plain sentence. *(Under the hood: `ikigenba_crm_delete` with type `interaction` on the mistaken entry, then a brand-new `ikigenba_crm_log` on `‹wile-e›`. There's no "edit interaction" — correcting one is always delete-then-re-log.)*

**Say it your way:**
> • "Wrong contact — kill that call on Daffy and re-log it under Wile E."
> • "Scrap the Daffy note and add it to the coyote's timeline."
> • "Move that call over to Wile E. — it's on the wrong contact."

> **Why this is worth knowing:** Every other record in your CRM you can edit in place — change a contact's title, raise a deal's stage, mark a task done. **Interactions are the exception.** They're a permanent record of *what happened and when*, so you don't revise them; you correct the timeline by deleting the wrong entry and logging the right one. You'll still just *say* "fix that" — the agent handles the two-step dance for you.

---
#### Close the deal — mark it won

The discount got worked out, Sam's satisfied, and the coyote finally gets his rocket skates. Time for the moment that makes all of this worth it.

Before you close, sanity-check the numbers:

**You say:**
> "Make sure the amount's still $48,000 and the close date is the end of July."

**What happens:** Your assistant confirms — or sets — the deal's amount and close date. You say "$48,000" and "end of July"; the agent stores the amount as exact cents and the date in its standard format. Everything else on the deal stays put, because an update only changes the fields you mention. *(Under the hood: `ikigenba_crm_save` on the deal, confirming `amount_cents` 4800000 and `close_date` 2026-07-31; untouched fields are left alone.)*

Numbers confirmed — now close it.

**You say:**
> "We got it! Mark the Rocket-Skates deal as won."

**What happens:** Your assistant sets the deal to its closing stage — **won** — the outcome that ends the deal. (Every deal finishes one of two ways: **won** or **lost**.) Because a deal's *status* is derived from its stage, the status flips from **open** to **won** on its own — you named the stage; the status updated itself. *(Under the hood: `ikigenba_crm_save`, updating the deal's `stage` to `won`; `status` becomes `won` on its own.)*

**Say it your way:**
> • "Close it out as a win."
> • "We won the Coyote Pursuit deal!"
> • "Sam signed — mark it won."
> • "That one's in the bag, set it to won."

Four different ways to say "we won," all landing in the same place. You don't reach for the "right" word — you say what happened, and the agent maps it to the **won** stage.

**You get back:**
> Marked **Coyote Pursuit Industries — Rocket-Skates & Anvil Bundle** as **won**. Status is now **won**, amount **$48,000**, expected close **July 31, 2026**.

---
#### Promote Wile E. to customer

A won deal is the headline, but there's one more thing that should change: Wile E. isn't a prospect anymore. He's a *customer*. That's a step on the **lifecycle** ladder — the relationship ladder for people — and right now he's sitting at **opportunity**. Let's move him up the last rung.

**You say:**
> "Wile E.'s a real customer now — bump him up."

**What happens:** Your assistant moves Wile E.'s lifecycle from **opportunity** to **customer**, the top of the ladder. Nothing else about him changes — same email, same phone, same newsletter tag, same company. You just say "he's a customer now"; the agent knows that's a lifecycle move and picks the exact value. *(Under the hood: `ikigenba_crm_save` on `‹wile-e›`, setting `lifecycle` to `customer`.)*

**Say it your way:**
> • "Promote the coyote to customer."
> • "Wile E. went from prospect to paying customer — update him."
> • "Mark Wile E. as a customer."

Sam earned the same upgrade — he's a contact on a won deal too. Just say "Sam's a customer now as well" and the agent promotes him the same way *(`ikigenba_crm_save` on `‹yosemite-sam›`, `lifecycle` to `customer`)*. Two contacts, one closed deal, both now on the books as customers.

---

**Where things stand now:** The Rocket-Skates & Anvil Bundle is **won** — its status flipped to **won** the instant you set the stage. The deal carries its **$48,000** amount and a **July 31, 2026** close date. Wile E. Coyote has climbed the entire relationship ladder, from a newsletter subscriber in Act I to a **customer** here in Act V, with Yosemite Sam right beside him. And along the way you learned the one record you never rewrite — the timeline — and the only way to correct it: remove and re-log.

The coyote finally caught something. Next, in Act VI, we'll cover the everyday work of running a CRM full of won deals: catching duplicates, reviewing your pipeline, reading a full history, and tidying up.

---

## 9. Act VI: Everyday Operations

The crisis is over. The pipeline is full, the timeline is long, and Wile E. Coyote has — against every prior expectation — actually closed a deal. What's left is the part nobody writes home about: the daily upkeep. This is the act where the CRM stops being a place you *build* and becomes a place you *live*. No new tricks here — just the same six moves you already know, worn smooth from use. The whole point of a six-move surface is that "everyday operations" never needs a seventh.

---
#### The coyote, again — when you create a twin

You're tidying up the morning's inbox and you go to add a contact for Wile E. Coyote, figuring you surely haven't gotten to him yet.

**You say:**
> "Add a contact — Wile E. Coyote, wile.e@coyotepursuit.example."

**What happens:** Nothing gets created — and that's the CRM doing you a favor. Before making a new contact, it checks whether that email is already on file, finds it belongs to the Wile E. you've been working all along, and stops. Instead of quietly minting a second coyote, your assistant comes back with: "looks like he's already in here — want me to update the existing record, or make a separate one anyway?" *(Under the hood: `ikigenba_crm_save` tried to create, the duplicate check matched his primary email, and it handed back the existing contact instead of a twin.)*

This is the CRM catching the second purchase before it ships — a courtesy it manages for you that it never quite manages for the coyote.

You've got two honest ways forward:

- **You meant the same person** (almost always) → just say **"oh, update the one we have."** Your assistant edits the existing Wile E. instead of duplicating him — and, as ever, only the fields you mention change.
- **You really did mean a separate record** that happens to share the email (rare — say, a shared returns mailbox the coyote reads) → say **"no, make a new one anyway,"** and your assistant creates a distinct contact on purpose.

> **The takeaway.** A duplicate isn't an error you have to clean up — it's the CRM steering you toward the record you probably meant. The match is *exact* (same email for people; same website or exact name for companies), so near-misses like "Coyote Pursuit" versus "Coyote Pursuit Industries" won't trip it. Only true collisions do.

**Say it your way:**
> • "Update the coyote we already have."
> • "Yeah, that's the same guy — just edit him."
> • "No, genuinely a different person — add them anyway."

---
#### Monday pipeline review

Standup time. You want to know where the money is. Start broad — everything still in play:

**You say:**
> "What deals are still open?"

**What happens:** Your assistant lists every deal that hasn't been won or lost yet — the live pipeline, most-recently-touched first. *(Under the hood: `ikigenba_crm_search` over deals, filtered to status **open**.)*

Then zoom into a single rung of the ladder — say, the deals stuck in proposal, where things tend to stall like the coyote mid-air, briefly weightless, holding a small sign:

**You say:**
> "Show me the deals sitting in proposal."

**What happens:** You get just the deals at the **proposal** stage. *(Under the hood: `ikigenba_crm_search` over deals, filtered to stage **proposal**.)*

And the part you actually look forward to:

**You say:**
> "What have we won this quarter?"

**What happens:** Your assistant returns the closed-won list — every deal where the boulder, for once, landed on the *target*. *(Under the hood: `ikigenba_crm_search` over deals, filtered to status **won**.)*

> **Status and stage are two zoom levels on the same pipeline.** "Open / won / lost" is the three-bucket executive view; the stage — lead → qualified → proposal → negotiation → won/lost — is the fine-grained one. You don't maintain both: you set the *stage*, and the status follows. Ask in whichever terms you happen to think in; the agent reaches for the matching filter.

**Say it your way:**
> • "Where's my pipeline at?"
> • "Anything still in negotiation?"
> • "Show me this quarter's wins."

---
#### Read the whole story — a full timeline

A card shows a deal's or a contact's *recent* history — the last 20 entries, which is plenty for "what happened lately." But sometimes you want the *whole* saga: every call, email, and meeting where someone tried to talk ACME into a refund. That's a deeper read.

**You say:**
> "Show me everything that's ever happened on the Coyote Pursuit deal."

**What happens:** Your assistant pulls the deal's complete timeline — every logged interaction about it, newest first — instead of just the recent slice a card carries. *(Under the hood: `ikigenba_crm_search` over interactions, filtered to that deal as the subject.)*

If the history is long, it comes back a page at a time. You don't manage any of that — you just say:

**You say:**
> "Keep going — show me the next page."

**What happens:** Your assistant fetches the next batch, picking up exactly where the last one left off, and keeps going until you reach the start of the story — the coyote's very first ACME order. *(Under the hood: `ikigenba_crm_search` continues from the last entry it saw.)*

> **The card is a preview; this is the archive.** "Recent interactions" on a card is the highlight reel; asking for the full timeline is the director's cut. And notice there's no special "timeline" tool — reading a full history is just a search with the right filter, and "next page" is just one more part of the same ask. Six moves, still no seventh.

---
#### The roster — who actually bought

Pipeline reviewed, history read — now the quarterly question: who actually *bought*? Your customers live at the very end of the relationship ladder.

**You say:**
> "List all our customers."

**What happens:** Your assistant returns every contact whose lifecycle is **customer** — the people who, unlike a certain coyote, eventually caught what they were chasing. *(Under the hood: `ikigenba_crm_search` over contacts, filtered to lifecycle **customer**.)*

The same ask answers the neighboring questions just by swapping the rung: "show me our leads," "who are the active opportunities," "list the newsletter subscribers." One move, four rosters.

> **"List my customers" isn't a special feature** — it's a search scoped to a lifecycle. There's no separate "list" tool, because a list is just a search with a filter and no search words. The day you add a fifth lifecycle stage, this exact request still works — you change a word, never a verb.

**Say it your way:**
> • "Who are our paying customers?"
> • "Pull the customer list."
> • "Everyone who's actually bought from us."

---
#### Tidy up — delete a stale task

Last chore. There's a task from three sprints ago — *"Follow up re: Spring-Powered Boxing Glove (returns)"* — that's never getting done and is cluttering the coyote's card.

**You say:**
> "Delete that stale boxing-glove follow-up task."

**What happens:** Your assistant removes the task. It vanishes from the coyote's card and won't surface in searches — gone from every view you'll ever use. *(Under the hood: `ikigenba_crm_delete` on the task.)*

**Say it your way:**
> • "Drop that old boxing-glove to-do."
> • "Get rid of the stale returns task."
> • "Clear that follow-up, it's dead."

Two things are worth knowing about what "delete" really means here:

- **It's removed from view, not shredded — but there's no undo.** A delete hides a record from every read; there's no trash bin to fish it back out of. So treat delete as final, and reach for it only for genuine cleanup.
- **It's shallow — it won't blow a crater in your data.** Deleting a record takes only that record (and the small things it *owns* — a contact's emails, phones, and tags). Anything that merely *points* at it is left alone. Delete a contact and a deal that listed them as a participant survives untouched — it simply stops showing the now-hidden person. So you can tidy up without fear of toppling a row of dominoes.

> This is the same gentle instinct as the duplicate check at the top of the act: the CRM would rather hide something quietly than do something it can't take back.

---

**Curtain.** Notice what running the CRM *wasn't*: there was no new verb, no "admin mode," no escape hatch. A duplicate caught, a pipeline reviewed, a full history read, a customer roster pulled, a stale task cleared — all of it was the same handful of moves you learned closing Wile E.'s deal: find, look, save, log, remove, plus the quick "who am I" handshake at the door. The coyote never learns. The CRM, mercifully, doesn't have to — the six things that built this story run it forever. Which was the whole bet from Act I, made good.

---

## 10. Say It Your Way

Here's the secret the whole CRM is built around: **you never have to learn its words.** There are exactly six things this service can do, and your assistant's job is to map whatever you actually said onto one of them. You bring the human sentence; it brings the verb.

So this section is a phrasebook. Same intent, twenty ways to say it — all of them work, because they all land on the same six tools underneath: `ikigenba_crm_search`, `ikigenba_crm_get`, `ikigenba_crm_save`, `ikigenba_crm_delete`, `ikigenba_crm_log`, and `ikigenba_crm_health`. Say it your way; the translation is on the house.

### The one rule that makes this work

> **Loose in, corrective out.** You phrase the request like a person. If something's off — a typo'd stage, a phone number missing its country code, a word that isn't in the menu — the service doesn't faceplant. It hands back a plain-English error that names the field and lists the valid options, and your assistant fixes it and retries. You usually never see the round trip.

That's the deal. Be casual; the guardrails are downstream.

### Finding things

This is the opening move on almost any request. If you're not sure what exists yet, you're searching (the assistant reaches for `ikigenba_crm_search`).

- "Who do we know at Coyote Pursuit Industries?"
- "Pull up the deals stuck in proposal."
- "Show me everyone tagged newsletter."
- "List my open tasks."
- "Find that contact whose email starts with wile…"

All of those become a search — a recency-ordered list of skinny summaries (newest-touched first). Notice the last three: "tagged newsletter," "stuck in proposal," "open tasks" are *filters*, and the service knows them by name (`tag`, `stage`, `status`). You don't say `filters:{stage:"proposal"}` — you say "stuck in proposal" and let the robot type the JSON.

When you want the *full picture* of one specific record, that's a different move (the assistant reaches for `ikigenba_crm_get`):

- "Open up Wile's card."
- "Give me everything on the rocket-skates deal."
- "What's the full history on this contact?"

> **One call brings the whole story.** Ask for one contact and it comes back with their organization, their open deals, recent interactions, **and** open tasks already stapled on. You don't request the attachments; they show up. It's the colleague who answers "where's the file?" by walking in with the file, the folder, and a coffee.

You don't have to know an ID to start. "Find Wile" then "open his card" is two sentences; the assistant carries the ID between them so you don't have to.

### Creating and changing things

One verb covers organizations, contacts, deals, and tasks. Create, update, doesn't matter — it's all `ikigenba_crm_save`. (Interactions are the exception; those get their own verb, below.)

| You say… | What it does |
|---|---|
| "Add a contact, Wile E. Coyote at Coyote Pursuit Industries, wile.e@coyotepursuit.example" | Creates a contact |
| "Set his lifecycle to customer" | Updates that one field |
| "New deal: Coyote Pursuit Industries — Rocket-Skates & Anvil Bundle, $48,000, in negotiation" | Creates a deal |
| "Bump the rocket-skates deal to the won stage" | Updates the stage |
| "Tag Wile newsletter and vip" | Updates his tags |
| "Remind me to follow up Friday" | Creates a task |

Phrase it however you'd say it to a coworker. "Make a," "create a," "add," "set up," "we just signed," "log a new" — they all route to the same place. And yes, "$48,000" is fine: the service stores money as plain cents (4,800,000 of them here), and your assistant does the conversion so you never have to.

A few things worth knowing so the results don't surprise you:

> **Only what you mention changes.** Saying "set his title to VP of Acquisitions" doesn't quietly wipe his email or his tags — fields you don't name are left exactly as they were. The one twist is **set-valued fields** — a contact's tags and a deal's participants. Those are *declarative*: the list you give is the **complete** new list. "Tag Wile vip" means his tags are now `[vip]` and the old ones are gone. If you mean *add*, say "add vip to his tags" so your assistant sends the full set. Tags don't accumulate on their own; whatever list you give is the list you get, so when you change them, name the whole set.

> **Some words come off a fixed menu.** A contact's lifecycle is `subscriber`, `lead`, `opportunity`, or `customer`. A deal's stage is `lead`, `qualified`, `proposal`, `negotiation`, `won`, or `lost`. A task is `open` or `done`. Say "he's a prospect now" and the service gently notes that "prospect" isn't on the menu and offers the real four — your assistant picks the closest (`opportunity`) and moves on. You speak human; you usually never see the exchange.

> **Deal status is read-only.** A deal's status (`open` / `won` / `lost`) is *derived* from its stage — you can't set it directly, and that's correct. Move the stage to `won` and the status follows; everything that isn't won/lost reads as `open`. Ask to "mark the deal won" and the right thing happens via the stage. The status field is a mirror — yelling at the reflection won't move you.

### When you create a twin — the duplicate nudge

Try to add a contact whose primary email is already on file, or an organization whose domain (or exact name) already exists, and the save stops and says so. It'll come back with something like *"a contact with that email already exists — want me to update the one we have, or make a second one anyway?"* (The actual message names the existing record and mentions a flag for forcing a duplicate, but that part is for your assistant to read, not you.)

This is a gift, not a wall. Ninety-nine times out of a hundred you meant "update the one we have," so just say **"oh, update the existing one instead"** and your assistant retries as an edit. If you genuinely want a second record — same email, separate person, long story — say **"no, make a new one anyway"** and it forces the duplicate through. Either way, you say the human thing and the assistant handles the mechanics. The CRM would rather ask once than grow evil twins quietly. (Matching is *exact*: same normalized email, or same domain / same name. "Coyote Pursuit" and "Coyote Pursuit Industries" won't trip it — only true collisions do.)

### Logging what happened

The single most frequent thing you'll do: write down that something occurred. That's `ikigenba_crm_log`, and it's append-only — a timeline, not a whiteboard.

- "Log a call with Wile — he's in for the rocket-skates bundle."
- "Note: emailed Coyote Pursuit the proposal."
- "Had a meeting with Yosemite Sam today, the decision maker — going great."
- "Add a note to the rocket-skates deal that legal signed off."

You name what it's *about* (a contact, an organization, or a deal — your assistant resolves the right record) and what *kind* it was: `note`, `call`, `email`, or `meeting`. Time defaults to now; say "yesterday" or "last Tuesday at 3" and that gets carried along.

> **Logs are written in ink.** You can't edit an interaction — to fix a mistake you delete that entry and log a fresh one. So "actually, that was a meeting, not a call" means *delete and re-log*, not *edit*. A timeline you could quietly rewrite isn't a timeline; it's a rumor with timestamps.

### Removing things

- "Delete that duplicate contact."
- "Drop the Coyote Pursuit test deal."
- "Remove this task."
- "Get rid of that bogus note."

Every delete here is a **soft, shallow** delete: the record is removed from view — hidden from every read, not shredded. Delete a contact and its own emails, phones, and tags go with it; but anything that merely *points* at it (a deal that listed them as a participant) stays intact and just stops showing the hidden record. So a delete won't blow a crater in your deals. Treat it as final, though — there's no trash bin and no undo button to fish a record back out.

### "Who am I, even?"

- "Who am I connected as?"
- "Is this thing actually authenticated?"
- "Confirm the connection works."

`ikigenba_crm_health` takes no input and just reports the identity the platform established for you — your owner email and client id (alongside the shared health envelope). It changes nothing. It's the verb you reach for once, right after setup, to prove the whole chain lit up — the handshake at the end of the wiring, not part of your daily work.

### The cheat sheet

| What you want | Say something like… | Verb underneath |
|---|---|---|
| See what exists / a list | "find," "who do we know," "show me all," "list the…" | `ikigenba_crm_search` |
| The full story on one record | "open," "pull up," "everything on…" | `ikigenba_crm_get` |
| Make or change a record | "add," "create," "set," "update," "tag," "remind me to…" | `ikigenba_crm_save` |
| Write down what happened | "log," "note," "had a call/meeting/email…" | `ikigenba_crm_log` |
| Make something go away | "delete," "remove," "drop," "get rid of…" | `ikigenba_crm_delete` |
| Check who you're connected as | "who am I," "is this authenticated" | `ikigenba_crm_health` |

You don't memorize this table — your assistant does. You just keep talking. Say it your way; the six verbs will be waiting.

---

## 11. Cheatsheet, Field Reference, Gotchas & FAQ

This is the one section you can mostly ignore — you run this CRM by *describing what you want*, and your assistant handles the exact words, units, and IDs for you. But when you want to know precisely what the CRM remembers and exactly how it behaves — the kind of thing you'd double-check before quoting a number to Yosemite Sam — this is the page to trust. It's the precise reference; accuracy here beats cleverness.

A reminder before the tables: you don't type any of this. The field names, enum values, and tool names below are the vocabulary *the agent* speaks on your behalf. You still just say "bump Wile E. up to a customer."

### The two ladders (and the one read-only rule)

There are two "ladders" a record can climb. Keep them straight: one is about a *person's relationship* with you, the other is about a *deal's progress*.

**The lifecycle ladder (for contacts)** — where a person stands with ACME:

```
subscriber  →  lead  →  opportunity  →  customer
```

- `subscriber` — on your list (e.g. the newsletter), not yet a real prospect.
- `lead` — a real prospect worth working. **This is the default** if you don't say otherwise.
- `opportunity` — actively in a deal.
- `customer` — they bought.

You can move a contact up or down this ladder anytime by just saying so ("Wile E.'s a real lead now," "promote him to a customer"). The rungs are a convention, not a locked sequence.

**The deal-stage ladder (for deals)** — how far along a sale is:

```
lead  →  qualified  →  proposal  →  negotiation  →  won
                                                  ↘  lost
```

- Stages run `lead → qualified → proposal → negotiation`, then end at either `won` or `lost`. **`lead` is the default** for a new deal.
- You set the *stage* ("we're at proposal now," "mark it won").
- Like the lifecycle rungs, these are a vocabulary, not a forced march — you can jump straight to any stage by saying so ("skip ahead, mark it won"). Nothing makes you walk them in order.

**The derived-status rule — read this twice.** A deal also has a **status**, but you never set it. Status is *computed* from the stage and is **read-only**:

| If the stage is… | …the status is |
|---|---|
| `won` | **won** |
| `lost` | **lost** |
| anything else (`lead`, `qualified`, `proposal`, `negotiation`) | **open** |

So when you said "mark the Rocket-Skates deal won," the stage became `won` and the status flipped to `won` on its own. Trying to set the status directly is rejected — there's nothing to set. You drive the stage; status follows.

### Field reference, per record type

Five record types. Every record has an opaque ID and timestamps that the assistant manages for you. "Required when creating" means the agent needs that from you (in plain words) to make the record; everything else is optional and filled in or left blank.

**organization** — a company or account (e.g. Coyote Pursuit Industries).

| Field | What it is |
|---|---|
| `name` | The company name. **Required to create.** |
| `domain` | Website / email domain (e.g. `coyotepursuit.example`). |

> Duplicate check on create: by exact `domain` if the company has one; by exact `name` only when no domain is given.

**contact** — a person (e.g. Wile E. Coyote).

| Field | What it is |
|---|---|
| `given_name` | First name (e.g. `Wile E.`). |
| `family_name` | Last name (e.g. `Coyote`). |
| `display_name` | What you call them. **Auto-derived only if you don't give one:** it uses what you say first, otherwise "given family", otherwise the primary email. An explicit name is always respected. |
| `org_id` | The company they belong to (you say the company's name; the agent links the ID). |
| `title` | Their job title (e.g. `Chief Pursuit Officer`). |
| `lifecycle` | One of `subscriber, lead, opportunity, customer`. Defaults to `lead`. |
| `emails` | A list of `{email, label}`. **The first one is the primary.** Stored lowercased. |
| `phones` | A list of `{phone, label}`, stored in E.164 form. |
| `tags` | A list of plain strings (e.g. `newsletter`). |

> Duplicate check on create: by the normalized **primary email**.

**deal** — a potential sale (e.g. the $48,000 Rocket-Skates & Anvil bundle).

| Field | What it is |
|---|---|
| `name` | A label for the sale. **Required to create.** |
| `org_id` | The company the deal is with. |
| `stage` | One of `lead, qualified, proposal, negotiation, won, lost`. Defaults to `lead`. |
| `amount_cents` | The value as integer **cents** (you say `$48,000`; this stores `4800000`). |
| `currency` | Defaults to `USD`; stored uppercased. |
| `close_date` | The expected close date. |
| `contacts` | Participants, as a list of `{id, role}` (e.g. Wile E. as `champion`, Sam as `decision_maker`). |
| `status` | **Derived & read-only** — see the rule above. Not something you set. |

**task** — a follow-up or reminder (e.g. "book a discovery call").

| Field | What it is |
|---|---|
| `title` | What to do. **Required to create.** |
| `status` | `open` or `done`. Defaults to `open`. |
| `due_at` | When it's due. |
| `done_at` | When it was finished — **stamped automatically** when you mark it done; **cleared automatically** if you reopen it. |
| `subject` | *Optional* — one of `contact_id`, `org_id`, or `deal_id` to attach the task to a person, company, or deal. |

> There's no "assignee" — this is your single-user box, so every task is yours.

**interaction** — one logged touch on a timeline (a note, call, email, or meeting).

| Field | What it is |
|---|---|
| `kind` | One of `note, call, email, meeting`. |
| `body` | What happened ("Demoed the rocket skates; Sam wants a volume discount"). |
| `occurred_at` | When it happened. Defaults to now. |
| `subject` | The contact, org, **or deal** the entry is about (one of `contact_id`, `org_id`, or `deal_id`). |

> Interactions are special — see "Append-only" below.

**The exact enum words, all in one place** (the agent picks these from your casual phrasing — you never type them):

| Ladder / list | Allowed values |
|---|---|
| Contact lifecycle | `subscriber`, `lead`, `opportunity`, `customer` |
| Deal stage | `lead`, `qualified`, `proposal`, `negotiation`, `won`, `lost` |
| Deal status (read-only) | `open`, `won`, `lost` |
| Task status | `open`, `done` |
| Interaction kind | `note`, `call`, `email`, `meeting` |

### Behaviors worth understanding

**Set-valued fields are declarative (the "add Sam" rule).** Emails, phones, tags, and deal participants are *sets*: when one of them is updated, the list sent **is** the complete desired list. This is why, back in Act IV, when you said "add Sam" — bringing Yosemite Sam onto the Rocket-Skates deal as the decision-maker on the buyer's side — your assistant re-sent **both** Wile E. and Sam, not just Sam. If it had sent only Sam, Sam would have *replaced* Wile E. You never see this: you say "also tag him `vip`" or "add Sam," and the agent reads the current list, merges your change in, and writes the whole thing back. The rules underneath:

- **Mention a set → it's replaced** with exactly what's sent.
- **Omit a set → it's left untouched.**
- **Send an empty list → it's cleared.**

The takeaway for you: say what you want *added* or *removed*; the agent handles the merge so nothing falls off the list by accident.

**Updates touch only what you mention.** Saving a change updates *only* the fields you bring up; everything else is left exactly as it was. "Change his title to VP" changes the title and nothing else.

**Duplicate detection & `force`.** When the agent *creates* a record, the CRM runs a duplicate check (organizations by domain, or by name when there's no domain; contacts by primary email). If there's already a match, the CRM stops and says "this already exists," handing the agent the existing record's ID. It's not a failure you have to clean up — it's the system steering you toward the record you probably meant. The agent's normal move is to offer to update the existing record instead. If you genuinely want a second, separate record anyway, you can say so ("no, make a brand-new one") and the agent creates it deliberately with `force`. This only fires on *create*; updates never trip it.

**Append-only interactions (and how to correct one).** You **cannot edit** a logged call, note, email, or meeting, and you cannot create one through the save verb — interactions are added **only** by logging them. So say you logged "discussed the anvil order" against Daffy Duck when it was really Wile E. You don't edit it; you tell your assistant "that call was actually with Wile E., not Daffy — move it," and it quietly deletes the misfiled entry and re-logs a clean one on Wile E.'s timeline. The timeline stays an honest record of what happened, not a thing you rewrite in place.

**Soft, shallow delete — and no undo.** "Deleting" anything removes it **from view**; it's a hide, not a shred. It is also **shallow**: it hides the record and the children it *owns* (a contact's emails/phones/tags, a deal's participant links), but records that merely *reference* it are left alone — they just stop showing it. So deleting Coyote Pursuit Industries would **not** delete Wile E. or Sam; their contacts survive and simply lose the company link. Important: there is **no undelete tool**. Treat delete as final, and lean on it only for genuine cleanup.

### How finding things works

When you ask the CRM to find or list something, two things are worth knowing so your expectations match reality.

**Search matches text, not meaning.** Matching is **substring** — the search looks for your words *inside* a record's key text. It is **not** fuzzy and it does **not** do typo-tolerance or "relevance ranking." What `query` matches, per type:

| Type | `query` matches on |
|---|---|
| contact | name / email / phone |
| organization | name / domain |
| deal | name |
| task | title |
| interaction | body |

If a search comes up empty because of a typo or an odd phrasing, that's the *search layer* being literal — and it's exactly where your **assistant compensates**, rephrasing and retrying until it finds the right record. You won't feel the substring limit; the agent absorbs it.

**Filters narrow a search.** Beyond the text query, you can scope to one type and apply filters:

| Type | Filters you can apply |
|---|---|
| contact | `lifecycle`, `org_id`, `tag` |
| deal | `stage`, `status` (open / won / lost), `org_id` |
| task | `status`, `contact_id`, `org_id`, `deal_id` |
| interaction | `subject_id` (the contact/org/deal it's about), `kind` |
| organization | (none beyond the text query) |

This is what's underneath "show me our open deals" (`deal`, `status: open`), "who's on the newsletter" (`contact`, `tag: newsletter`), or "the whole timeline for the Rocket-Skates deal" (`interaction`, `subject_id` = the deal).

**Results come back most-recently-updated first, and they paginate.** Search orders results by **`updated_at` descending** — most-recently-*touched* first. The practical consequence is worth internalizing: edit an old record and it rises back to the top, ahead of a never-touched newer one. "Newest-first" would be the wrong mental model; it's "most-recently-changed first." With no type specified it searches across everything and merges the same way, by most-recent update. (The one exception is an interaction timeline, which is ordered by *when each entry happened* — and because logged entries are append-only, that lines up with the order you added them.)

Results come back a page at a time. If you ask for a batch and get a full one, there's probably more — the CRM hands back a `next_cursor`, which is really just the ID of the last record you've seen so far. To continue, the agent asks again, passing that value back as `after_id` ("give me what comes after this one"). You just say "more" or "next page"; the agent remembers where it left off and picks up from there.

### FAQ & gotchas

**Do I need to know any IDs?**
No. You never type a record ID. You refer to things by description — "the coyote," "the Rocket-Skates deal," "Sam at Coyote Pursuit (CPI)" — and the assistant resolves it to the right record. Internally every record has an opaque ID (a ULID); the agent tracks them so you don't have to.

**Can I edit a call (or note, email, meeting) I already logged?**
No — logged interactions are append-only. To fix one, the agent deletes the wrong entry and logs a corrected one (the Daffy-to-Wile-E. "move it" trick above). The result looks like an edit to you; underneath it's a clean delete-and-re-log.

**What happens if I add someone who's already in here?**
The CRM catches it. On create, it checks for duplicates (people by primary email, companies by domain or — when there's no domain — by name) and, if there's a match, points the agent at the existing record. The agent will normally offer to update *that* one instead of making a second copy. If you really want a separate record, say so and it'll create one anyway.

**Did I lose Wile E. and Sam when I deleted their company?**
No. Delete is shallow — it hides the company and the things the company *owns*, but contacts only *reference* the company, so they survive. They just stop showing the now-hidden org. (And note: deleting only hides; there's no restore button, so delete intentionally.)

**Why did it say $48,000 is `4800000`?**
Because money is stored as integer **cents** for exactness — `$48,000` is `4,800,000` cents. You always speak in dollars ("forty-eight grand," "$48,000"); the agent does the conversion both ways, so you never count zeros. Currency defaults to USD and is stored uppercased.

**Why doesn't a typo'd search find my contact, even though I can see them?**
Search is literal substring matching, not fuzzy. The exact letters have to appear in the record's text. In practice your assistant notices the miss and rephrases the search for you — but if you're ever surprised by an empty result, a typo is the usual culprit.

**Why does an old contact jump to the top of my search?**
Because search is ordered by most-recent *change*, not by when the record was created. Edit a long-standing contact and it sorts ahead of newer, untouched ones. That's the intended behavior — "what I worked on last" usually being what you want first — but it can surprise you if you expect strict newest-added order.

**Why can't I set a deal's status to "won"?**
Because status isn't yours to set — it's *derived* from the stage. Set the **stage** to `won` (or `lost`) and the status follows automatically. For every other stage, the status is `open`.

**How do I "complete" a task?**
Just mark it done ("that's handled," "cross it off"). The agent saves the task with status `done`, and the system stamps the completion time for you. Change your mind? Reopen it and the completion time is cleared.

**Can I add a tag without wiping the existing ones?**
Yes — that's automatic. Even though the tag list is replaced as a whole set under the hood, the agent reads your current tags, adds the new one, and writes the full list back. You only ever say "also tag him `vip`."

**Is there a screen or app I'm missing?**
No — and that's the point. There are no buttons, forms, or screens. The entire CRM is the six things the agent can do on your behalf, driven by you describing what you want in plain language. This appendix exists for precision, not because you'll ever type any of it.
