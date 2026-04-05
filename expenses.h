#ifndef EXPENSES_H
#define EXPENSES_H

#include "lib/civetweb.h"

/* GET/POST /api/expenses */
int expenses_handler(struct mg_connection *conn, void *cbdata);

/* PUT/DELETE /api/expenses/:id */
int expenses_id_handler(struct mg_connection *conn, void *cbdata);

/* GET /api/expenses/summary */
int expenses_summary_handler(struct mg_connection *conn, void *cbdata);

/* POST /api/upload (receipt image) */
int upload_handler(struct mg_connection *conn, void *cbdata);

#endif /* EXPENSES_H */
