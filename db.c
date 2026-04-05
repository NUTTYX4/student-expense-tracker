#include "db.h"
#include <stdio.h>
#include <string.h>

static sqlite3 *g_db = NULL;

static const char *SCHEMA_SQL =
    "CREATE TABLE IF NOT EXISTS categories ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  name TEXT NOT NULL,"
    "  color TEXT NOT NULL,"
    "  icon TEXT NOT NULL"
    ");"
    "CREATE TABLE IF NOT EXISTS expenses ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  amount REAL NOT NULL,"
    "  category_id INTEGER,"
    "  note TEXT,"
    "  date TEXT NOT NULL,"
    "  is_recurring INTEGER DEFAULT 0,"
    "  receipt_path TEXT,"
    "  FOREIGN KEY(category_id) REFERENCES categories(id)"
    ");"
    "CREATE TABLE IF NOT EXISTS budgets ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  category_id INTEGER UNIQUE,"
    "  monthly_limit REAL,"
    "  period TEXT,"
    "  FOREIGN KEY(category_id) REFERENCES categories(id)"
    ");"
    "CREATE TABLE IF NOT EXISTS recurring ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  expense_id INTEGER,"
    "  frequency TEXT,"
    "  next_due_date TEXT,"
    "  FOREIGN KEY(expense_id) REFERENCES expenses(id)"
    ");"
    "CREATE TABLE IF NOT EXISTS audit_log ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  action TEXT,"
    "  timestamp TEXT,"
    "  details TEXT"
    ");";

int db_init(const char *db_path) {
    int rc = sqlite3_open(db_path, &g_db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(g_db));
        return -1;
    }

    /* Enable WAL mode for better concurrency */
    sqlite3_exec(g_db, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
    sqlite3_exec(g_db, "PRAGMA foreign_keys=ON;", NULL, NULL, NULL);

    char *err_msg = NULL;
    rc = sqlite3_exec(g_db, SCHEMA_SQL, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Schema error: %s\n", err_msg);
        sqlite3_free(err_msg);
        return -1;
    }

    printf("Database initialized at %s\n", db_path);
    return 0;
}

sqlite3 *db_get(void) {
    return g_db;
}

void db_close(void) {
    if (g_db) {
        sqlite3_close(g_db);
        g_db = NULL;
    }
}
