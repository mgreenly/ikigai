/**
 * @file bg_startup.h
 * @brief Startup recovery scan for background processes
 *
 * On ikigai startup, any background processes with status 'starting' or
 * 'running' in the database are orphans from a previous session that crashed.
 * This module detects them and updates their status to reflect their real state.
 */

#ifndef IK_BG_STARTUP_H
#define IK_BG_STARTUP_H

#include "apps/ikigai/db/connection.h"

/**
 * Recover orphaned background processes on startup.
 *
 * Queries background_processes for rows with status 'starting' or 'running'.
 * For each row:
 *   - If pid is alive (kill(pid, 0) == 0): SIGKILL the process group and
 *     mark the row KILLED in the database.
 *   - If pid is dead or invalid: mark the row EXITED with exit_code = -1
 *     (unknown exit code).
 *
 * No-op when db is NULL.
 *
 * @param db  Database context. May be NULL (skips scan).
 */
void bg_startup_recover(ik_db_ctx_t *db);

#endif /* IK_BG_STARTUP_H */
