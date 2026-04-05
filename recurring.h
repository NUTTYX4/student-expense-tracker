#ifndef RECURRING_H
#define RECURRING_H

#include "lib/civetweb.h"

/* POST /api/recurring/process — auto-log overdue recurring expenses */
int recurring_handler(struct mg_connection *conn, void *cbdata);

#endif /* RECURRING_H */
