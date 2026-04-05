CC = gcc
CFLAGS = -I. -I./lib -DUSE_WEBSOCKET=0 -DNO_SSL -DNO_CGI -DMUST_IMPLEMENT_CLOCK_GETTIME
LDFLAGS = -lws2_32 -lpthread -lm

SOURCES = main.c db.c expenses.c budgets.c reports.c recurring.c categories.c audit.c lib/civetweb.c lib/cJSON.c lib/sqlite3.c
TARGET = expense_tracker.exe

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LDFLAGS)

clean:
	del /Q $(TARGET) 2>NUL
	del /Q expenses.db 2>NUL

.PHONY: all clean
