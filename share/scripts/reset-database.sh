#!/bin/bash
# Database reset script for ikigai
# Drops and recreates the database (destructive!)

set -e

echo "=== Ikigai Database Reset ==="
echo

# Validate required environment variables
if [ -z "$IKIGAI_DB_NAME" ] || [ -z "$IKIGAI_DB_USER" ]; then
    echo "Error: Required environment variables not set."
    echo "Make sure .envrc is loaded (run 'direnv allow')"
    echo "  IKIGAI_DB_NAME: ${IKIGAI_DB_NAME:-<not set>}"
    echo "  IKIGAI_DB_USER: ${IKIGAI_DB_USER:-<not set>}"
    exit 1
fi

DB_NAME="$IKIGAI_DB_NAME"
DB_USER="$IKIGAI_DB_USER"

echo "Resetting database '$DB_NAME'..."

# Terminate existing connections and drop database
sudo -u postgres psql -v ON_ERROR_STOP=1 <<EOF
-- Terminate existing connections
SELECT pg_terminate_backend(pid)
FROM pg_stat_activity
WHERE datname = '$DB_NAME' AND pid <> pg_backend_pid();

-- Drop database
DROP DATABASE IF EXISTS $DB_NAME;

-- Recreate database
CREATE DATABASE $DB_NAME OWNER $DB_USER;

-- Grant privileges
GRANT ALL PRIVILEGES ON DATABASE $DB_NAME TO $DB_USER;

-- Grant schema privileges (required for PostgreSQL 15+)
\c $DB_NAME
GRANT ALL ON SCHEMA public TO $DB_USER;
EOF

echo
echo "✓ Database '$DB_NAME' reset"
echo
echo "Migrations will run automatically when you start ikigai."
echo
