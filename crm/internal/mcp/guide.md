# CRM usage guide

## Entity model

- organization: a company account. Contacts, deals, tasks, and interactions can refer to an organization.
- contact: a person. Contacts can belong to an organization, participate in deals, have tags, and own emails or phones.
- deal: a pipeline opportunity. Deals can belong to an organization and have participating contacts.
- task: a to-do item. Tasks can point at a contact, organization, or deal.
- interaction: a timeline entry such as a note, call, email, or meeting. Interactions are append-only through `log`.

## Field catalog

- organization: `name` (required on create), `domain` (website/email domain; drives dedup).
- contact: `given_name`, `family_name`, `display_name` (derived if absent), `org_id`, `title`, `lifecycle` (`subscriber`, `lead`, `opportunity`, `customer`; default `lead`), `emails` (`[{ "email": "...", "label": "..." }]`, first is primary), `phones` (`[{ "phone": "...", "label": "..." }]`, E.164, first is primary), `tags` (`["newsletter"]`).
- deal: `name` (required on create), `org_id`, `stage` (`lead`, `qualified`, `proposal`, `negotiation`, `won`, `lost`; default `lead`), `amount_cents` (integer), `currency` (default `USD`), `close_date` (RFC3339 date), `contacts` (`[{ "id": "...", "role": "..." }]` participants).
- task: `title` (required on create), `status` (`open`, `done`; default `open`), `due_at`, `done_at`, `contact_id`, `org_id`, `deal_id` (optional subject). Complete a task with `fields: { "status": "done" }`.

Interactions are not saved with `save`; use `log`.

## Basics

Create a company:

```json
{"name":"save","arguments":{"type":"organization","fields":{"name":"Acme","domain":"acme.com"}}}
```

Create a contact at that company:

```json
{"name":"save","arguments":{"type":"contact","fields":{"given_name":"Ada","family_name":"Lovelace","org_id":"01HORG...","emails":[{"email":"ada@example.com","label":"work"}],"tags":["newsletter"]}}}
```

Find records:

```json
{"name":"search","arguments":{"type":"contact","query":"Ada"}}
```

Log a call on the contact timeline:

```json
{"name":"log","arguments":{"subject_id":"01HCONTACT...","kind":"call","body":"Discussed renewal timing."}}
```

Complete a task:

```json
{"name":"save","arguments":{"type":"task","id":"01HTASK...","fields":{"status":"done"}}}
```

## Advanced

- Dedup and `force`: creating a contact with an existing primary email, or an organization with a matching domain or exact name, returns a duplicate error with `existing_id`. Retry with `"force": true` only when you intentionally want a separate record.
- Set replacement: set-valued fields are declarative. Sending `emails`, `phones`, `tags`, or deal `contacts` replaces the whole set; omit the field to leave it untouched, or send `[]` to clear it.
- Deal `status`: deal `status` is derived from `stage` as `open`, `won`, or `lost`; it is read-only. Change the `stage` instead.
- Filtered search: use `search` filters such as `{"type":"deal","filters":{"stage":"proposal"}}`, `{"type":"task","filters":{"status":"open"}}`, or `{"type":"interaction","filters":{"subject_id":"01HCONTACT..."}}`.
- Correcting an interaction: interactions are append-only. To correct one, call `delete` with `{"type":"interaction","id":"01HINTERACTION..."}` and then call `log` again with the corrected details.
