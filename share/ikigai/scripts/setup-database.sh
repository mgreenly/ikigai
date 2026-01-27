#!/bin/bash
# Database setup script for ikigai
# Creates PostgreSQL database and user for development

set -e

# Use environment variables with defaults
DB_HOST="${IKIGAI_DB_HOST:-localhost}"
DB_PORT="${IKIGAI_DB_PORT:-5432}"
DB_NAME="${IKIGAI_DB_NAME:-ikigai}"
DB_USER="${IKIGAI_DB_USER:-ikigai}"
DB_PASS="${IKIGAI_DB_PASS:-ikigai}"

echo "=== Ikigai Database Setup ==="
echo
echo "Configuration:"
echo "  Host: $DB_HOST"
echo "  Port: $DB_PORT"
echo "  Database: $DB_NAME"
echo "  User: $DB_USER"
echo

# Check if PostgreSQL is installed
if ! command -v psql &> /dev/null; then
    echo "PostgreSQL not found. Installing..."
    sudo apt update
    sudo apt install -y postgresql postgresql-contrib libpq-dev

    # Start and enable PostgreSQL service
    sudo systemctl start postgresql
    sudo systemctl enable postgresql

    echo "✓ PostgreSQL installed"
    echo
else
    echo "✓ PostgreSQL already installed"
    echo
fi

# Check if PostgreSQL service is running
if ! sudo systemctl is-active --quiet postgresql; then
    echo "Starting PostgreSQL service..."
    sudo systemctl start postgresql
    echo "✓ PostgreSQL service started"
    echo
fi

# Create database and user
echo "Creating database and user..."

sudo -u postgres psql -v ON_ERROR_STOP=1 <<EOF
-- Create user if not exists
DO \$\$
BEGIN
    IF NOT EXISTS (SELECT FROM pg_user WHERE usename = '$DB_USER') THEN
        CREATE USER $DB_USER WITH PASSWORD '$DB_PASS';
    END IF;
END
\$\$;

-- Create database if not exists
SELECT 'CREATE DATABASE $DB_NAME OWNER $DB_USER'
WHERE NOT EXISTS (SELECT FROM pg_database WHERE datname = '$DB_NAME')\gexec

-- Grant privileges
GRANT ALL PRIVILEGES ON DATABASE $DB_NAME TO $DB_USER;

-- Grant schema privileges (required for PostgreSQL 15+)
\c $DB_NAME
GRANT ALL ON SCHEMA public TO $DB_USER;
EOF

echo
echo "✓ Database '$DB_NAME' created"
echo "✓ User '$DB_USER' created with password '$DB_PASS'"
echo
echo "=== Setup Complete ==="
echo
echo "Set these environment variables (or add to .envrc):"
echo
echo "  export IKIGAI_DB_HOST=$DB_HOST"
echo "  export IKIGAI_DB_PORT=$DB_PORT"
echo "  export IKIGAI_DB_NAME=$DB_NAME"
echo "  export IKIGAI_DB_USER=$DB_USER"
echo "  export IKIGAI_DB_PASS=$DB_PASS"
echo
echo "Migrations will run automatically when you start ikigai."
echo
