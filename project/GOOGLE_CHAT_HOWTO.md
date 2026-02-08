# Google Chat Integration Setup

This guide sets up a Google Chat channel between a human user and an agent. The agent (ikigai@logic-refinery.com) uses domain-wide delegation to read and send messages as itself.

## Architecture

```
┌─────────────────────────────────────────┐
│           Shared Google Chat Space      │
├─────────────────────────────────────────┤
│  michael: "Deploy the new version"      │
│  ikigai:  "Starting deployment..."      │
│  ikigai:  "Done. Version 1.2.3 live."   │
└─────────────────────────────────────────┘
        ▲                       ▲
        │                       │
   Human user              Agent script
   (browser/app)           (impersonates ikigai@)
```

## Prerequisites

- Google Workspace domain (e.g., logic-refinery.com)
- Admin access to Google Workspace
- Two user accounts:
  - Human user (e.g., michaelgreenly@logic-refinery.com)
  - Agent user (e.g., ikigai@logic-refinery.com)
- Ruby with gems: `google-apis-chat_v1`, `googleauth`

## Step 1: Create a Google Cloud Project

1. Go to [Google Cloud Console](https://console.cloud.google.com)
2. Select the project **ikigai-dev-1982**

## Step 2: Enable the Google Chat API

1. Go to **Menu (☰) → APIs & Services → Library**
2. Search for **Google Chat API**
3. Click it → **Enable**

## Step 3: Create a Service Account

Service account: `ikigai-dev@ikigai-dev-1982.iam.gserviceaccount.com`

1. Go to **Menu (☰) → IAM & Admin → Service Accounts**
2. Click **Create Service Account**
3. Fill in:
   - Name: `ikigai-dev`
   - ID: auto-generated
4. Click **Create and Continue**
5. Skip role assignment → **Continue** → **Done**

## Step 4: Create the JSON Key

1. Click on your new service account email
2. Go to the **Keys** tab
3. Click **Add Key → Create new key**
4. Select **JSON** → **Create**
5. Save the downloaded file to `~/.secrets/ikigai-dev-1982.iam.gserviceaccount.com`

## Step 5: Get the Client ID

Open the JSON key and find the `client_id` field. You'll need this for the next step.

```bash
grep client_id ~/.secrets/ikigai-dev-1982.iam.gserviceaccount.com
```

## Step 6: Configure Domain-Wide Delegation

1. Go to [Google Admin Console](https://admin.google.com)
2. Navigate to **Security → Access and data control → API controls**
3. Click **Manage Domain Wide Delegation**
4. Click **Add new**
5. Fill in:
   - **Client ID**: (the `client_id` from step 5)
   - **OAuth scopes**: `https://www.googleapis.com/auth/chat.messages`
6. Click **Authorize**

Note: Changes can take up to 24 hours but usually happen within minutes.

## Step 7: Create a Shared Space

1. Open [Google Chat](https://chat.google.com) as the human user
2. Click **+** next to Spaces → **Create space**
3. Name it **ikigai-dev**
4. Add the agent user (ikigai@logic-refinery.com) as a member

Space URL: `https://mail.google.com/chat/u/0/#chat/space/AAQAJRRg41M`
Space ID for API: `spaces/AAQAJRRg41M`

## Step 8: Install Dependencies

```bash
gem install google-apis-chat_v1 googleauth
```

## Step 9: Run the Scripts

```bash
export GOOGLE_APPLICATION_CREDENTIALS=~/.secrets/ikigai-dev-1982.iam.gserviceaccount.com
export GOOGLE_CHAT_SPACE='spaces/AAQAJRRg41M'

# List available spaces (run without GOOGLE_CHAT_SPACE set)
chat-read

# Read messages
chat-read

# Send a message
chat-send "Hello from ikigai"

# Or pipe input
echo "Deployment complete" | chat-send
```

## Files

| File | Purpose |
|------|---------|
| `~/.secrets/ikigai-dev-1982.iam.gserviceaccount.com` | Service account credentials |
| `.claude/scripts/chat-read` | Read messages from a space |
| `.claude/scripts/chat-send` | Send a message to a space |
| `.claude/data/chat-read-cursor` | Tracks last message read |

## Troubleshooting

### "Caller does not have permission"
- Domain-wide delegation not configured or not propagated yet (wait up to 24 hours)
- Wrong client ID in admin console
- Missing or wrong OAuth scope

### "Space not found"
- Agent user (ikigai@) is not a member of the space
- Wrong space ID

### Token errors
- Service account JSON file path is wrong
- JSON file is corrupted or incomplete

## Security Notes

- `service-account.json` grants access to impersonate the agent user — protect it
- Domain-wide delegation is powerful; scope it narrowly (only `chat.messages`)
- The agent can only access spaces where the impersonated user is a member

## References

- [Google Chat API](https://developers.google.com/workspace/chat)
- [Domain-wide delegation](https://support.google.com/a/answer/162106)
- [Ruby Client Library](https://googleapis.dev/ruby/google-api-client/latest/Google/Apis/ChatV1/HangoutsChatService.html)
