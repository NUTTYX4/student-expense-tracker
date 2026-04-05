#ifndef BUDGETS_H
#define BUDGETS_H

#include "lib/civetweb.h"

/* GET/POST /api/budgets */
int budgets_handler(struct mg_connection *conn, void *cbdata);

#endif /* BUDGETS_H */
