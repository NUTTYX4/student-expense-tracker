#include "categories.h"
#include "db.h"
#include "lib/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void categories_seed(void) {
    sqlite3 *db = db_get();
    if (!db) return;

    /* Check if categories already exist */
    sqlite3_stmt *stmt;
    const char *check = "SELECT COUNT(*) FROM categories";
    int count = 0;
    if (sqlite3_prepare_v2(db, check, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    if (count > 0) return; /* Already seeded */

    const char *names[]  = {"Food",    "Transport", "Books",   "Hostel",  "Health",  "Entertainment"};
    const char *colors[] = {"#EF4444", "#F59E0B",   "#3B82F6", "#8B5CF6", "#10B981", "#EC4899"};
    const char *icons[]  = {"\xF0\x9F\x8D\x94", "\xF0\x9F\x9A\x8C", "\xF0\x9F\x93\x9A",
                            "\xF0\x9F\x8F\xA0", "\xF0\x9F\x92\x8A", "\xF0\x9F\x8E\xAE"};

    const char *sql = "INSERT INTO categories (name, color, icon) VALUES (?, ?, ?)";
    for (int i = 0; i < 6; i++) {
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, names[i], -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, colors[i], -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, icons[i], -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }
    printf("Seeded 6 default categories\n");
}

int categories_handler(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    const struct mg_request_info *ri = mg_get_request_info(conn);

    if (strcmp(ri->request_method, "GET") != 0) {
        mg_printf(conn, "HTTP/1.1 405 Method Not Allowed\r\n"
                        "Content-Type: application/json\r\n\r\n"
                        "{\"error\":\"Method not allowed\"}");
        return 405;
    }

    sqlite3 *db = db_get();
    const char *sql = "SELECT id, name, color, icon FROM categories ORDER BY id";
    sqlite3_stmt *stmt;

    cJSON *arr = cJSON_CreateArray();
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            cJSON *obj = cJSON_CreateObject();
            cJSON_AddNumberToObject(obj, "id", sqlite3_column_int(stmt, 0));
            cJSON_AddStringToObject(obj, "name", (const char *)sqlite3_column_text(stmt, 1));
            cJSON_AddStringToObject(obj, "color", (const char *)sqlite3_column_text(stmt, 2));
            cJSON_AddStringToObject(obj, "icon", (const char *)sqlite3_column_text(stmt, 3));
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
