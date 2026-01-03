## Overview

This file implements PostgreSQL schema migrations, allowing the database to evolve over time by applying SQL migration files in a specific sequence. It tracks the current schema version and applies pending migrations (those with higher version numbers), ensuring migrations execute in strict order and providing error handling with detailed diagnostics.

## Code

```
function get_current_version(database_connection):
    execute query: "SELECT schema_version FROM schema_metadata LIMIT 1"

    if query fails or returns no rows:
        return 0 (database is fresh)

    extract version number from first row
    return version number

function read_file_contents(file_path):
    read entire file using shared file utility

    if read fails:
        map generic errors to migration-specific error messages
        return error (e.g., "Cannot open migration file", "File too large")

    return file contents

function parse_migration_number(filename):
    validate filename has .sql extension
    validate filename starts with digits (3 or 4)
    validate digit separator is a dash (-)

    if all validations pass:
        extract and convert digits to number
        return migration number

    return -1 (invalid format)

function scan_migrations(migrations_directory):
    open the migrations directory
    allocate array to hold migration entries (grow as needed)

    for each file in directory:
        skip . and .. entries

        parse filename to get migration number
        if filename format invalid:
            skip this file

        if array is full:
            double the array size

        build full file path
        add entry to array (number and path)

    sort all entries by migration number (ascending)
    return sorted migration list

function apply_migration(database_connection, migration):
    read migration file contents
    if read fails:
        return error

    execute SQL statement against database

    if execution fails:
        return error with migration number and PostgreSQL error message

    return success

function ik_db_migrate(database_context, migrations_directory):
    validate parameters are not null
    allocate temporary memory context

    get current schema version from database (0 if table doesn't exist)

    scan migrations directory for all .sql files
    if scan fails:
        clean up and return error

    for each migration (in sorted order):
        if migration number is greater than current version:
            apply the migration
            if application fails:
                clean up and return error

    clean up temporary memory
    return success
```
