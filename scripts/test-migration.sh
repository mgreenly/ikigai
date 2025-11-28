#!/bin/bash
# Automated migration test script
# Tests database migration validity, idempotency, and constraints
#
# Usage: ./scripts/test-migration.sh
# Exit codes: 0 = success, non-zero = failure

set -euo pipefail

# Color output for better readability
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Generate unique test database name using process ID
TEST_DB="ikigai_test_migration_$$"
MIGRATION_FILE="migrations/001-initial-schema.sql"

# Database connection - use postgres user for test database creation
DB_USER="postgres"

# Cleanup function - always runs on exit
cleanup() {
    local exit_code=$?
    echo -e "${YELLOW}Cleaning up test database...${NC}"
    sudo -u "$DB_USER" dropdb --if-exists "$TEST_DB" 2>/dev/null || true
    if [ $exit_code -eq 0 ]; then
        echo -e "${GREEN}✓ All tests passed, cleanup complete${NC}"
    else
        echo -e "${RED}✗ Tests failed, cleanup complete${NC}"
    fi
    exit $exit_code
}

trap cleanup EXIT INT TERM

echo "======================================"
echo "Database Migration Test Suite"
echo "======================================"
echo "Test database: $TEST_DB"
echo "Migration file: $MIGRATION_FILE"
echo ""

# Verify migration file exists
if [ ! -f "$MIGRATION_FILE" ]; then
    echo -e "${RED}✗ Migration file not found: $MIGRATION_FILE${NC}"
    exit 1
fi
echo -e "${GREEN}✓ Migration file exists${NC}"

# Create temporary test database
echo -e "${YELLOW}Creating test database...${NC}"
sudo -u "$DB_USER" createdb "$TEST_DB"
echo -e "${GREEN}✓ Test database created${NC}"

# Apply migration for the first time
echo -e "${YELLOW}Applying migration (first time)...${NC}"
if ! cat "$MIGRATION_FILE" | sudo -u "$DB_USER" psql -d "$TEST_DB" > /dev/null 2>&1; then
    echo -e "${RED}✗ Migration failed to apply${NC}"
    exit 1
fi
echo -e "${GREEN}✓ Migration applied successfully${NC}"

# Verify schema_metadata table exists
echo -e "${YELLOW}Verifying schema_metadata table...${NC}"
RESULT=$(sudo -u "$DB_USER" psql -d "$TEST_DB" -t -c "SELECT COUNT(*) FROM information_schema.tables WHERE table_name = 'schema_metadata';")
if [ "$(echo $RESULT | tr -d '[:space:]')" != "1" ]; then
    echo -e "${RED}✗ schema_metadata table not found${NC}"
    exit 1
fi
echo -e "${GREEN}✓ schema_metadata table exists${NC}"

# Verify sessions table exists
echo -e "${YELLOW}Verifying sessions table...${NC}"
RESULT=$(sudo -u "$DB_USER" psql -d "$TEST_DB" -t -c "SELECT COUNT(*) FROM information_schema.tables WHERE table_name = 'sessions';")
if [ "$(echo $RESULT | tr -d '[:space:]')" != "1" ]; then
    echo -e "${RED}✗ sessions table not found${NC}"
    exit 1
fi
echo -e "${GREEN}✓ sessions table exists${NC}"

# Verify messages table exists
echo -e "${YELLOW}Verifying messages table...${NC}"
RESULT=$(sudo -u "$DB_USER" psql -d "$TEST_DB" -t -c "SELECT COUNT(*) FROM information_schema.tables WHERE table_name = 'messages';")
if [ "$(echo $RESULT | tr -d '[:space:]')" != "1" ]; then
    echo -e "${RED}✗ messages table not found${NC}"
    exit 1
fi
echo -e "${GREEN}✓ messages table exists${NC}"

# Verify idx_messages_session index exists
echo -e "${YELLOW}Verifying idx_messages_session index...${NC}"
RESULT=$(sudo -u "$DB_USER" psql -d "$TEST_DB" -t -c "SELECT COUNT(*) FROM pg_indexes WHERE indexname = 'idx_messages_session';")
if [ "$(echo $RESULT | tr -d '[:space:]')" != "1" ]; then
    echo -e "${RED}✗ idx_messages_session index not found${NC}"
    exit 1
fi
echo -e "${GREEN}✓ idx_messages_session index exists${NC}"

# Verify idx_sessions_started index exists
echo -e "${YELLOW}Verifying idx_sessions_started index...${NC}"
RESULT=$(sudo -u "$DB_USER" psql -d "$TEST_DB" -t -c "SELECT COUNT(*) FROM pg_indexes WHERE indexname = 'idx_sessions_started';")
if [ "$(echo $RESULT | tr -d '[:space:]')" != "1" ]; then
    echo -e "${RED}✗ idx_sessions_started index not found${NC}"
    exit 1
fi
echo -e "${GREEN}✓ idx_sessions_started index exists${NC}"

# Verify idx_messages_search GIN index exists
echo -e "${YELLOW}Verifying idx_messages_search GIN index...${NC}"
RESULT=$(sudo -u "$DB_USER" psql -d "$TEST_DB" -t -c "SELECT COUNT(*) FROM pg_indexes WHERE indexname = 'idx_messages_search';")
if [ "$(echo $RESULT | tr -d '[:space:]')" != "1" ]; then
    echo -e "${RED}✗ idx_messages_search index not found${NC}"
    exit 1
fi
echo -e "${GREEN}✓ idx_messages_search GIN index exists${NC}"

# Verify schema version is 1
echo -e "${YELLOW}Verifying schema version...${NC}"
RESULT=$(sudo -u "$DB_USER" psql -d "$TEST_DB" -t -c "SELECT schema_version FROM schema_metadata;")
if [ "$(echo $RESULT | tr -d '[:space:]')" != "1" ]; then
    echo -e "${RED}✗ Schema version is not 1${NC}"
    exit 1
fi
echo -e "${GREEN}✓ Schema version is 1${NC}"

# Test idempotency - apply migration again
echo -e "${YELLOW}Testing idempotency (applying migration again)...${NC}"
if ! cat "$MIGRATION_FILE" | sudo -u "$DB_USER" psql -d "$TEST_DB" > /dev/null 2>&1; then
    echo -e "${RED}✗ Migration not idempotent (failed on second application)${NC}"
    exit 1
fi
echo -e "${GREEN}✓ Migration is idempotent${NC}"

# Verify schema version still 1 after idempotent reapply
echo -e "${YELLOW}Verifying schema version unchanged after reapply...${NC}"
RESULT=$(sudo -u "$DB_USER" psql -d "$TEST_DB" -t -c "SELECT schema_version FROM schema_metadata;")
if [ "$(echo $RESULT | tr -d '[:space:]')" != "1" ]; then
    echo -e "${RED}✗ Schema version changed after idempotent reapply${NC}"
    exit 1
fi
echo -e "${GREEN}✓ Schema version unchanged${NC}"

# Test foreign key constraint - insert session first
echo -e "${YELLOW}Testing foreign key constraint...${NC}"
SESSION_ID=$(sudo -u "$DB_USER" psql -d "$TEST_DB" -t -A -c "INSERT INTO sessions (started_at) VALUES (NOW()) RETURNING id;" | head -1)
SESSION_ID=$(echo $SESSION_ID | tr -d '[:space:]')

# Valid insert should succeed
if ! sudo -u "$DB_USER" psql -d "$TEST_DB" -c "INSERT INTO messages (session_id, kind, content) VALUES ($SESSION_ID, 'user', 'test');" > /dev/null 2>&1; then
    echo -e "${RED}✗ Valid message insert failed${NC}"
    exit 1
fi

# Invalid session_id should fail
if sudo -u "$DB_USER" psql -d "$TEST_DB" -c "INSERT INTO messages (session_id, kind, content) VALUES (99999, 'user', 'test');" > /dev/null 2>&1; then
    echo -e "${RED}✗ Foreign key constraint not enforced (invalid session_id accepted)${NC}"
    exit 1
fi
echo -e "${GREEN}✓ Foreign key constraint working${NC}"

# Verify table structures match expected schema
echo -e "${YELLOW}Verifying sessions table structure...${NC}"
COLUMNS=$(sudo -u "$DB_USER" psql -d "$TEST_DB" -t -c "SELECT column_name FROM information_schema.columns WHERE table_name = 'sessions' ORDER BY ordinal_position;")
EXPECTED_COLS=("id" "started_at" "ended_at" "title")
COL_ARRAY=($COLUMNS)
for i in "${!EXPECTED_COLS[@]}"; do
    if [ "${COL_ARRAY[$i]}" != "${EXPECTED_COLS[$i]}" ]; then
        echo -e "${RED}✗ sessions table structure mismatch${NC}"
        exit 1
    fi
done
echo -e "${GREEN}✓ sessions table structure correct${NC}"

echo -e "${YELLOW}Verifying messages table structure...${NC}"
COLUMNS=$(sudo -u "$DB_USER" psql -d "$TEST_DB" -t -c "SELECT column_name FROM information_schema.columns WHERE table_name = 'messages' ORDER BY ordinal_position;")
EXPECTED_COLS=("id" "session_id" "kind" "content" "data" "created_at")
COL_ARRAY=($COLUMNS)
for i in "${!EXPECTED_COLS[@]}"; do
    if [ "${COL_ARRAY[$i]}" != "${EXPECTED_COLS[$i]}" ]; then
        echo -e "${RED}✗ messages table structure mismatch${NC}"
        exit 1
    fi
done
echo -e "${GREEN}✓ messages table structure correct${NC}"

# Test CASCADE delete - verify that deleting session deletes messages
echo -e "${YELLOW}Testing CASCADE delete behavior...${NC}"
TEST_SESSION_ID=$(sudo -u "$DB_USER" psql -d "$TEST_DB" -t -A -c "INSERT INTO sessions (started_at) VALUES (NOW()) RETURNING id;" | head -1)
TEST_SESSION_ID=$(echo $TEST_SESSION_ID | tr -d '[:space:]')
sudo -u "$DB_USER" psql -d "$TEST_DB" -c "INSERT INTO messages (session_id, kind, content) VALUES ($TEST_SESSION_ID, 'user', 'cascade test');" > /dev/null 2>&1

# Count messages before delete
MSG_COUNT_BEFORE=$(sudo -u "$DB_USER" psql -d "$TEST_DB" -t -c "SELECT COUNT(*) FROM messages WHERE session_id = $TEST_SESSION_ID;")
MSG_COUNT_BEFORE=$(echo $MSG_COUNT_BEFORE | tr -d '[:space:]')

if [ "$MSG_COUNT_BEFORE" != "1" ]; then
    echo -e "${RED}✗ Failed to insert test message for cascade test${NC}"
    exit 1
fi

# Delete session
sudo -u "$DB_USER" psql -d "$TEST_DB" -c "DELETE FROM sessions WHERE id = $TEST_SESSION_ID;" > /dev/null 2>&1

# Count messages after delete
MSG_COUNT_AFTER=$(sudo -u "$DB_USER" psql -d "$TEST_DB" -t -c "SELECT COUNT(*) FROM messages WHERE session_id = $TEST_SESSION_ID;")
MSG_COUNT_AFTER=$(echo $MSG_COUNT_AFTER | tr -d '[:space:]')

if [ "$MSG_COUNT_AFTER" != "0" ]; then
    echo -e "${RED}✗ CASCADE delete not working (messages not deleted with session)${NC}"
    exit 1
fi
echo -e "${GREEN}✓ CASCADE delete working correctly${NC}"

echo ""
echo "======================================"
echo -e "${GREEN}✓ All migration tests passed!${NC}"
echo "======================================"

# cleanup trap will handle database deletion
exit 0
