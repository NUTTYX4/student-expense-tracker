#ifndef AUDIT_H
#define AUDIT_H

#include "lib/civetweb.h"

/* Write an entry to the audit_log table */
void audit_log(const char *action, const char *details);

/* GET /api/audit — return last 20 audit log entries */
int audit_handler(struct mg_connection *conn, void *cbdata);

#endif /* AUDIT_H */
