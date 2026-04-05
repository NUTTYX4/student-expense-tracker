#ifndef DB_H
#define DB_H

#include "lib/sqlite3.h"

/* Initialize the database: open file, create tables, seed defaults */
int db_init(const char *db_path);

/* Get the global database handle */
sqlite3 *db_get(void);

/* Close the database */
void db_close(void);

#endif /* DB_H */
