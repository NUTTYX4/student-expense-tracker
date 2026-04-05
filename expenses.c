#include "expenses.h"
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

/* Helper: extract ID from URI like /api/expenses/123 */
static int extract_id(const char *uri) {
    const char *p = strrchr(uri, '/');
    if (p && *(p + 1)) return atoi(p + 1);
    return -1;
}

/* GET /api/expenses?month=YYYY-MM */
static int handle_get_expenses(struct mg_connection *conn) {
    sqlite3 *db = db_get();
    const struct mg_request_info *ri = mg_get_request_info(conn);

    /* Check for month filter in query string */
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

    const char *sql;
    sqlite3_stmt *stmt;

    if (month[0]) {
        sql = "SELECT e.id, e.amount, e.category_id, e.note, e.date, e.is_recurring, e.receipt_path, "
              "c.name, c.color, c.icon FROM expenses e "
              "LEFT JOIN categories c ON e.category_id = c.id "
              "WHERE strftime('%Y-%m', e.date) = ? "
              "ORDER BY e.date DESC, e.id DESC";
        sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
        sqlite3_bind_text(stmt, 1, month, -1, SQLITE_TRANSIENT);
    } else {
        sql = "SELECT e.id, e.amount, e.category_id, e.note, e.date, e.is_recurring, e.receipt_path, "
              "c.name, c.color, c.icon FROM expenses e "
              "LEFT JOIN categories c ON e.category_id = c.id "
              "ORDER BY e.date DESC, e.id DESC";
        sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    }

    cJSON *arr = cJSON_CreateArray();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(obj, "id", sqlite3_column_int(stmt, 0));
        cJSON_AddNumberToObject(obj, "amount", sqlite3_column_double(stmt, 1));
        cJSON_AddNumberToObject(obj, "category_id", sqlite3_column_int(stmt, 2));
        const char *note = (const char *)sqlite3_column_text(stmt, 3);
        cJSON_AddStringToObject(obj, "note", note ? note : "");
        cJSON_AddStringToObject(obj, "date", (const char *)sqlite3_column_text(stmt, 4));
        cJSON_AddNumberToObject(obj, "is_recurring", sqlite3_column_int(stmt, 5));
        const char *receipt = (const char *)sqlite3_column_text(stmt, 6);
        cJSON_AddStringToObject(obj, "receipt_path", receipt ? receipt : "");
        const char *cat_name = (const char *)sqlite3_column_text(stmt, 7);
        cJSON_AddStringToObject(obj, "category_name", cat_name ? cat_name : "");
        const char *cat_color = (const char *)sqlite3_column_text(stmt, 8);
        cJSON_AddStringToObject(obj, "category_color", cat_color ? cat_color : "");
        const char *cat_icon = (const char *)sqlite3_column_text(stmt, 9);
        cJSON_AddStringToObject(obj, "category_icon", cat_icon ? cat_icon : "");
        cJSON_AddItemToArray(arr, obj);
    }
    sqlite3_finalize(stmt);

    char *json = cJSON_PrintUnformatted(arr);
    mg_printf(conn, "HTTP/1.1 200 OK\r\n"
                    "Content-Type: application/json\r\n"
                    "Content-Length: %d\r\n\r\n%s",
              (int)strlen(json), json);
    free(json);
    cJSON_Delete(arr);
    return 200;
}

/* POST /api/expenses — create expense */
static int handle_post_expense(struct mg_connection *conn) {
    char body[4096];
    read_body(conn, body, sizeof(body));

    cJSON *json = cJSON_Parse(body);
    if (!json) {
        mg_printf(conn, "HTTP/1.1 400 Bad Request\r\n"
                        "Content-Type: application/json\r\n\r\n"
                        "{\"error\":\"Invalid JSON\"}");
        return 400;
    }

    double amount = 0;
    int category_id = 1, is_recurring = 0;
    const char *note = "", *date = "", *frequency = "";

    cJSON *j;
    if ((j = cJSON_GetObjectItem(json, "amount"))) amount = j->valuedouble;
    if ((j = cJSON_GetObjectItem(json, "category_id"))) category_id = j->valueint;
    if ((j = cJSON_GetObjectItem(json, "note"))) note = j->valuestring ? j->valuestring : "";
    if ((j = cJSON_GetObjectItem(json, "date"))) date = j->valuestring ? j->valuestring : "";
    if ((j = cJSON_GetObjectItem(json, "is_recurring"))) is_recurring = j->valueint;
    if ((j = cJSON_GetObjectItem(json, "frequency"))) frequency = j->valuestring ? j->valuestring : "";

    sqlite3 *db = db_get();
    const char *sql = "INSERT INTO expenses (amount, category_id, note, date, is_recurring) VALUES (?, ?, ?, ?, ?)";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        cJSON_Delete(json);
        mg_printf(conn, "HTTP/1.1 500 Internal Server Error\r\n"
                        "Content-Type: application/json\r\n\r\n"
                        "{\"error\":\"Database error\"}");
        return 500;
    }

    sqlite3_bind_double(stmt, 1, amount);
    sqlite3_bind_int(stmt, 2, category_id);
    sqlite3_bind_text(stmt, 3, note, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, date, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, is_recurring);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    int expense_id = (int)sqlite3_last_insert_rowid(db);

    /* If recurring, insert into recurring table */
    if (is_recurring && frequency[0]) {
        const char *rec_sql = "INSERT INTO recurring (expense_id, frequency, next_due_date) VALUES (?, ?, ?)";
        sqlite3_stmt *rec_stmt;

        /* Calculate next due date based on frequency */
        char next_date[16];
        strncpy(next_date, date, 15);
        next_date[15] = '\0';

        /* Simple date advancement: parse YYYY-MM-DD */
        int y, m, d;
        if (sscanf(date, "%d-%d-%d", &y, &m, &d) == 3) {
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
            snprintf(next_date, sizeof(next_date), "%04d-%02d-%02d", y, m, d);
        }

        if (sqlite3_prepare_v2(db, rec_sql, -1, &rec_stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(rec_stmt, 1, expense_id);
            sqlite3_bind_text(rec_stmt, 2, frequency, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(rec_stmt, 3, next_date, -1, SQLITE_TRANSIENT);
            sqlite3_step(rec_stmt);
            sqlite3_finalize(rec_stmt);
        }
    }

    /* Audit log */
    char details[256];
    snprintf(details, sizeof(details), "Added expense #%d: %.2f", expense_id, amount);
    audit_log("ADD_EXPENSE", details);

    cJSON_Delete(json);

    char response[256];
    snprintf(response, sizeof(response), "{\"id\":%d,\"message\":\"Expense created\"}", expense_id);
    mg_printf(conn, "HTTP/1.1 201 Created\r\n"
                    "Content-Type: application/json\r\n"
                    "Content-Length: %d\r\n\r\n%s",
              (int)strlen(response), response);
    return 201;
}

/* GET/POST /api/expenses */
int expenses_handler(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    const struct mg_request_info *ri = mg_get_request_info(conn);

    if (strcmp(ri->request_method, "GET") == 0) {
        return handle_get_expenses(conn);
    } else if (strcmp(ri->request_method, "POST") == 0) {
        return handle_post_expense(conn);
    }

    mg_printf(conn, "HTTP/1.1 405 Method Not Allowed\r\n"
                    "Content-Type: application/json\r\n\r\n"
                    "{\"error\":\"Method not allowed\"}");
    return 405;
}

/* PUT /api/expenses/:id */
static int handle_put_expense(struct mg_connection *conn, int id) {
    char body[4096];
    read_body(conn, body, sizeof(body));

    cJSON *json = cJSON_Parse(body);
    if (!json) {
        mg_printf(conn, "HTTP/1.1 400 Bad Request\r\n"
                        "Content-Type: application/json\r\n\r\n"
                        "{\"error\":\"Invalid JSON\"}");
        return 400;
    }

    sqlite3 *db = db_get();
    const char *sql = "UPDATE expenses SET amount=?, category_id=?, note=?, date=?, is_recurring=? WHERE id=?";
    sqlite3_stmt *stmt;

    double amount = 0;
    int category_id = 1, is_recurring = 0;
    const char *note = "", *date = "";

    cJSON *j;
    if ((j = cJSON_GetObjectItem(json, "amount"))) amount = j->valuedouble;
    if ((j = cJSON_GetObjectItem(json, "category_id"))) category_id = j->valueint;
    if ((j = cJSON_GetObjectItem(json, "note"))) note = j->valuestring ? j->valuestring : "";
    if ((j = cJSON_GetObjectItem(json, "date"))) date = j->valuestring ? j->valuestring : "";
    if ((j = cJSON_GetObjectItem(json, "is_recurring"))) is_recurring = j->valueint;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_double(stmt, 1, amount);
        sqlite3_bind_int(stmt, 2, category_id);
        sqlite3_bind_text(stmt, 3, note, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, date, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 5, is_recurring);
        sqlite3_bind_int(stmt, 6, id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    char details[256];
    snprintf(details, sizeof(details), "Updated expense #%d", id);
    audit_log("EDIT_EXPENSE", details);

    cJSON_Delete(json);

    char response[128];
    snprintf(response, sizeof(response), "{\"id\":%d,\"message\":\"Expense updated\"}", id);
    mg_printf(conn, "HTTP/1.1 200 OK\r\n"
                    "Content-Type: application/json\r\n"
                    "Content-Length: %d\r\n\r\n%s",
              (int)strlen(response), response);
    return 200;
}

/* DELETE /api/expenses/:id */
static int handle_delete_expense(struct mg_connection *conn, int id) {
    sqlite3 *db = db_get();

    /* Delete associated recurring entry first */
    const char *del_rec = "DELETE FROM recurring WHERE expense_id=?";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, del_rec, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    /* Delete expense */
    const char *del_exp = "DELETE FROM expenses WHERE id=?";
    if (sqlite3_prepare_v2(db, del_exp, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    char details[256];
    snprintf(details, sizeof(details), "Deleted expense #%d", id);
    audit_log("DELETE_EXPENSE", details);

    char response[128];
    snprintf(response, sizeof(response), "{\"id\":%d,\"message\":\"Expense deleted\"}", id);
    mg_printf(conn, "HTTP/1.1 200 OK\r\n"
                    "Content-Type: application/json\r\n"
                    "Content-Length: %d\r\n\r\n%s",
              (int)strlen(response), response);
    return 200;
}

/* PUT/DELETE /api/expenses/:id */
int expenses_id_handler(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    const struct mg_request_info *ri = mg_get_request_info(conn);
    int id = extract_id(ri->local_uri);

    if (id <= 0) {
        mg_printf(conn, "HTTP/1.1 400 Bad Request\r\n"
                        "Content-Type: application/json\r\n\r\n"
                        "{\"error\":\"Invalid expense ID\"}");
        return 400;
    }

    if (strcmp(ri->request_method, "PUT") == 0) {
        return handle_put_expense(conn, id);
    } else if (strcmp(ri->request_method, "DELETE") == 0) {
        return handle_delete_expense(conn, id);
    } else if (strcmp(ri->request_method, "GET") == 0) {
        /* GET single expense by ID */
        sqlite3 *db = db_get();
        const char *sql = "SELECT e.id, e.amount, e.category_id, e.note, e.date, e.is_recurring, e.receipt_path, "
                          "c.name, c.color, c.icon FROM expenses e "
                          "LEFT JOIN categories c ON e.category_id = c.id WHERE e.id = ?";
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, id);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                cJSON *obj = cJSON_CreateObject();
                cJSON_AddNumberToObject(obj, "id", sqlite3_column_int(stmt, 0));
                cJSON_AddNumberToObject(obj, "amount", sqlite3_column_double(stmt, 1));
                cJSON_AddNumberToObject(obj, "category_id", sqlite3_column_int(stmt, 2));
                const char *note = (const char *)sqlite3_column_text(stmt, 3);
                cJSON_AddStringToObject(obj, "note", note ? note : "");
                cJSON_AddStringToObject(obj, "date", (const char *)sqlite3_column_text(stmt, 4));
                cJSON_AddNumberToObject(obj, "is_recurring", sqlite3_column_int(stmt, 5));
                const char *receipt = (const char *)sqlite3_column_text(stmt, 6);
                cJSON_AddStringToObject(obj, "receipt_path", receipt ? receipt : "");
                const char *cn = (const char *)sqlite3_column_text(stmt, 7);
                cJSON_AddStringToObject(obj, "category_name", cn ? cn : "");
                const char *cc = (const char *)sqlite3_column_text(stmt, 8);
                cJSON_AddStringToObject(obj, "category_color", cc ? cc : "");
                const char *ci = (const char *)sqlite3_column_text(stmt, 9);
                cJSON_AddStringToObject(obj, "category_icon", ci ? ci : "");

                char *json_str = cJSON_PrintUnformatted(obj);
                mg_printf(conn, "HTTP/1.1 200 OK\r\n"
                                "Content-Type: application/json\r\n"
                                "Content-Length: %d\r\n\r\n%s",
                          (int)strlen(json_str), json_str);
                free(json_str);
                cJSON_Delete(obj);
                sqlite3_finalize(stmt);
                return 200;
            }
            sqlite3_finalize(stmt);
        }
        mg_printf(conn, "HTTP/1.1 404 Not Found\r\n"
                        "Content-Type: application/json\r\n\r\n"
                        "{\"error\":\"Expense not found\"}");
        return 404;
    }

    mg_printf(conn, "HTTP/1.1 405 Method Not Allowed\r\n"
                    "Content-Type: application/json\r\n\r\n"
                    "{\"error\":\"Method not allowed\"}");
    return 405;
}

/* GET /api/expenses/summary — monthly totals per category */
int expenses_summary_handler(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    const struct mg_request_info *ri = mg_get_request_info(conn);

    if (strcmp(ri->request_method, "GET") != 0) {
        mg_printf(conn, "HTTP/1.1 405 Method Not Allowed\r\n"
                        "Content-Type: application/json\r\n\r\n"
                        "{\"error\":\"Method not allowed\"}");
        return 405;
    }

    /* Default to current month */
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
    const char *sql =
        "SELECT c.id, c.name, c.color, c.icon, COALESCE(SUM(e.amount), 0) as total "
        "FROM categories c "
        "LEFT JOIN expenses e ON e.category_id = c.id AND strftime('%Y-%m', e.date) = ? "
        "GROUP BY c.id ORDER BY total DESC";
    sqlite3_stmt *stmt;

    cJSON *result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "month", month);

    double grand_total = 0;
    cJSON *categories = cJSON_CreateArray();

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

    cJSON_AddNumberToObject(result, "total", grand_total);
    cJSON_AddItemToObject(result, "categories", categories);

    char *json = cJSON_PrintUnformatted(result);
    mg_printf(conn, "HTTP/1.1 200 OK\r\n"
                    "Content-Type: application/json\r\n"
                    "Content-Length: %d\r\n\r\n%s",
              (int)strlen(json), json);
    free(json);
    cJSON_Delete(result);
    return 200;
}

/* POST /api/upload — receipt image upload */
struct upload_state {
    char filepath[512];
    int success;
};

static int field_found_cb(const char *key, const char *filename,
                          char *path, size_t pathlen, void *user_data) {
    struct upload_state *state = (struct upload_state *)user_data;
    (void)key;
    if (filename && filename[0]) {
        snprintf(path, pathlen, "uploads/%s", filename);
        snprintf(state->filepath, sizeof(state->filepath), "uploads/%s", filename);
        return 0x8; /* MG_FORM_FIELD_STORAGE_STORE */
    }
    return 0x2; /* MG_FORM_FIELD_STORAGE_GET */
}

static int field_get_cb(const char *key, const char *value, size_t valuelen, void *user_data) {
    (void)key; (void)value; (void)valuelen; (void)user_data;
    return 0; /* MG_FORM_FIELD_HANDLE_NEXT */
}

static int field_stored_cb(const char *path, long long file_size, void *user_data) {
    struct upload_state *state = (struct upload_state *)user_data;
    (void)file_size;
    state->success = 1;
    printf("Receipt stored: %s (%lld bytes)\n", path, file_size);
    return 0;
}

int upload_handler(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    const struct mg_request_info *ri = mg_get_request_info(conn);

    if (strcmp(ri->request_method, "POST") != 0) {
        mg_printf(conn, "HTTP/1.1 405 Method Not Allowed\r\n"
                        "Content-Type: application/json\r\n\r\n"
                        "{\"error\":\"Method not allowed\"}");
        return 405;
    }

    struct upload_state state;
    memset(&state, 0, sizeof(state));

    struct mg_form_data_handler fdh;
    memset(&fdh, 0, sizeof(fdh));
    fdh.field_found = field_found_cb;
    fdh.field_get = field_get_cb;
    fdh.field_stored = field_stored_cb;
    fdh.user_data = &state;

    mg_handle_form_request(conn, &fdh);

    if (state.success) {
        char response[512];
        snprintf(response, sizeof(response), "{\"path\":\"%s\"}", state.filepath);
        mg_printf(conn, "HTTP/1.1 200 OK\r\n"
                        "Content-Type: application/json\r\n"
                        "Content-Length: %d\r\n\r\n%s",
                  (int)strlen(response), response);
        return 200;
    }

    mg_printf(conn, "HTTP/1.1 400 Bad Request\r\n"
                    "Content-Type: application/json\r\n\r\n"
                    "{\"error\":\"No file uploaded\"}");
    return 400;
}
