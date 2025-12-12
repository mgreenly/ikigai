# Managing Secrets with systemd-creds

This documents how we manage service secrets using systemd's encrypted credentials feature.

## Overview

- Secrets are encrypted at rest on disk
- Decrypted only at service start into a private namespace
- Bound to the host machine (encrypted files won't work on other machines)
- Requires systemd 250+

## Prerequisites

```bash
# Verify systemd version
systemctl --version  # Need 250+

# Create credential storage directory
sudo mkdir -p /etc/credstore.encrypted
sudo chmod 700 /etc/credstore.encrypted
```

## Creating Encrypted Credentials

```bash
# From stdin (recommended - no plaintext on disk)
echo -n "your-secret-value" | sudo systemd-creds encrypt \
    --name=credential_name - /etc/credstore.encrypted/credential_name

# From an existing file
sudo systemd-creds encrypt \
    --name=credential_name /path/to/secret.txt /etc/credstore.encrypted/credential_name

# Verify it was created
sudo systemd-creds decrypt /etc/credstore.encrypted/credential_name -
```

The `--name=` parameter must match the credential name used in the service unit.

## Service Unit Configuration

Add `LoadCredentialEncrypted` to your service unit:

```ini
# /etc/systemd/system/myapp.service
[Unit]
Description=My Application

[Service]
Type=simple
ExecStart=/usr/bin/myapp

# Load encrypted credentials
LoadCredentialEncrypted=api_key:/etc/credstore.encrypted/api_key
LoadCredentialEncrypted=db_password:/etc/credstore.encrypted/db_password

[Install]
WantedBy=multi-user.target
```

After changes:

```bash
sudo systemctl daemon-reload
sudo systemctl restart myapp
```

## Accessing Credentials in Application Code

At runtime, credentials appear as files in `$CREDENTIALS_DIRECTORY`:

```
/run/credentials/myapp.service/api_key
/run/credentials/myapp.service/db_password
```

### Python

```python
import os
from pathlib import Path

def get_credential(name: str) -> str:
    cred_dir = os.environ.get('CREDENTIALS_DIRECTORY')
    if not cred_dir:
        raise RuntimeError("CREDENTIALS_DIRECTORY not set - not running under systemd?")
    return Path(cred_dir, name).read_text()

api_key = get_credential('api_key')
```

### Node.js

```javascript
const fs = require('fs');
const path = require('path');

function getCredential(name) {
    const credDir = process.env.CREDENTIALS_DIRECTORY;
    if (!credDir) {
        throw new Error('CREDENTIALS_DIRECTORY not set');
    }
    return fs.readFileSync(path.join(credDir, name), 'utf8');
}

const apiKey = getCredential('api_key');
```

### Shell

```bash
API_KEY=$(< "$CREDENTIALS_DIRECTORY/api_key")
```

## Updating Credentials

```bash
# Re-encrypt with new value
echo -n "new-secret-value" | sudo systemd-creds encrypt \
    --name=api_key - /etc/credstore.encrypted/api_key

# Restart service to pick up new value
sudo systemctl restart myapp
```

## Troubleshooting

```bash
# Check if credentials loaded
sudo systemctl show myapp --property=LoadCredentialEncrypted

# Test decryption manually
sudo systemd-creds decrypt /etc/credstore.encrypted/api_key -

# Check service environment
sudo systemctl show myapp --property=Environment

# View service logs
journalctl -u myapp -f
```

## Security Notes

- Credentials are encrypted using host-specific keys (machine-id derived)
- Encrypted files cannot be decrypted on other machines
- Root can still access decrypted credentials at runtime via `/proc`
- For stronger protection, enable TPM binding if hardware supports it:
  ```bash
  systemd-analyze has-tpm2  # Check TPM availability
  # If available, add --with-key=tpm2 to encrypt command
  ```
