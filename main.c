#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "lib/civetweb.h"
#include "db.h"
#include "categories.h"
#include "expenses.h"
#include "budgets.h"
#include "reports.h"
#include "recurring.h"
#include "audit.h"

static volatile int s_running = 1;

static void signal_handler(int sig) {
    (void)sig;
    s_running = 0;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    printf("=== Student Expense Tracker ===\n");
    printf("Initializing database...\n");

    /* Initialize database */
    if (db_init("expenses.db") != 0) {
        fprintf(stderr, "Failed to initialize database\n");
        return 1;
    }

    /* Seed default categories */
    categories_seed();

    /* Configure Civetweb */
    const char *options[] = {
        "listening_ports", "8080",
        "document_root", "./public",
        "enable_directory_listing", "no",
        "access_control_allow_origin", "*",
        "access_control_allow_methods", "GET, POST, PUT, DELETE, OPTIONS",
        "access_control_allow_headers", "Content-Type",
        NULL
    };

    struct mg_callbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));

    /* Start web server */
    struct mg_context *ctx = mg_start(&callbacks, NULL, options);
    if (!ctx) {
        fprintf(stderr, "Failed to start web server!\n");
        db_close();
        return 1;
    }

    /* Register API route handlers
     * NOTE: More specific routes MUST be registered before less specific ones.
     * Civetweb matches the longest matching URI prefix. */
    mg_set_request_handler(ctx, "/api/expenses/summary", expenses_summary_handler, NULL);
    mg_set_request_handler(ctx, "/api/expenses/", expenses_id_handler, NULL);
    mg_set_request_handler(ctx, "/api/expenses", expenses_handler, NULL);
    mg_set_request_handler(ctx, "/api/categories", categories_handler, NULL);
    mg_set_request_handler(ctx, "/api/budgets", budgets_handler, NULL);
    mg_set_request_handler(ctx, "/api/reports/export/csv", reports_csv_handler, NULL);
    mg_set_request_handler(ctx, "/api/reports/monthly", reports_monthly_handler, NULL);
    mg_set_request_handler(ctx, "/api/recurring/process", recurring_handler, NULL);
    mg_set_request_handler(ctx, "/api/upload", upload_handler, NULL);
    mg_set_request_handler(ctx, "/api/audit", audit_handler, NULL);

    printf("\n");
    printf("  Server running at: http://localhost:8080\n");
    printf("  Press Ctrl+C to stop\n");
    printf("\n");

    /* Handle graceful shutdown */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    while (s_running) {
#ifdef _WIN32
        Sleep(100);
#else
        usleep(100000);
#endif
    }

    printf("\nShutting down...\n");
    mg_stop(ctx);
    db_close();
    printf("Goodbye!\n");

    return 0;
}
