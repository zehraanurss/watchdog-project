CC = gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic

WATCHDOG_SOURCES = src/main.c src/signal_manager.c src/process_manager.c src/logger.c
WATCHDOG_HEADERS = include/signal_manager.h include/process_manager.h include/logger.h

.PHONY: all test clean

all: watchdog worker

watchdog: $(WATCHDOG_SOURCES) $(WATCHDOG_HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(WATCHDOG_SOURCES) $(LDLIBS)

worker: workers/worker.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

test: all
	bash tests/test_scenarios.sh

clean:
	rm -f watchdog worker
