#ifndef CATEGORIES_H
#define CATEGORIES_H

#include "lib/civetweb.h"

/* Seed default categories if the table is empty */
void categories_seed(void);

/* GET /api/categories handler */
int categories_handler(struct mg_connection *conn, void *cbdata);

#endif /* CATEGORIES_H */
