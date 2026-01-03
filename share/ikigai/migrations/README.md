# Database Migrations

This directory contains versioned SQL migration files for the ikigai database schema.

## Current Schema

**Version:** 1 (001-initial-schema.sql)

The initial schema implements the event stream architecture with:
- `schema_metadata` table for tracking migration versions
- `sessions` table for grouping conversation events
- `messages` table for storing individual events (clear, system, user, assistant, mark, rewind)
- Indexes for efficient session message retrieval and full-text search

## Migration Files

- `001-initial-schema.sql` - Initial database schema with sessions and messages tables

## Testing Migrations

An automated test script validates migration correctness:

```bash
./scripts/test-migration.sh
```

The test script:
- Creates a temporary test database
- Applies the migration
- Verifies all tables and indexes exist
- Validates schema structure matches expected design
- Tests idempotency (applying migration multiple times)
- Verifies foreign key constraints work correctly
- Tests CASCADE delete behavior
- Cleans up temporary database

## Future Migration Process

When schema changes are needed:

### 1. Create New Migration File

Create `share/ikigai/migrations/00N-descriptive-name.sql` where N is the next sequential number.

### 2. Migration File Structure

```sql
-- Migration: 00N-descriptive-name
-- Description: What this migration does and why

BEGIN;

-- Schema changes (ALTER TABLE, CREATE INDEX, etc.)
ALTER TABLE messages ADD COLUMN new_field TEXT;

-- Update schema_metadata version
UPDATE schema_metadata SET schema_version = N;

COMMIT;
```

### 3. Test Migration

```bash
# Apply to test database
cat share/ikigai/migrations/00N-descriptive-name.sql | psql ikigai_test

# Verify schema_metadata updated
psql ikigai_test -c "SELECT * FROM schema_metadata"

# Test idempotency if applicable
cat share/ikigai/migrations/00N-descriptive-name.sql | psql ikigai_test
```

### 4. Update Migration Runner

When automated migration runner is added (Task 4):
- Check current schema version
- Apply migrations in order from current+1 to latest
- Update application to handle new schema

### 5. Document Changes

- Add comments in migration file explaining changes
- Document any breaking changes
- Update related documentation

## Migration Numbering Convention

- `001-initial-schema.sql` - Base schema (sessions, messages tables)
- `002-add-indexes.sql` - Example: Additional performance indexes
- `003-add-search.sql` - Example: Full-text search enhancements
- Sequential numbering ensures clear ordering

## Best Practices

1. **Always use transactions** - Wrap changes in BEGIN/COMMIT
2. **Update schema_metadata** - Last step in transaction
3. **Test on copy of production data** - Before applying to production
4. **Migrations are immutable** - Once applied to production, never modify
5. **Use IF NOT EXISTS** - For idempotent table/index creation (initial schema)
6. **Document breaking changes** - In migration file comments
7. **No rollback migrations** - Migrations are forward-only; create new migration to revert if needed
8. **Separate data migrations** - Document data migrations separately from schema changes

## Schema Version Tracking

The `schema_metadata` table contains a single row with the current schema version:

```sql
SELECT schema_version FROM schema_metadata;
```

This enables the application to:
- Detect schema version at startup
- Apply pending migrations automatically (when migration runner added)
- Validate schema compatibility

## Connection String

Development database connection string:

```
postgresql://ikigai:ikigai@localhost/ikigai
```

Add to `config.json`:

```json
{
  "db_connection_string": "postgresql://ikigai:ikigai@localhost/ikigai"
}
```
