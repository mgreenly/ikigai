#!/bin/bash
# Database setup script for ikigai
# Creates PostgreSQL database and user for development

set -e

echo "=== Ikigai Database Setup ==="
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
    IF NOT EXISTS (SELECT FROM pg_user WHERE usename = 'ikigai') THEN
        CREATE USER ikigai WITH PASSWORD 'ikigai';
    END IF;
END
\$\$;

-- Create database if not exists
SELECT 'CREATE DATABASE ikigai OWNER ikigai'
WHERE NOT EXISTS (SELECT FROM pg_database WHERE datname = 'ikigai')\gexec

-- Grant privileges
GRANT ALL PRIVILEGES ON DATABASE ikigai TO ikigai;

-- Grant schema privileges (required for PostgreSQL 15+)
\c ikigai
GRANT ALL ON SCHEMA public TO ikigai;
EOF

echo
echo "✓ Database 'ikigai' created"
echo "✓ User 'ikigai' created with password 'ikigai'"
echo
echo "=== Setup Complete ==="
echo
echo "Add this to your config.json:"
echo
echo '  "db_connection_string": "postgresql://ikigai:ikigai@localhost/ikigai"'
echo
echo "Migrations will run automatically when you start ikigai."
echo
