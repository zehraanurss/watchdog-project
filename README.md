# Watchdog

Watchdog is a POSIX process supervisor written in C for the System Programming term project. It starts two workers, monitors their exit status, and restarts each worker up to three times after unexpected termination. Workers that exit normally stay stopped.

## Requirements

Use Linux with GCC, GNU Make, and Bash. The project was tested on Ubuntu 20.04 under WSL. On Ubuntu, install the build tools if needed:

```bash
sudo apt update
sudo apt install build-essential
```

## Build and run

From the repository root, run:

```bash
make
./watchdog
```

The build creates two executables, `watchdog` and `worker`. Run `./watchdog` from the repository root so it can find `./worker`. The startup output shows the watchdog PID and both worker PIDs. The health check reports how many workers are active.

## Request a status report

Keep the watchdog running in the first terminal. In a second terminal, change to the same repository directory and enter the watchdog PID printed at startup:

```bash
read -rp "Watchdog PID: " WATCHDOG_PID
kill -USR1 "$WATCHDOG_PID"
grep -F '[STATUS]' watchdog.log | tail -n 3
```

If the report has not appeared yet, run the last command again. The report lists each configured worker's name, PID, state, and restart count. A PID of `-1` means that the worker has no active process.

## Shut down

Press **Ctrl+C** in the watchdog terminal, or run `kill -TERM "$WATCHDOG_PID"` from the second terminal. The watchdog sends `SIGTERM` to running workers and collects their exit statuses. It can send `SIGKILL` to workers that remain active after the shutdown timeout.

## Read the log

The watchdog appends records to `watchdog.log` in its working directory. Run `tail -f watchdog.log` to follow new entries while the program runs. Each record starts with a local timestamp and an event label.

| Label         | Recorded event                                 |
| ------------- | ---------------------------------------------- |
| `START`       | Watchdog startup                               |
| `PROCESS`     | Worker startup and assigned PID                |
| `EXIT`        | Worker termination and its exit code or signal |
| `RESTART`     | Restart attempt after an unexpected exit       |
| `LIMIT`       | Restart budget exhausted                       |
| `STATUS`      | Worker state reported in response to `SIGUSR1` |
| `SHUTDOWN`    | Shutdown progress and completion               |

## Run the tests

```bash
make test
```

The Bash test script runs in temporary working directories and prints a `PASS` line for each successful check. If a check fails, it prints the relevant console and log output.

| Scenario               | Expected result                                                           |
| ---------------------- | --- ------------ ---------------------------------------------------------|
| Normal operation       | Two workers start and belong to the test watchdog.                        |
| Normal termination     | A worker in `normal:1` mode exits with code 0 and is not restarted.       |
| Unexpected termination | A test worker terminated with `SIGKILL` is replaced by a worker with a     new PID.                                                                                             |
| Restart limit       |Three restarts are allowed. The next unexpected exit produces a `LIMIT` entry.|
| Status reporting    | `SIGUSR1` produces a report with worker PIDs and states.                     |
| Graceful shutdown   | Separate `SIGTERM` and `SIGINT` checks verify that the watchdog exits and the remaining worker is collected.                                                                       |
| Log verification    | All seven event labels appear with timestamps.                               |

To verify keyboard shutdown separately, run `./watchdog` in the foreground, wait for both workers to start, and press **Ctrl+C**. Check that the shell prompt returns and that the log contains both worker exit records and the shutdown completion message.

## Source layout

| Path                     | Responsibility |
| --- -------------------- | ---------------------------------------------------------------------- |
| `src/main.c`             | Main control loop and signal-driven dispatch                           |
| `src/signal_manager.c`   | POSIX signal handlers and event flags                                  |
| `src/process_manager.c`  | Worker lifecycle, restarts, health checks, and shutdown                |
| `src/logger.c`           | Timestamped file logging                                               |
| `include/`               | Public module interfaces                                               |
| `workers/worker.c`       | Worker executable                                                      |
| `tests/test_scenarios.sh`| Automated scenario checks                                              |

Run `make clean` to remove the executables. The log file remains available for inspection.
