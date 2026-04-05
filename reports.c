#include "reports.h"
#include "db.h"
#include "lib/cJSON.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/* GET /api/reports/monthly?month=YYYY-MM */
int reports_monthly_handler(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    const struct mg_request_info *ri = mg_get_request_info(conn);

    if (strcmp(ri->request_method, "GET") != 0) {
        mg_printf(conn, "HTTP/1.1 405 Method Not Allowed\r\n"
                        "Content-Type: application/json\r\n\r\n"
                        "{\"error\":\"Method not allowed\"}");
        return 405;
    }

    /* Get month parameter */
    char month[16] = "";
    if (ri->query_string) {
        const char *m = strstr(ri->query_string, "month=");
        if (m) {
            m += 6;
            int i = 0;
            while (*m && *m != '&' && i < 15) month[i++] = *m++;
            month[i] = '\0';
        }
    }
    if (!month[0]) {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        snprintf(month, sizeof(month), "%04d-%02d", t->tm_year + 1900, t->tm_mon + 1);
    }

    sqlite3 *db = db_get();
    cJSON *result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "month", month);

    /* Daily spending for the month */
    cJSON *daily = cJSON_CreateArray();
    {
        const char *sql = "SELECT strftime('%d', date) as day, SUM(amount) "
                          "FROM expenses WHERE strftime('%Y-%m', date) = ? "
                          "GROUP BY day ORDER BY day";
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, month, -1, SQLITE_TRANSIENT);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                cJSON *d = cJSON_CreateObject();
                cJSON_AddStringToObject(d, "day", (const char *)sqlite3_column_text(stmt, 0));
                cJSON_AddNumberToObject(d, "total", sqlite3_column_double(stmt, 1));
                cJSON_AddItemToArray(daily, d);
            }
            sqlite3_finalize(stmt);
        }
    }
    cJSON_AddItemToObject(result, "daily", daily);

    /* Per-category breakdown */
    cJSON *categories = cJSON_CreateArray();
    double grand_total = 0;
    {
        const char *sql = "SELECT c.id, c.name, c.color, c.icon, COALESCE(SUM(e.amount), 0) "
                          "FROM categories c "
                          "LEFT JOIN expenses e ON e.category_id = c.id AND strftime('%Y-%m', e.date) = ? "
                          "GROUP BY c.id ORDER BY SUM(e.amount) DESC";
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, month, -1, SQLITE_TRANSIENT);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                cJSON *cat = cJSON_CreateObject();
                cJSON_AddNumberToObject(cat, "id", sqlite3_column_int(stmt, 0));
                cJSON_AddStringToObject(cat, "name", (const char *)sqlite3_column_text(stmt, 1));
                cJSON_AddStringToObject(cat, "color", (const char *)sqlite3_column_text(stmt, 2));
                cJSON_AddStringToObject(cat, "icon", (const char *)sqlite3_column_text(stmt, 3));
                double total = sqlite3_column_double(stmt, 4);
                cJSON_AddNumberToObject(cat, "total", total);
                grand_total += total;
                cJSON_AddItemToArray(categories, cat);
            }
            sqlite3_finalize(stmt);
        }
    }
    cJSON_AddItemToObject(result, "categories", categories);
    cJSON_AddNumberToObject(result, "total", grand_total);

    char *json = cJSON_PrintUnformatted(result);
    mg_printf(conn, "HTTP/1.1 200 OK\r\n"
                    "Content-Type: application/json\r\n"
                    "Content-Length: %d\r\n\r\n%s",
              (int)strlen(json), json);
    free(json);
    cJSON_Delete(result);
    return 200;
}

/* GET /api/reports/export/csv */
int reports_csv_handler(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    const struct mg_request_info *ri = mg_get_request_info(conn);

    if (strcmp(ri->request_method, "GET") != 0) {
        mg_printf(conn, "HTTP/1.1 405 Method Not Allowed\r\n"
                        "Content-Type: application/json\r\n\r\n"
                        "{\"error\":\"Method not allowed\"}");
        return 405;
    }

    sqlite3 *db = db_get();
    const char *sql = "SELECT e.date, c.name, e.amount, e.note "
                      "FROM expenses e "
                      "LEFT JOIN categories c ON e.category_id = c.id "
                      "ORDER BY e.date DESC";
    sqlite3_stmt *stmt;

    /* Build CSV in memory */
    char csv[65536];
    int pos = 0;
    pos += snprintf(csv + pos, sizeof(csv) - pos, "Date,Category,Amount,Note\r\n");

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW && pos < (int)sizeof(csv) - 256) {
            const char *date = (const char *)sqlite3_column_text(stmt, 0);
            const char *cat = (const char *)sqlite3_column_text(stmt, 1);
            double amount = sqlite3_column_double(stmt, 2);
            const char *note = (const char *)sqlite3_column_text(stmt, 3);
            if (!date) date = "";
            if (!cat) cat = "";
            if (!note) note = "";

            /* Escape note field for CSV (wrap in quotes if contains comma) */
            pos += snprintf(csv + pos, sizeof(csv) - pos,
                            "%s,%s,%.2f,\"%s\"\r\n",
                            date, cat, amount, note);
        }
        sqlite3_finalize(stmt);
    }

    mg_printf(conn, "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/csv\r\n"
                    "Content-Disposition: attachment; filename=\"expenses.csv\"\r\n"
                    "Content-Length: %d\r\n\r\n%s",
              pos, csv);
    return 200;
}
