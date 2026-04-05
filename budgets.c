#include "budgets.h"
#include "db.h"
#include "audit.h"
#include "lib/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Helper: read request body */
static int read_body(struct mg_connection *conn, char *buf, int buf_size) {
    int total = 0, nread;
    while (total < buf_size - 1) {
        nread = mg_read(conn, buf + total, (size_t)(buf_size - 1 - total));
        if (nread <= 0) break;
        total += nread;
    }
    buf[total] = '\0';
    return total;
}

/* GET /api/budgets — return budgets with current spending */
static int handle_get_budgets(struct mg_connection *conn) {
    sqlite3 *db = db_get();

    /* Current month */
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char month[16];
    snprintf(month, sizeof(month), "%04d-%02d", t->tm_year + 1900, t->tm_mon + 1);

    const char *sql =
        "SELECT c.id, c.name, c.color, c.icon, "
        "COALESCE(b.monthly_limit, 0), "
        "COALESCE((SELECT SUM(e.amount) FROM expenses e WHERE e.category_id = c.id AND strftime('%Y-%m', e.date) = ?), 0) "
        "FROM categories c "
        "LEFT JOIN budgets b ON b.category_id = c.id "
        "ORDER BY c.id";

    sqlite3_stmt *stmt;
    cJSON *arr = cJSON_CreateArray();

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, month, -1, SQLITE_TRANSIENT);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            cJSON *obj = cJSON_CreateObject();
            cJSON_AddNumberToObject(obj, "category_id", sqlite3_column_int(stmt, 0));
            cJSON_AddStringToObject(obj, "name", (const char *)sqlite3_column_text(stmt, 1));
            cJSON_AddStringToObject(obj, "color", (const char *)sqlite3_column_text(stmt, 2));
            cJSON_AddStringToObject(obj, "icon", (const char *)sqlite3_column_text(stmt, 3));

            double limit = sqlite3_column_double(stmt, 4);
            double spent = sqlite3_column_double(stmt, 5);
            cJSON_AddNumberToObject(obj, "monthly_limit", limit);
            cJSON_AddNumberToObject(obj, "spent", spent);

            /* Calculate percentage and status */
            double pct = (limit > 0) ? (spent / limit) * 100.0 : 0;
            cJSON_AddNumberToObject(obj, "percentage", pct);

            if (limit > 0 && pct >= 100.0) {
                cJSON_AddStringToObject(obj, "status", "exceeded");
            } else if (limit > 0 && pct >= 80.0) {
                cJSON_AddStringToObject(obj, "status", "warning");
            } else {
                cJSON_AddStringToObject(obj, "status", "ok");
            }

            cJSON_AddItemToArray(arr, obj);
        }
        sqlite3_finalize(stmt);
    }

    char *json = cJSON_PrintUnformatted(arr);
    mg_printf(conn, "HTTP/1.1 200 OK\r\n"
                    "Content-Type: application/json\r\n"
                    "Content-Length: %d\r\n\r\n%s",
              (int)strlen(json), json);
    free(json);
    cJSON_Delete(arr);
    return 200;
}

/* POST /api/budgets — set/update budget for a category */
static int handle_post_budget(struct mg_connection *conn) {
    char body[2048];
    read_body(conn, body, sizeof(body));

    cJSON *json = cJSON_Parse(body);
    if (!json) {
        mg_printf(conn, "HTTP/1.1 400 Bad Request\r\n"
                        "Content-Type: application/json\r\n\r\n"
                        "{\"error\":\"Invalid JSON\"}");
        return 400;
    }

    int category_id = 0;
    double monthly_limit = 0;
    const char *period = "";

    cJSON *j;
    if ((j = cJSON_GetObjectItem(json, "category_id"))) category_id = j->valueint;
    if ((j = cJSON_GetObjectItem(json, "monthly_limit"))) monthly_limit = j->valuedouble;
    if ((j = cJSON_GetObjectItem(json, "period"))) period = j->valuestring ? j->valuestring : "";

    if (!period[0]) {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        static char auto_period[16];
        snprintf(auto_period, sizeof(auto_period), "%04d-%02d", t->tm_year + 1900, t->tm_mon + 1);
        period = auto_period;
    }

    sqlite3 *db = db_get();

    /* Upsert: INSERT OR REPLACE */
    const char *sql =
        "INSERT INTO budgets (category_id, monthly_limit, period) VALUES (?, ?, ?) "
        "ON CONFLICT(category_id) DO UPDATE SET monthly_limit=excluded.monthly_limit, period=excluded.period";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, category_id);
        sqlite3_bind_double(stmt, 2, monthly_limit);
        sqlite3_bind_text(stmt, 3, period, -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    char details[256];
    snprintf(details, sizeof(details), "Set budget for category #%d: %.2f", category_id, monthly_limit);
    audit_log("SET_BUDGET", details);

    cJSON_Delete(json);

    const char *response = "{\"message\":\"Budget updated\"}";
    mg_printf(conn, "HTTP/1.1 200 OK\r\n"
                    "Content-Type: application/json\r\n"
                    "Content-Length: %d\r\n\r\n%s",
              (int)strlen(response), response);
    return 200;
}

int budgets_handler(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    const struct mg_request_info *ri = mg_get_request_info(conn);

    if (strcmp(ri->request_method, "GET") == 0) {
        return handle_get_budgets(conn);
    } else if (strcmp(ri->request_method, "POST") == 0) {
        return handle_post_budget(conn);
    }

    mg_printf(conn, "HTTP/1.1 405 Method Not Allowed\r\n"
                    "Content-Type: application/json\r\n\r\n"
                    "{\"error\":\"Method not allowed\"}");
    return 405;
}
