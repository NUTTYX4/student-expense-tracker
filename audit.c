#include "audit.h"
#include "db.h"
#include "lib/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void audit_log(const char *action, const char *details) {
    sqlite3 *db = db_get();
    if (!db) return;

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", t);

    const char *sql = "INSERT INTO audit_log (action, timestamp, details) VALUES (?, ?, ?)";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, action, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, timestamp, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, details, -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

int audit_handler(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    const struct mg_request_info *ri = mg_get_request_info(conn);

    if (strcmp(ri->request_method, "GET") != 0) {
        mg_printf(conn, "HTTP/1.1 405 Method Not Allowed\r\n"
                        "Content-Type: application/json\r\n\r\n"
                        "{\"error\":\"Method not allowed\"}");
        return 405;
    }

    sqlite3 *db = db_get();
    const char *sql = "SELECT id, action, timestamp, details FROM audit_log ORDER BY id DESC LIMIT 20";
    sqlite3_stmt *stmt;

    cJSON *arr = cJSON_CreateArray();
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            cJSON *obj = cJSON_CreateObject();
            cJSON_AddNumberToObject(obj, "id", sqlite3_column_int(stmt, 0));
            cJSON_AddStringToObject(obj, "action", (const char *)sqlite3_column_text(stmt, 1));
            cJSON_AddStringToObject(obj, "timestamp", (const char *)sqlite3_column_text(stmt, 2));
            const char *details = (const char *)sqlite3_column_text(stmt, 3);
            cJSON_AddStringToObject(obj, "details", details ? details : "");
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
