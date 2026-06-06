# The Greased Lightning Ledger: Talking to Your Books

A guide to keeping a real set of double-entry books by talking to your assistant in plain English. It follows one shop — Greased Lightning Auto — through its first year: you're Homer Simpson, you open with your own money and a loan, and you keep the books through a year of sales, a lingering invoice, a corrected mistake, a bank reconciliation, and a year-end close. Read straight through to follow the story, or jump to "Say It Your Way" (§9) and the reference (§10) to look something up.

## Table of Contents

1. [Talking to Your Ledger](#1-talking-to-your-ledger)
2. [Meet the Cast](#2-meet-the-cast)
3. [What Your Ledger Remembers](#3-what-your-ledger-remembers)
4. [What You Can Ask For](#4-what-you-can-ask-for)
5. [Day One — Opening the Shop](#5-day-one--opening-the-shop)
6. [The Daily Rhythm — Sales, Invoices & Bills](#6-the-daily-rhythm--sales-invoices--bills)
7. [End of Month — Reconcile & Report](#7-end-of-month--reconcile--report)
8. [End of Year — Closing the Books](#8-end-of-year--closing-the-books)
9. [Say It Your Way](#9-say-it-your-way)
10. [Reference — Tools, Fields, Gotchas & FAQ](#10-reference--tools-fields-gotchas--faq)

---

## 1. Talking to Your Ledger

You don't keep these books; you describe your day, and your assistant keeps them. You say "Moe paid cash for the $450 transmission job" and the right entries appear, balanced to the penny. The assistant works out which accounts to touch, which side each goes on, and how to make the two halves match.

That means you never speak the bookkeeper's dialect. You won't type an account name, a debit, a credit, or a plus or minus sign. Say "Moe paid cash" and the money lands in the till, not the bank. Say "bill Mr. Burns for the engine overhaul" and the assistant knows money is now owed *to you* and files it there. There's no phrase to memorize, either: "Moe paid cash for the transmission job," "log a $450 cash sale to Moe," and "we did Moe's transmission job, he paid four-fifty cash" all land in the same place.

### It's a real set of books

This isn't a running tally of your bank balance. It's double-entry bookkeeping — the discipline a real accountant uses — so every change to your money is recorded as a small balanced entry with two sides: where the money came from and where it went. Buy a lift with money from the bank and the books note two things: you own a lift now, and your bank balance is lower by what it cost. Bill a customer and they note two more: the income you earned, and the customer's promise to pay. You do none of that work; you say "we bought a hydraulic lift for eight thousand out of the bank," and the assistant writes both sides.

### It keeps you honest

Two habits make the books trustworthy:

- **You never erase; you reverse.** The journal is permanent — no editing an entry, no deleting one. To fix a mistake, the assistant posts a mirror-image entry that cancels it, then records the correct version. The mistake, its cancellation, and the correction all stay on the books. (You'll watch this with a fat-fingered utility bill later.)
- **The books always balance.** Every entry has two equal and opposite sides, so the grand total across every account is always zero. That's a built-in correctness check, running under everything, which is why you can trust the number when the assistant says you're $1,220 in the red.

Every entry also has a unique ID under the hood, but you'll never see or type one. You say "that wrong utilities bill from the 30th," and the assistant tracks which entry that is.

### Check the connection

The first time you connect, ask your assistant to confirm it can reach the ledger and report who you are. If it comes back with your email, the whole chain — your assistant, the connection, your sign-in, the service — is wired up and you're ready. If not, fix the connection before trusting it with real money.

The rest of this guide follows Greased Lightning Auto through its first year: opening the books, the daily rhythm of sales and bills, closing the month, and closing the year. You'll meet the cast next.

---

## 2. Meet the Cast

You'll learn these books by running one shop through a year. The setting is **Greased Lightning Auto**, a small repair shop in Springfield, and you own it. One set of books for the whole shop. Introductions:

**You: Homer Simpson, owner.** A good mechanic and, by your own admission, hopeless with money. It doesn't matter here, because you won't do any accounting — you'll describe your day ("Lenny and Carl got paid," "Mr. Burns finally paid his bill") and the assistant keeps the books.

**Lenny & Carl, the mechanics.** They turn the wrenches and expect to be paid. They teach **wages** — money that leaves the shop as an expense.

**Moe Szyslak, the cash customer.** Comes in with a slipping transmission; you rebuild it; he pays **$450 cash** into the till. The simplest transaction there is: a cash sale, money in.

**Ned Flanders, the prompt payer.** You do the work, send him a bill, and he pays it promptly. Ned teaches an **invoice that opens and closes fast** — money owed to you for a few days, then settled.

**Mr. Burns, the slow payer.** Buys a big **engine overhaul on account** and takes his time. His unpaid bill is the receivable we follow start to finish: billed in June, still open at month-end, paid in July.

**Crazy Vaclav, the parts supplier.** Sells you parts, sometimes on account (he bills you, you pay later), sometimes on the shop credit card. He teaches the **owing** side: money you owe a vendor.

**Springfield Savings, the bank.** Put up the **startup loan** and mails a monthly **statement** — its record of what moved through your checking account. Matching your books against that statement is **reconciling**.

| Who | Role | Teaches |
|---|---|---|
| You / Homer | Owner keeping the books by talking | You describe what happened; the ledger does the precise part |
| Lenny & Carl | The two mechanics | Wages — money out as an expense |
| Moe Szyslak | Cash customer ($450 transmission job) | The simplest sale: cash in |
| Ned Flanders | Prompt-paying customer | A receivable that opens and closes fast |
| Mr. Burns | Wealthy, slow-paying customer | A receivable start to finish: billed → outstanding → paid → closed |
| Crazy Vaclav | Parts supplier | A vendor bill you owe (a payable), and a credit-card purchase |
| Springfield Savings | The bank | The startup loan and the monthly reconciliation |

The year ahead, in brief: Homer opens with **$10,000** of his own money and a **$15,000** loan, and buys an **$8,000** lift. Moe pays cash. Ned gets billed and pays; Burns gets billed and doesn't. Parts come from Vaclav, some on account, some on the card. Lenny and Carl get paid, rent gets paid, a utility bill gets fat-fingered and corrected. At month-end you reconcile against the bank and find June ran a **$1,220 loss** — normal for month one. Burns pays in July. On December 31, after a **$21,000** profit for the year, you close the books.

Two threads are worth watching: **Mr. Burns's invoice** (billed → outstanding → paid → closed, June to July) and **a payment's confirmation journey** (pending → cleared → reconciled, with one rent check stuck on pending because it hasn't hit the bank).

---

## 3. What Your Ledger Remembers

A set of books is a memory: a permanent, balanced record of everything that happened to your money. You tell your assistant something true about the shop, and it remembers, so that at month-end "who owes me?" or "did I make a profit?" already has an exact answer. There's really only one kind of thing the ledger writes down; everything else is reading it back.

### The one thing it writes: a balanced transaction

A transaction is one thing that happened to your money, recorded with both sides showing. Every transaction has at least **two sides** (bookkeepers call them *postings*), because money comes from somewhere and goes somewhere. Moe's $450 sale has two: the till goes up $450, labor income goes up $450. Buying the lift has two: you own a lift now, and your bank balance went down. The two sides always **balance** — add up every side of any transaction and it sums to **zero**. The ledger won't record one that doesn't.

The two sides are what old-school bookkeeping calls **debit** and **credit**, and generations of people have been tortured remembering which is which. You won't: the assistant decides which side each account goes on. You say "Moe paid cash for the $450 job," and it works out that the till is the debit and the income is the credit, and that they net to zero. The signs are real and they matter; they're the assistant's job, not yours.

One shortcut is worth meeting now. When you describe a transaction you usually know only *one* number — "Mr. Burns owes me $1,854." You say the numbers you know, and the assistant lets the last side balance itself to whatever makes the entry level. You'll see this everywhere.

### The five buckets

Every account — and an account is just a labeled place money can sit — belongs to one of five buckets:

1. **Assets — what you own.** Checking, cash in the till, the lift, and money customers owe you (a promise to pay you is something you own).
2. **Liabilities — what you owe.** The startup loan, the credit-card balance, an unpaid bill from Vaclav, sales tax you collected for the state.
3. **Equity — your stake.** What's yours once you subtract what you owe from what you own: the money Homer put in, plus accumulated profit.
4. **Income — what you earn.** Labor, parts sold. (Also answers to **Revenue**.)
5. **Expenses — what you spend.** Wages, rent, utilities, the parts you buy.

That's the whole chart of accounts: **own, owe, your stake, earn, spend.** Every transaction is money moving between these five. They always relate the same way — what you own equals what you owe plus your stake — and the assistant keeps that true on every entry, which is *why* every transaction balances.

### Accounts appear when you use them

You never maintain a chart of accounts. Accounts are emergent: they spring into existence the first time you use them. The first time you describe putting money in the bank, the checking account is born; the first time you bill Mr. Burns, an account for what Burns owes appears. The one rule is that every account lives under one of the five buckets. Below that, the assistant organizes things sensibly — keeping what Burns owes separate from what Flanders owes — and you never think about it.

### Confirmation states: pending, cleared, reconciled

Recording that money moved and confirming it moved at the bank are different things. Each posting carries a status:

- **`pending`** — recorded, not yet confirmed. Everything starts here.
- **`cleared`** — seen on the bank or card statement; the money really moved.
- **`reconciled`** — matched against an official statement and locked in.

You'll walk a deposit up this ladder at month-end, and watch one rent check stay `pending` because it hasn't cleared. That gap is what reconciling is for. A status is the only thing about an existing entry you can change — and even then, only the status, never an amount, account, or date.

### Immutability: you reverse, you never erase

The journal never changes — no edits, no deletes. To fix a mistake you **reverse** it: the assistant posts a mirror-image transaction, every side flipped, that cancels the wrong one, then records the right one. The mistake, its cancellation, and the correction all stay on the books, in order. A book you could quietly rewrite isn't a record. The lone exception is the confirmation status above, which records the outside world catching up, not a rewrite of history.

You'll never need to memorize an account name, a bucket, a debit or a credit, or how to balance two sides. This section is here so that when the answer comes back — "you're $1,220 in the red for June, but Burns still owes you $1,854" — it reads like a sentence about your shop. Now go open the shop.

---

## 4. What You Can Ask For

Under the hood the ledger does exactly **eight things**, and you never call any of them by name. You describe what you want; the assistant picks the move. Only one of the eight is a true write — recording a transaction; everything else undoes it, confirms it, or reads it back.

#### 1. Record what happened

The one you'll use most. You describe a sale, bill, payment, or paycheck, and the assistant writes it into the books, balanced. There's no separate "invoice" feature and no "pay a bill" button — every one of those is this single move.

> • "Moe paid cash for the $450 transmission job."
> • "Bill Mr. Burns $1,854 for the engine overhaul — he's paying later."
> • "Pay Lenny and Carl their $2,000 wages out of the bank."

*(`ikigenba_ledger_record` — one balanced transaction.)*

#### 2. Undo a mistake

You can't edit or delete an entry. When something's wrong, the assistant posts a mirror-image transaction that cancels it, then records the right version.

> • "That utilities bill was wrong — back it out."
> • "Reverse the $1,500 utilities charge, it should've been $150."

*(`ikigenba_ledger_reverse` — posts the linked, sign-flipped mirror of an entry.)*

#### 3. Confirm something cleared

When a payment shows up on a statement, you mark it confirmed — `pending` to `cleared`, then `reconciled` once the statement ties out. This is the only change you can make to an existing entry, and it touches nothing but the status.

> • "These all cleared on the bank statement — mark them cleared."
> • "Everything ties out — lock these in as reconciled."

*(`ikigenba_ledger_reconcile` — moves postings among `pending` / `cleared` / `reconciled`.)*

#### 4. See where you stand

The workhorse read: it totals accounts and tells you the balance. A balance sheet, a P&L, a net worth, and a "who owes me" report are all this one move pointed at different accounts.

> • "How much is in the bank right now?"
> • "Who owes me money?"
> • "What did we make and spend in June?"

*(`ikigenba_ledger_balance` — totals accounts, optionally filtered by account, period, or status.)*

#### 5. List a history

Where balances give totals, this gives the story: every entry touching an account, in date order, with a running balance. This is a customer statement or an account history.

> • "Show me Mr. Burns's account — everything he's been charged and paid."
> • "List all the transactions that hit checking in June."

*(`ikigenba_ledger_register` — matched entries in date order with a running total.)*

#### 6. Pull up one entry

See a single transaction in full — every side, its status, whether it's been reversed. Handy right before you fix something.

> • "Pull up that wrong utilities entry so I can see it."

*(`ikigenba_ledger_get` — fetches one transaction in full.)*

#### 7. Learn the lay of the land

The orientation move: it reports the five buckets, the confirmation states, the live list of accounts, and the report recipes. You'll rarely ask for it — it's how the assistant gets its bearings the first time it opens your books.

> • "What kinds of accounts do my books have?"

*(`ikigenba_ledger_describe` — the discovery call.)*

#### 8. Confirm you're connected

The quick "are we wired up?" check: it reports who you're connected as. It changes nothing.

> • "Am I connected to my books? Who am I logged in as?"

*(`ikigenba_ledger_health` — reports the connected identity.)*

Invoices, bills, and reports aren't separate features. An invoice is a recorded transaction that says a customer owes you; a bill is the same in reverse; paying either is another recorded transaction; a report is a balance or register read. A single sentence often chains several moves: "that utilities bill should've been $150" makes the assistant pull up the bad entry, reverse it, and record the correct one — you said one thing, it did three, and you never type an ID along the way.

| What you want | Something you might say | Behind the scenes |
|---|---|---|
| Record what happened | "Moe paid cash for the $450 job." · "Bill Burns $1,854." | `ikigenba_ledger_record` |
| Undo a mistake | "That entry was wrong — back it out." | `ikigenba_ledger_reverse` |
| Confirm something cleared | "These cleared on the statement." | `ikigenba_ledger_reconcile` |
| See where you stand | "How much is in the bank?" · "Who owes me?" | `ikigenba_ledger_balance` |
| List a history | "Show me Burns's account." | `ikigenba_ledger_register` |
| Pull up one transaction | "Show me that utilities entry." | `ikigenba_ledger_get` |
| Get the lay of the land | "What kinds of accounts do I have?" | `ikigenba_ledger_describe` |
| Check you're connected | "Am I connected? Who am I?" | `ikigenba_ledger_health` |

---

## 5. Day One — Opening the Shop

You've signed a lease on a little repair shop in Springfield. You can rebuild a transmission blindfolded; the money side has never been your strong suit. The good news is you don't have to learn bookkeeping — you say what happened, and the assistant does the double-entry part. Let's open the shop from nothing.

#### Is this thing on?

**You say:** "Are you connected to my ledger? Who does it think I am?"

Your assistant reports who you're signed in as — your owner email and which app is talking to the books. Nothing is created. *(`ikigenba_ledger_health()` — takes no input.)*

**You get back:** "You're connected as **homer@greasedlightning.example**. The books are open and ready."

If that comes back clean, you're in business. If it doesn't, that's your cue to fix the connection, not your data.

#### Get your bearings

Before posting anything, have the assistant orient itself on the books.

**You say:** "I've never done this before. Walk me through what these books can hold."

It calls the ledger's discovery move and translates the answer: every dollar lands in one of the five buckets — Assets, Liabilities, Equity, Income, Expenses — and accounts appear as you use them, with no setup step. *(`ledger_describe()` — the first call any assistant makes. It returns the five typed roots, the money unit (`USD cents`), the reconciliation states, the live account tree, and report recipes.)*

#### T1 — Homer puts in $10,000

You're moving ten thousand dollars of your own savings into the business checking account at Springfield Savings.

**You say:** "I'm putting ten thousand dollars of my own money into the shop. Opening capital. Today."

Money showed up in the bank (one side), and it came from you, the owner (the other). The assistant records the $10,000 landing in the bank and the matching $10,000 as your stake — your Equity. You said one number; the balancing side took care of itself. *(`ledger_record("2026-06-01", "Owner's opening capital", [{Assets:Bank:Checking, +1000000}, {Equity:OwnerCapital (elided → −1000000)}])`. You said `$10,000`; the assistant stored `1000000` cents. The `Equity:OwnerCapital` leg has no amount written down — it's elided, so it absorbs the balancing `−1000000`. The two signed amounts sum to zero.)*

**You get back:** "Recorded. $10,000 of owner's capital — it's in checking, and it's logged as your stake in the shop."

You didn't say "debit," didn't pick an account from a list, and typed one dollar figure. That doesn't change no matter how big the shop gets.

#### T2 — The startup loan, $15,000

Springfield Savings is lending you fifteen thousand.

**You say:** "Springfield Savings just gave me a $15,000 startup loan. It's in checking."

Same two-sided shape, different second bucket. The $15,000 lands in the bank (an Asset up), and the matching side records that you now owe the bank (a Liability). Borrowed money isn't income — you have to pay it back — so it never touches Income. *(`ledger_record("2026-06-01", "Startup loan — Springfield Savings", [{Assets:Bank:Checking, +1500000}, {Liabilities:Loan:SpringfieldSavings (elided → −1500000)}])`.)*

> **Capital vs. loan.** Money you put in is Equity (your stake); money you borrow is a Liability (a debt). Both land cash in the bank; they differ on the other side. You say "my money" or "a loan," and the right bucket gets picked.

#### T3 — Buy the hydraulic lift, $8,000

You can't fix a car on the floor. You buy a lift for eight thousand, straight from checking.

**You say:** "I bought a hydraulic lift for $8,000, paid from checking."

This is the first entry where both sides are things you own: cash leaves the bank (an Asset down), and a lift takes its place (an Asset up). Your total wealth didn't change; you swapped cash for equipment. No income, no expense. *(`ledger_record("2026-06-01", "Hydraulic lift", [{Assets:Equipment:Lift, +800000}, {Assets:Bank:Checking, −800000}])`. Two real numbers this time — nothing elided — because you're naming both where the money went and where it came from.)*

You never created an account called `Assets:Equipment:Lift`. You mentioned a lift, and the account sprang into existence the moment you posted to it. That's true of every account here.

#### Watch the chart appear

**You say:** "Show me where everything stands right now."

The assistant pulls every account that now exists, with its balance, and reads it back in plain dollars. A minute ago this was empty; now there's a chart of accounts you never designed. *(`ledger_balance()` with no arguments — the whole emergent account tree with raw balances.)*

**You get back:**
> "Here's the shop so far:
> - **In the bank:** $17,000
> - **The hydraulic lift:** $8,000
> - **Owner's capital (your stake):** $10,000
> - **Loan owed to Springfield Savings:** $15,000
>
> You own **$25,000**, you owe **$15,000**, and your stake is **$10,000**. It ties out."

(The bank shows $17,000: $10,000 in plus $15,000 in minus $8,000 for the lift.)

In four sentences — a handshake, a "teach me," and three "here's what happened" — you opened a shop from nothing. Money in the bank, a lift on the floor, your stake on the books, a loan to pay back. You never typed an account name or said "debit," and only named numbers you knew. Next, the doors open.

---

## 6. The Daily Rhythm — Sales, Invoices & Bills

This is where you'll do most of your bookkeeping, and it's all the same move: describe what happened. Sales, invoices, vendor bills, a card swipe, payroll, rent.

#### T4 — Moe pays cash, $450

First customer: Moe Szyslak rolls in with a slipping transmission. You rebuild it; he pays four hundred fifty in cash into the till. The simplest sale there is.

**You say:** "Moe paid cash for the $450 transmission job."

Cash came in (the till, an Asset, goes up) and you earned it doing labor (Income). One number from you; the income side balances itself. *(`ledger_record("2026-06-04", "Moe — transmission job, cash", [{Assets:Cash:Drawer, +45000}, {Income:Service:Labor (elided → −45000)}])`. Income is stored negative under the hood; the assistant flips it right-side-up, so you only ever hear "$450 in revenue." More on that sign convention in the reference.)*

**You get back:** "Got it — $450 in the till from Moe, booked as labor revenue."

#### T5 — Deposit the till, $450

End of day, you walk the cash to the bank.

**You say:** "I deposited Moe's $450 from the till into the bank."

Like the lift, this is an asset-to-asset shuffle: money leaves one pocket (the till) and lands in another (the bank). No income — Moe's payment was booked as revenue when he handed it over; booking it again would double-count. *(`ledger_record("2026-06-04", "Deposit cash to bank", [{Assets:Bank:Checking, +45000}, {Assets:Cash:Drawer, −45000}])`. The till goes back to $0; the bank rises $450.)*

#### T6 — Invoice Ned Flanders, $309 on account

Ned needs a brake job but doesn't have cash on him; he'll pay next week. So you do the work and send him a bill — an **invoice**.

> **An invoice is a sale where the money is owed to you instead of in hand.** What a customer owes you is itself an Asset, a **receivable**. You record it like a cash sale, except the cash side becomes "Ned owes me."

The brake job breaks into **$200 labor**, **$100 parts**, and **$9 sales tax** on the parts — three pieces balanced against the $309 Ned owes.

**You say:** "Invoice Ned Flanders for the brake job — $200 labor, $100 parts, plus $9 sales tax. He'll pay later."

The assistant opens a receivable for Ned for $309. Against it: $200 labor income, $100 parts income, and $9 of sales tax. That last one matters — the tax was never yours; you collected it for the state, so it's a Liability, not income. *(`ledger_record("2026-06-07", "Ned Flanders — brake job (invoice)", [{Assets:Receivable:Flanders, +30900}, {Income:Service:Labor, −20000}, {Income:Service:Parts, −10000}, {Liabilities:SalesTaxPayable, −900}])`. Four legs summing to zero.)*

**You get back:** "Invoiced Ned $309 — $200 labor, $100 parts, $9 sales tax. It's on his tab until he pays."

You described the shape of the job — labor, parts, tax — and the assistant mapped each piece to a bucket. You never said "receivable" or "liability."

#### T7 — Ned pays, $309

A week later, Ned mails a check.

**You say:** "Ned paid his $309 invoice."

Money lands in the bank (Asset up), and his receivable drops to zero (Asset down by the same amount). No income here — you booked the labor, parts, and tax when you did the work; this is the receivable turning into cash. *(`ledger_record("2026-06-12", "Ned Flanders — payment", [{Assets:Bank:Checking, +30900}, {Assets:Receivable:Flanders (elided → −30900)}])`.)*

A receivable opens when you invoice and closes when they pay. Ned's was born June 7 and died June 12. The next customer won't be so prompt.

#### T8 — Invoice Mr. Burns, $1,854 on account

Mr. Burns's Bentley needs an engine overhaul: **$1,200 labor**, **$600 parts**, **$54 sales tax** — $1,854 on account. This is the invoice we follow to the end of the guide, because Burns is going to make us wait.

**You say:** "Invoice Mr. Burns for the engine overhaul — $1,200 labor, $600 parts, $54 sales tax, on account."

Same shape as Ned's, bigger numbers, a different tab. Burns owes $1,854; against it sit $1,200 labor, $600 parts, and $54 collected tax. *(`ledger_record("2026-06-18", "Mr. Burns — engine overhaul (invoice)", [{Assets:Receivable:Burns, +185400}, {Income:Service:Labor, −120000}, {Income:Service:Parts, −60000}, {Liabilities:SalesTaxPayable, −5400}])`.)*

**You get back:** "Invoiced Mr. Burns $1,854 — $1,200 labor, $600 parts, $54 tax. It's on his tab."

Burns's tab is open, and unlike Ned's, it's going to stay open. We'll check on it at month-end.

#### T9 — Parts on account from Crazy Vaclav, $600

You needed parts for Burns's overhaul, and Vaclav sent a bill instead of making you pay on the spot. A vendor bill is the mirror of an invoice: instead of someone owing you, you owe someone.

**You say:** "Crazy Vaclav sent a $600 bill for parts. I'll pay him later."

The parts are an **Expense** the moment you buy them — this ledger has no inventory, so parts are expensed right away. Since you haven't paid, the other side is a **payable** — a Liability, the money you owe Vaclav. *(`ledger_record("2026-06-20", "Crazy Vaclav — parts (on account)", [{Expenses:Parts, +60000}, {Liabilities:Payable:Vaclav (elided → −60000)}])`.)*

> **A bill is an invoice in reverse.** An invoice = a customer owes you (a receivable, an Asset). A bill = you owe a vendor (a payable, a Liability). Same machinery, mirror direction. You say "they'll pay me later" or "I'll pay them later"; the assistant picks the side.

#### T10 — Brake pads on the shop card, $120

You grab a box of brake pads on the shop credit card.

**You say:** "I bought $120 of brake pads on the shop credit card."

Parts again, so an **Expense** the moment you buy them. The difference is how you paid: the other side is your **credit-card balance**, a Liability that goes up $120. *(`ledger_record("2026-06-22", "Brake pads — shop card", [{Expenses:Parts, +12000}, {Liabilities:CreditCard (elided → −12000)}])`.)*

#### T11 — Pay Vaclav's bill, $600

**You say:** "I paid Crazy Vaclav's $600 bill from the bank."

The payable from T9 closes: you owe Vaclav $600 less (the Liability goes down), and the bank goes down $600. No new expense — you already expensed the parts when you got them; this just settles the debt. *(`ledger_record("2026-06-25", "Crazy Vaclav — pay bill", [{Liabilities:Payable:Vaclav, +60000}, {Assets:Bank:Checking, −60000}])`.)*

#### T12 — Wages for Lenny & Carl, $2,000

**You say:** "Paid Lenny and Carl their wages — $2,000 total, from the bank."

Wages are an **Expense** — money spent and gone, unlike the lift, which you could resell. The bank drops $2,000. *(`ledger_record("2026-06-27", "Wages — Lenny & Carl", [{Expenses:Wages, +200000}, {Assets:Bank:Checking, −200000}])`.)*

#### T13 — Rent for June, $900

**You say:** "Paid June rent — $900, by check from the bank."

Rent's an **Expense**; the bank covers it. *(`ledger_record("2026-06-28", "Shop rent — June", [{Expenses:Rent, +90000}, {Assets:Bank:Checking, −90000}])`.)*

Keep an eye on this rent check. You wrote it June 28, but the landlord won't deposit it for a couple of weeks. That gap matters when we reconcile.

#### T14–T16 — A fat-fingered utility bill, corrected

You go to record the June power bill. Utilities ran **$150**, but it's late and you fat-finger it as **$1,500**.

**You say:** "Log June utilities — $1,500, from the bank."

The assistant records exactly what you said — garbage in, garbage faithfully recorded. *(T14: `ledger_record("2026-06-30", "Utilities — June", [{Expenses:Utilities, +150000}, {Assets:Bank:Checking, −150000}])`. That's `150000` cents — fifteen hundred dollars, wrong by a factor of ten.)*

You realize nobody's power bill is $1,500, so you pull it up.

**You say:** "Pull up that utilities entry I just made — let me see it."

The assistant fetches the full transaction so you can eyeball it: both legs, the amounts, the date. *(`ledger_get(<the wrong entry's id>)` returns every posting, its status, and any reversal links. You never type the id — the assistant tracks which entry "that utilities one" means.)*

You can't edit it. There's no "change $1,500 to $150," no edit at all, and no delete — the journal is permanent. So you reverse it and record it right.

**You say:** "That's wrong — it should be $150, not $1,500. Reverse it and fix it."

First the reversal: the assistant posts the sign-flipped mirror — $1,500 back into the bank, $1,500 of utilities expense removed — so the original and its mirror net to zero. *(T15: `ledger_reverse(<that entry's id>)` → a linked mirror `[{Expenses:Utilities, −150000}, {Assets:Bank:Checking, +150000}]`. The two are cross-linked (`reverses_id` / `reversed_by_id`), and the mirror's legs come back `pending`.)*

Then the correction, recorded the way it should have been. *(T16: `ledger_record("2026-06-30", "Utilities — June (corrected)", [{Expenses:Utilities, +15000}, {Assets:Bank:Checking, −15000}])`. That's `15000` cents — one hundred fifty dollars.)*

**You get back:** "Done. I reversed the $1,500 entry and re-recorded utilities at $150. All three are on the books — the original, the reversal, and the fix — so the trail's honest. Your real June utilities expense is $150."

The bad entry and its reversal cancel to zero and represent no real bank movement — the money never actually left — so they stay `pending` and won't muddy the reconciliation we're about to do.

Two weeks of business, all captured by talking: a cash sale and its deposit, two invoices (one paid, one still open), a vendor bill paid off, a card swipe, payroll, rent, and a fat-fingered utility bill caught and corrected. Every entry balanced to zero without you once saying "debit." Next, the month closes.

---

## 7. End of Month — Reconcile & Report

June's over. You check your records against the bank, then ask the books what they've learned. Both are things you do by asking — there's no "close the month" button.

### Reconciling the bank

Your books say one thing about your bank balance; Springfield Savings' statement says another. A **bank reconciliation** makes them agree and explains any difference. Each posting's status tracks where it is: `pending` (recorded, not yet confirmed), `cleared` (seen on the statement), `reconciled` (matched and locked in).

**List the bank's history.** You say: "Show me everything that's hit checking this month." The assistant pulls every posting that touched the bank, in date order, with a running balance, so you can tick down the list against the paper statement. It also grabs each line's internal handle (its `posting_id`) for the next step. *(`ledger_register(query:"Assets:Bank")`. The substring `Assets:Bank` matches every bank sub-account — here just `Assets:Bank:Checking`.)*

Everything matches the statement — the capital, the loan, the lift, Moe's deposit, Ned's payment, the Vaclav payment, wages, the corrected utilities — except the **$900 rent check (T13)**: you wrote it June 28, but the landlord hasn't cashed it. The fat-fingered utilities entry and its reversal show up too, but they net to zero and were never real money, so they stay `pending`.

**Mark the cleared items.** You say: "Everything cleared the bank except the rent check. Mark the rest as cleared." The assistant flips every real bank movement that showed up on the statement from `pending` to `cleared`. This is the one thing in the ledger allowed to change an existing row, and it touches only the status — never an amount, account, or date. *(`ledger_reconcile([<cleared bank posting_ids>], "cleared")`. All-or-nothing: if even one posting_id is bad, the whole call fails, so you never end up half-done.)*

**Read the difference.** You say: "What's my checking balance on the books versus what's actually cleared?" The assistant runs the bank balance two ways:

- **Ledger balance** (everything): **$14,109** *(`ledger_balance(query:"Assets:Bank")`)*
- **Cleared balance** (only what's hit the bank): **$15,009** *(`ledger_balance(query:"Assets:Bank", status:"cleared")`)*

The cleared balance is higher by exactly **$900** — the outstanding rent check. The bank still thinks you have that $900 because the landlord hasn't cashed the check; when he does next month, the two will agree. One outstanding item explaining the whole difference is what a clean reconciliation looks like.

**Lock it in.** You say: "Great, it reconciles. Lock in the cleared items as reconciled." The assistant walks those same postings from `cleared` to `reconciled`. The rent check stays `pending` — it hasn't cleared, so there's nothing to reconcile yet; it gets its turn in July. *(`ledger_reconcile([...], "reconciled")`. Transitions are free in any direction.)*

Every bank entry started `pending`, became `cleared` when you saw it on the statement, and locked as `reconciled` when the balance matched — except the rent check, which waits until July.

### The reports

There's no "reports" feature. Every report is a question — the same balance and register reads, pointed at the right slice. The assistant knows the recipes; you just ask.

**Who owes me?** You say: "Who still owes me money?" The assistant totals every customer tab. Ned's is closed; Burns isn't. *(`ledger_balance(query:"Assets:Receivable")` — each customer has their own sub-account, so the per-customer breakdown falls out for free.)*
> "One open tab: **Mr. Burns owes $1,854.** Ned Flanders is paid up. Total receivables: **$1,854.**"

**Mr. Burns's statement.** You say: "Show me Mr. Burns's account history." The assistant pulls his tab chronologically with a running balance — which is what a customer statement is. *(`ledger_register(query:"Assets:Receivable:Burns")`.)*
> "**Mr. Burns — statement:** Jun 18 — Engine overhaul (invoice): **$1,854** · running balance **$1,854**. Still open, no payments yet."

**June's profit and loss.** You say: "Did I make a profit in June? Income versus expenses." *(`ledger_balance(query:"Income", period:"2026-06")` → revenue **$2,550**; `ledger_balance(query:"Expenses", period:"2026-06")` → expenses **$3,770**. Net income in raw signed terms is −(Income + Expenses) = −(−$2,550 + $3,770) = **−$1,220**; human-side-up, that's $2,550 − $3,770 = a **$1,220 loss**. The assistant always shows the human-side-up version.)*
> "June revenue was **$2,550**, expenses were **$3,770** — a **$1,220 loss** for the month. That's normal for month one: you bought a lift, paid two mechanics for a full month, covered rent, and billed work you haven't been paid for yet. Revenue isn't profit."

**The balance sheet, and a small mystery.** You say: "Give me a balance sheet — what do I own, owe, and what's my stake?" *(`ledger_balance(query:"Assets")` = **$23,963** (Bank $14,109 + Receivable $1,854 + Equipment $8,000 + Cash $0); `(query:"Liabilities")` = **$15,183** (Loan $15,000 + Card $120 + SalesTax $63); `(query:"Equity")` = **$10,000**.)*
> "**Balance sheet, right now:**
> - **You own:** $23,963 — $14,109 in the bank, $1,854 owed by Burns, the $8,000 lift, nothing in the till.
> - **You owe:** $15,183 — the $15,000 loan, $120 on the card, $63 of sales tax due the state.
> - **Your stake:** $10,000 — the capital you put in."

There's a puzzle here. Your stuff is worth **$23,963**, but your stake plus your debts is $10,000 + $15,183 = **$25,183**. Those are off by **$1,220** — June's loss. That loss is still sitting inside the Income and Expense accounts; it hasn't been moved into Equity yet. Properly, your stake including the month's result is $10,000 − $1,220 = **$8,780**, and then it ties: Liabilities $15,183 + Equity $8,780 = **$23,963**. The books were never wrong; the loss just hadn't been folded into your stake.

> **This is why bookkeepers "close the books."** Income and Expenses hold the running result until you sweep it into Equity. Until then, the balance-sheet equity line shows your *contributed* stake, not stake-plus-or-minus-profit. That gap of unclosed profit is the whole reason the year-end chapter exists.

For a quick gut check, "give me the five buckets, top level" rolls every account up to its root and reads back the five totals, from which net worth (Assets + Liabilities + Equity) falls out. *(`ledger_balance(depth:1)`.)*

### T17 — Burns finally pays (July)

Fast-forward into July. The June rent check clears, and Mr. Burns pays his bill — three weeks late and after a few reminders, but the check is good.

**You say:** "Mr. Burns finally paid his $1,854 invoice."

The receivable open since June 18 closes. Money lands in the bank; Burns's tab drops to zero. No income — you booked the labor, parts, and tax in June; this is the tab turning into cash. *(`ledger_record("2026-07-09", "Mr. Burns — payment", [{Assets:Bank:Checking, +185400}, {Assets:Receivable:Burns (elided → −185400)}])`.)*

**You get back:** "Mr. Burns paid $1,854 — it's in the bank, and his tab is closed."

Ask for his statement now and the whole arc is there:
> **Mr. Burns — statement:** Jun 18 — Engine overhaul: $1,854 · running **$1,854** · Jul 9 — Payment: −$1,854 · running **$0**

That deposit posts `pending`; when it shows on July's statement you'll mark it `cleared`, then `reconciled` — the same lifecycle. (And the June rent check, which finally cleared in July, reconciles on the July statement, closing the $900 gap for good.)

You reconciled June to the penny, pulled a receivables list, a customer statement, a P&L, and a balance sheet, and watched one invoice live its full life. Every report was just a question. One remains: what happens at year-end?

---

## 8. End of Year — Closing the Books

Jump ahead to **December 31, 2026**. Greased Lightning has been open seven months, and the numbers from here are full-year totals — not the June slice we followed step by step. (Skip the hundreds of entries in between; they happened the same plain way, one "here's what happened" at a time.) The shop found its feet.

**You say:** "Show me the whole year — total income and expenses for 2026."

Same P&L recipe as June, over the full year. *(`ledger_balance(query:"Income", period:"2026")` → revenue **$82,000** (raw `−8,200,000`); `ledger_balance(query:"Expenses", period:"2026")` → expenses **$61,000** (raw `+6,100,000`). Net profit = −(Income + Expenses) = −(−$82,000 + $61,000) = **$21,000**.)*

**You get back:** "Full-year revenue: **$82,000**. Expenses: **$61,000**. The shop cleared a **$21,000 profit** for 2026."

That profit is real, but right now it's trapped inside the Income and Expense accounts. Your stake on the balance sheet still reads $10,000 — your original capital — as if the shop earned nothing all year. To make the books tell the truth about your stake, you **close the year**.

#### Temporary vs. permanent

- **Permanent accounts** — Assets, Liabilities, Equity. They carry their balances forever. The bank balance on December 31 is the bank balance on January 1.
- **Temporary accounts** — Income and Expenses. They measure one year's activity and get swept to zero at year-end so next year starts fresh. They're the year's scoreboard; at the buzzer you record the score and reset the board.

Closing the books is that reset: you zero every Income and Expense account and roll the net result — your $21,000 profit — into a permanent Equity account, **retained earnings** (bookkeeper-speak for profit the business has kept).

#### T18 — The closing entry

**You say:** "Close out the year. Sweep 2026's profit into retained earnings."

The assistant reads the year-end balance of every Income and Expense account and writes **one balanced transaction** that empties them all: it pushes each Income and Expense account to zero, and the difference — the $21,000 — lands in retained earnings. Same elision trick as day one: it names the amounts it's clearing and lets the retained-earnings leg balance itself to whatever's left, which is, by definition, the profit. *(Shown at the totals level for readability; the real call has one offsetting leg per Income and Expense account: `ledger_record("2026-12-31", "Close FY2026 to retained earnings", [ +8,200,000 across the Income accounts, −6,100,000 across the Expense accounts, {Equity:RetainedEarnings (elided → −2,100,000)} ])`. The elided leg absorbs the **$21,000** net profit (raw `−2,100,000`). The whole thing sums to zero.)*

**You get back:** "Year closed. I zeroed out your income and expense accounts and moved the **$21,000** profit into retained earnings. Your books are ready for 2027, and your equity finally reflects the year you had."

Ask for the reports now and watch what changed:

- **Income and Expenses** are back to **$0** — the temporary accounts reset.
- **`Equity:RetainedEarnings`** holds **$21,000** — the year's profit, made permanent.
- **Equity on the balance sheet** now reads OwnerCapital $10,000 + RetainedEarnings $21,000 = **$31,000** — your true stake: what you put in, plus the profit you earned and kept. No more gap.

#### One caveat

Closing is a practice, not a requirement of this ledger. Because every report here is date-filtered, you can ask for any year's P&L anytime (`period:"2026"`, `period:"2027"`) and get a clean answer, closed or not. So why close? For one reason: so the balance sheet's equity reflects your accumulated profit. Without it, your stake forever reads as just the cash you contributed, and the year's earnings hang in limbo in the temporary accounts. There's no fiscal-period lock and no penalty — closing is the standard practice that makes your net worth read true. You do it once a year, in one sentence.

That's the year. You opened a shop from nothing, ran a month of real business entirely by saying what happened, fixed a mistake without erasing anything, reconciled to the penny, pulled every report a shop owner needs, watched one stubborn invoice live its full life from June to July, and closed the year with a $21,000 profit folded into your stake. You brought the story; the assistant brought the precision.

---

## 9. Say It Your Way

You never learn the ledger's words. There are eight things it can do, and the assistant maps whatever you actually said onto one of them. This is a phrasebook: the same intent, many ways to say it, all landing on the same eight tools — `ikigenba_ledger_record`, `ikigenba_ledger_reverse`, `ikigenba_ledger_reconcile`, `ikigenba_ledger_balance`, `ikigenba_ledger_register`, `ikigenba_ledger_get`, `ikigenba_ledger_describe`, `ikigenba_ledger_health`.

> **Loose in, corrective out.** Phrase the request like someone who runs an auto shop, not an accountant. If something's off — the two sides don't add up, an account's root isn't one of the five, a date isn't a real day, a transaction doesn't exist — the service hands back a plain typed error (`unbalanced`, `bad_root`, `validation`, `not_found`, `already_reversed`), and the assistant fixes the entry and retries. You usually never see the round trip.

**Recording what happened** (`ikigenba_ledger_record`) — every sale, bill, paycheck, and deposit:

- "Moe paid cash for the $450 transmission job."
- "We did a $450 transmission job for Moe, cash."
- "Bill Ned Flanders $309 for the brake job — $200 labor, $100 parts, $9 sales tax."
- "Invoice Mr. Burns for the engine overhaul, $1,854 on account."
- "Lenny and Carl's pay this period was $2,000."
- "Deposit the $450 from the drawer into checking."

Say the number you know and the other side balances itself — that's **elision**. Exactly one leg of a transaction can leave its amount blank; the ledger fills it with whatever balances. Name two unknowns and there's nothing to solve for (`validation`).

A bill or invoice is still just `ikigenba_ledger_record`: an invoice puts the amount into `Assets:Receivable:<customer>` (owed to you), a bill into `Liabilities:Payable:<vendor>` (you owe), a card purchase into `Liabilities:CreditCard`, and getting paid moves money from the receivable into the bank. You say "invoice," "bill," "on account," "they paid"; the assistant routes each. There's no invoice object to manage — there's the journal, and the journal remembers everything.

**Fixing a mistake** (`ikigenba_ledger_reverse`) — you can't edit or delete, so you reverse and re-record:

- "I fat-fingered the utilities bill — that should've been $150, not $1,500. Fix it."
- "Undo that last entry, it's wrong."
- "Scrap that one and re-enter it correctly."

The assistant reverses the wrong transaction (posting its sign-flipped mirror, the mirror's legs reset to `pending`), then re-records the right numbers. All three entries stay. A transaction can be reversed only once; reverse an already-reversed one and you get `already_reversed` (reverse its mirror instead).

**Reconciling against a statement** (`ikigenba_ledger_reconcile`) — touches only a posting's status:

- "These all cleared on the June statement — mark them cleared."
- "The rent check hasn't hit the bank yet, leave it."
- "Everything ties out — lock June in as reconciled."

It's all-or-nothing: one bad posting fails the whole batch (`not_found`). The assistant pulls the postings and grabs their ids; you never touch an id.

**Asking "what's the balance?"** (`ikigenba_ledger_balance`) — your trial balance, balance sheet, net worth, and A/R, all one read:

- "What's in the checking account?"
- "How much does Burns owe me?"
- "Who owes me money right now?"
- "What did we make in June?" · "What did we spend in June?"
- "What's actually cleared the bank?"

"Who owes me" and "what's in checking" are the same verb, just a different slice. The account match is a plain substring, so "Receivable" finds every customer's tab at once.

**Asking "what happened, in order?"** (`ikigenba_ledger_register`) — every matching posting in date order with a running total: a customer statement, an account history, a search:

- "Show me Burns's account history."
- "Walk me through everything that hit checking."
- "Show me every parts purchase this year."

**Looking one transaction up** (`ikigenba_ledger_get`) — all its legs, each leg's status, whether it's been reversed; usually right before a correction:

- "Pull up that utilities transaction."
- "Let me see both sides of that one before I fix it."

**Finding your footing** (`ikigenba_ledger_describe`) — the assistant's first call: the five account types, the money unit, the reconciliation states, the live account list, and the report recipes:

- "What can this thing track?"
- "How do the books work here?"

**The handshake** (`ikigenba_ledger_health`) — reports who the platform sees you as; changes nothing:

- "Am I connected to my ledger?"
- "Who does it think I am?"

You don't memorize any of this — the assistant does. You keep talking like a shop owner.

---

## 10. Reference — Tools, Fields, Gotchas & FAQ

You run the ledger by describing what happened; this section is the precise reference for when you want to know exactly how the books behave — the kind of thing you'd double-check before quoting a number to the bank. You never type any of it: the account paths, signs, and tool names below are the vocabulary the assistant speaks on your behalf.

### The eight tools

There is **one write entity — the balanced transaction** — and everything else is a read. The surface is a function of verbs, not entities, and this set of eight never grows.

| Tool | Signature | What it does |
|---|---|---|
| `ikigenba_ledger_record` | `(date, description, postings[], status?)` | Post one balanced transaction (2+ postings summing to zero). Returns the full transaction with the resolved residual and assigned ids. |
| `ikigenba_ledger_reverse` | `(id, date?, memo?)` | Post the sign-flipped mirror of a transaction, linked both ways. The correction primitive. Returns the mirror. |
| `ikigenba_ledger_reconcile` | `(posting_ids[], status)` | Transition the reconciliation status of one or more postings — the only mutation of existing rows. Returns the affected transactions. |
| `ikigenba_ledger_balance` | `(query?, period?, depth?, status?)` | The `bal` report and live chart of accounts. Returns `{lines:[{account, amount_cents}], total, unit}`. |
| `ikigenba_ledger_register` | `(query?, period?, status?)` | The `reg` report: matched postings in chronological order with a running total. |
| `ikigenba_ledger_get` | `(id)` | Fetch one transaction in full (all postings, per-posting status, order, reversal links). |
| `ikigenba_ledger_describe` | `()` | Discovery — the five roots, the unit, the reconciliation states, the live account tree, and report recipes. The first call an agent makes. |
| `ikigenba_ledger_health` | `()` | The authenticated caller's identity (owner email + client id). The end-to-end auth proof. |

There is no `create_account`, `ledger_report`, `ledger_delete`, or `ledger_update`, and no invoice / bill / customer / vendor entity. Accounts are emergent (they appear on first posting); reports come from recipes over `balance` and `register`.

**`ikigenba_ledger_record` postings.** Each posting is `{account, amount_cents, status?}`. There must be **2 or more**, and the signed `amount_cents` (debit `+`, credit `−`) must **sum to exactly zero**. Transaction-level and posting-level `status?` is one of `pending` / `cleared` / `reconciled`, defaulting to `pending`.

### The five account roots

Accounts are emergent colon-paths (`Assets:Bank:Checking`): they appear the first time you post to them, and none is ever "created" as a step. The only guardrail is that the **root** (before the first colon) must be one of these five. Sub-paths below the root are free-form. `Revenue` is an alias of `Income`; root alias and case are folded so the tree can't fork, while sub-path case is preserved as you wrote it.

| Root | Normal balance | Feeds statement | Stored sign | Plain English |
|---|---|---|---|---|
| `Assets` | debit | balance sheet | **positive** | what you own |
| `Liabilities` | credit | balance sheet | **negative** | what you owe |
| `Equity` | credit | balance sheet | **negative** | your stake |
| `Income` (alias `Revenue`) | credit | income statement (P&L) | **negative** | what you earn |
| `Expenses` | debit | income statement (P&L) | **positive** | what you spend |

A `bad_root` error means an account named a root that isn't one of these five.

### The reconciliation states

A status lives on each **posting** and defaults to `pending`. Transitions are free in any direction, including backward.

| Status | Meaning |
|---|---|
| `pending` | Recorded but not yet confirmed against an external source (the default). |
| `cleared` | Confirmed to have cleared the account — e.g. seen on the bank/card statement. |
| `reconciled` | Matched against an official statement balance and locked in. |

The lifecycle you'll walk for the bank: `pending → cleared → reconciled`.

### The sign / zero convention

- Money is **integer USD cents**, single currency. `$450.00` = `45000`; `$1,854.00` = `185400`. The unit string the tools return is `"USD cents"`.
- **Debit `+`, credit `−`**, stored raw and signed with no normalization.
- A single transaction's postings sum to **0**, and a balance over *every* account also sums to **0** — that whole-ledger `total: 0` is a free correctness check on every `ikigenba_ledger_balance`.
- Reads return raw signed sums (ledger-cli convention): Assets and Expenses come back positive; Liabilities, Equity, and Income come back negative. The assistant flips the signs using each root's normal balance to show numbers human-side-up (revenue and expenses read as positive dollars).

### The elision rule

Exactly **one** posting in a `ikigenba_ledger_record` may omit its `amount_cents`; that leg receives the **balancing residual** — the negation of everything else, so the transaction nets to zero. Omitting the amount on two or more postings is a `validation` error (there's nothing to solve for). This is why "deposit the $450" or "put $10,000 of capital in" needs only the single number you know.

### Immutability & reverse

The journal **never changes** — no edit, no delete. A mistake is fixed with `ikigenba_ledger_reverse`, which:

- posts a new transaction whose legs are the **sign-flipped mirror** of the original (whole transaction only, never a partial leg),
- links the two both ways (`reverses_id` on the mirror, `reversed_by_id` on the original),
- resets the mirror's legs to `pending` (a reversal hasn't cleared anything),
- and defaults the mirror's description to `"Reversal of: <original>"` (override with `memo?`) and its date to the original's (override with `date?`).

The original, the reversal, and the correction all stay in the journal. A transaction can be reversed only once; reversing an already-reversed one returns `already_reversed` (reverse its mirror instead). The lone row-mutating exception is `ikigenba_ledger_reconcile`, which touches **status only** — never an amount, account, or date.

### Filters: period, query, depth, status

These narrow the `ikigenba_ledger_balance` and `ikigenba_ledger_register` reads.

**`period`** — a **bucket string** or an inclusive **range object**:

| Form | Example | Means |
|---|---|---|
| Year | `"2026"` | all of 2026 |
| Month | `"2026-06"` | all of June 2026 |
| Day | `"2026-06-01"` | that single day |
| Range | `{"from":"2026-06-01","to":"2026-06-30"}` | inclusive `from`…`to` |

Omit `period` for "all time" / "now."

**`query`** — a **case-insensitive substring** matched against the full account path. `"Receivable"` matches `Assets:Receivable:Burns` and `Assets:Receivable:Flanders`; `"Bank"` matches every bank sub-account. Omit it to match every account. It's a literal substring, not fuzzy search, but the assistant rephrases for you.

**`depth`** — roll accounts up to *N* colon-levels. `depth:1` collapses everything to the five roots (the net-worth roll-up); omit it or pass `0` for full leaf accounts as posted.

**`status`** — restrict to postings in one reconciliation state. `status:"cleared"` over a bank account is exactly the cleared-vs-ledger reconciliation view.

### The recipes (reports without report tools)

Every statement is a recipe over `balance` / `register` — there is no report tool. These are the ones published via `ikigenba_ledger_describe`:

| Report | How |
|---|---|
| **Balance sheet** | `ledger_balance(query:"Assets")`, `(query:"Liabilities")`, `(query:"Equity")` — omit period for "now," or `{to:DATE}` for a point in time. |
| **Income statement (P&L)** | `ledger_balance(query:"Income", period:P)` and `(query:"Expenses", period:P)`; **net income = −(Income + Expenses)** (Income is stored negative). |
| **Net worth** | `ledger_balance(depth:1)`, then sum Assets + Liabilities + Equity (raw signed, so just add them). |
| **A/R — who owes me** | `ledger_balance(query:"Assets:Receivable")` — outstanding balance per customer sub-account. |
| **Customer statement** | `ledger_register(query:"Assets:Receivable:<customer>")` — chronological charges, payments, and running A/R balance. |
| **Bank reconciliation** | `ledger_balance(query:"Assets:Bank", status:"cleared")` vs. the same without a status filter; the difference is the uncleared items. |

From June: `ledger_balance(query:"Assets:Bank")` reads **$14,109** (the ledger balance), while `(query:"Assets:Bank", status:"cleared")` reads **$15,009** (cleared) — the **$900** difference is the one outstanding rent check that hadn't hit the bank yet.

### The error vocabulary

These are typed; the assistant catches each and fixes the entry before you'd notice.

| Error | Means |
|---|---|
| `unbalanced` | the postings don't sum to zero |
| `bad_root` | an account's root isn't one of the five known types |
| `validation` | too few postings, more than one elided amount, a malformed account path, a bad date, or an unknown status |
| `not_found` | no transaction or posting with that id |
| `already_reversed` | the target transaction already has a reversal mirror |

### FAQ & gotchas

**Do I need account codes or a chart of accounts to set up first?**
No. Accounts are emergent — `Assets:Bank:Checking` exists the moment you first post to it. You describe what happened in plain English; the assistant picks the account paths. The only rule is that an account's root has to be one of the five types.

**Can I edit or delete a transaction I already recorded?**
No — the journal is immutable, no edits and no deletes. To fix a mistake you reverse it (a linked, sign-flipped mirror that cancels it out) and re-record it correctly, exactly like the utilities fat-finger in the story. All three entries — the mistake, the reversal, the fix — stay on the books. The one thing you can change on an existing row is a posting's reconciliation status, via `ikigenba_ledger_reconcile`.

**Why is income negative under the hood?**
The ledger stores raw signed amounts ledger-cli style: debits `+`, credits `−`. Revenue is credit-normal, so `Income` accounts sum to a negative number internally — a sign convention, not a loss. The assistant flips it so revenue reads as positive dollars. The same is true of Liabilities and Equity (stored negative); Assets and Expenses are stored positive. A handy consequence: net income is `−(Income + Expenses)`, and a balance over every account sums to exactly zero.

**What does "closing the books" mean?**
At year-end you sweep every `Income` and `Expenses` account to `$0` and roll the year's net profit into `Equity:RetainedEarnings`, with one balanced `ikigenba_ledger_record` the assistant computes from the year's totals. In the story, FY2026's **$21,000** profit moves into retained earnings (the elided leg absorbs it) and the temporary accounts reset to zero. Because this ledger's reports are date-filtered, your P&L for any year is always available via `period:"YYYY"` regardless — so there's no fiscal-period lock and no "close" tool. You close mainly so the balance sheet's equity reflects retained earnings; it's a standard practice, not a feature.

**Is there a screen, an app, or a forms-based dashboard?**
No — and that's the point. No buttons, no forms, no debit/credit columns to fill in. The ledger is the eight things the assistant can do on your behalf, driven by you describing what happened in plain language. This appendix exists for precision, not because you'll ever type any of it.
