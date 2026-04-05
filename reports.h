#ifndef REPORTS_H
#define REPORTS_H

#include "lib/civetweb.h"

/* GET /api/reports/monthly */
int reports_monthly_handler(struct mg_connection *conn, void *cbdata);

/* GET /api/reports/export/csv */
int reports_csv_handler(struct mg_connection *conn, void *cbdata);

#endif /* REPORTS_H */
