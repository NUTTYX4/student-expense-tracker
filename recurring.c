#include "recurring.h"
#include "db.h"
#include "audit.h"
#include "lib/cJSON.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/* POST /api/recurring/process — auto-log overdue recurring expenses */
int recurring_handler(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    const struct mg_request_info *ri = mg_get_request_info(conn);

    if (strcmp(ri->request_method, "POST") != 0) {
        mg_printf(conn, "HTTP/1.1 405 Method Not Allowed\r\n"
                        "Content-Type: application/json\r\n\r\n"
                        "{\"error\":\"Method not allowed\"}");
        return 405;
    }

    sqlite3 *db = db_get();

    /* Get today's date */
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char today[16];
    snprintf(today, sizeof(today), "%04d-%02d-%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);

    /* Find all overdue recurring entries */
    const char *sql = "SELECT r.id, r.expense_id, r.frequency, r.next_due_date, "
                      "e.amount, e.category_id, e.note "
                      "FROM recurring r "
                      "JOIN expenses e ON r.expense_id = e.id "
                      "WHERE r.next_due_date <= ?";
    sqlite3_stmt *stmt;
    int processed = 0;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, today, -1, SQLITE_TRANSIENT);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int rec_id = sqlite3_column_int(stmt, 0);
            /* int orig_expense_id = sqlite3_column_int(stmt, 1); */
            const char *frequency = (const char *)sqlite3_column_text(stmt, 2);
            const char *next_due = (const char *)sqlite3_column_text(stmt, 3);
            double amount = sqlite3_column_double(stmt, 4);
            int category_id = sqlite3_column_int(stmt, 5);
            const char *note = (const char *)sqlite3_column_text(stmt, 6);
            if (!note) note = "";
            if (!frequency) continue;
            if (!next_due) continue;

            /* Create new expense for the due date */
            const char *ins_sql = "INSERT INTO expenses (amount, category_id, note, date, is_recurring) "
                                  "VALUES (?, ?, ?, ?, 0)";
            sqlite3_stmt *ins_stmt;
            if (sqlite3_prepare_v2(db, ins_sql, -1, &ins_stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_double(ins_stmt, 1, amount);
                sqlite3_bind_int(ins_stmt, 2, category_id);
                sqlite3_bind_text(ins_stmt, 3, note, -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(ins_stmt, 4, next_due, -1, SQLITE_TRANSIENT);
                sqlite3_step(ins_stmt);
                sqlite3_finalize(ins_stmt);
            }

            /* Calculate next due date */
            int y, m, d;
            if (sscanf(next_due, "%d-%d-%d", &y, &m, &d) == 3) {
                if (strcmp(frequency, "daily") == 0) {
                    d += 1;
                    if (d > 28) { d = 1; m += 1; }
                    if (m > 12) { m = 1; y += 1; }
                } else if (strcmp(frequency, "weekly") == 0) {
                    d += 7;
                    if (d > 28) { d -= 28; m += 1; }
                    if (m > 12) { m = 1; y += 1; }
                } else if (strcmp(frequency, "monthly") == 0) {
                    m += 1;
                    if (m > 12) { m = 1; y += 1; }
                }

                char new_next[16];
                snprintf(new_next, sizeof(new_next), "%04d-%02d-%02d", y, m, d);

                /* Update next_due_date */
                const char *upd_sql = "UPDATE recurring SET next_due_date = ? WHERE id = ?";
                sqlite3_stmt *upd_stmt;
                if (sqlite3_prepare_v2(db, upd_sql, -1, &upd_stmt, NULL) == SQLITE_OK) {
                    sqlite3_bind_text(upd_stmt, 1, new_next, -1, SQLITE_TRANSIENT);
                    sqlite3_bind_int(upd_stmt, 2, rec_id);
                    sqlite3_step(upd_stmt);
                    sqlite3_finalize(upd_stmt);
                }
            }

            char details[256];
            snprintf(details, sizeof(details), "Auto-logged recurring expense: %.2f on %s", amount, next_due);
            audit_log("RECURRING_PROCESS", details);
            processed++;
        }
        sqlite3_finalize(stmt);
    }

    char response[128];
    snprintf(response, sizeof(response), "{\"processed\":%d}", processed);
    mg_printf(conn, "HTTP/1.1 200 OK\r\n"
                    "Content-Type: application/json\r\n"
                    "Content-Length: %d\r\n\r\n%s",
              (int)strlen(response), response);
    return 200;
}
