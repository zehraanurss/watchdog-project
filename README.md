# Watchdog

Watchdog is a C program that starts and monitors two child processes using POSIX signals. It restarts a worker at most three times after unexpected termination. A worker that exits normally stays stopped.

## Build and run

Use a Linux environment with GCC and GNU Make. On Windows, you can use Ubuntu through WSL. From the repository root, run:

```bash
make
./watchdog
```

The build produces `watchdog` and `worker`. At startup, the watchdog prints its own PID and the worker PIDs.

From a second terminal, set `WATCHDOG_PID` to the watchdog PID and use `kill -USR1 "$WATCHDOG_PID"` to request a status report. Press **Ctrl+C** in the watchdog terminal or run `kill -TERM "$WATCHDOG_PID"` from the second terminal to shut down.

The watchdog appends timestamped start, exit, restart, status, and shutdown records to `watchdog.log` in its working directory. Run `tail -f watchdog.log` to follow the log while the watchdog runs.

## Tests

```bash
make test
```

The Bash script uses temporary working directories to test worker startup, normal exit, unexpected termination, the three-restart limit, SIGUSR1 reporting, SIGTERM and SIGINT shutdown, and timestamped log records. For the normal-exit case, the script starts one worker in `normal:1` mode and checks that it exits without being restarted. The script prints `PASS` for successful checks and shows the relevant logs if a check fails.

To test Ctrl+C manually, run `./watchdog` in the foreground and wait for both workers to start. Press Ctrl+C, then check that the shell prompt returns and the log contains the worker exit records and the shutdown completion record.

Run `make clean` to remove the compiled executables.
