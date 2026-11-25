#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d)
watchdog_pid=

cleanup() {
    local result=$?
    if [[ -n "$watchdog_pid" ]]; then
        kill -TERM "$watchdog_pid" 2>/dev/null || true
        wait "$watchdog_pid" || true
    fi
    if (( result != 0 )); then
        for output in "$test_dir"/*/watchdog.log "$test_dir"/*/console.log; do
            [[ -f "$output" ]] || continue
            printf '\n--- %s ---\n' "$output" >&2
            cat "$output" >&2
        done
    fi
    rm -rf -- "$test_dir"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

fail() {
    printf 'FAIL: %s\n' "$*" >&2
    exit 1
}

wait_until() {
    local attempt
    for ((attempt = 0; attempt < 160; attempt++)); do
        if "$@"; then
            return 0
        fi
        sleep 0.05
    done
    fail "Timed out: $*"
}

log_contains() {
    grep -Eq -- "$1" watchdog.log 2>/dev/null
}

worker_pid() {
    sed -nE "s/.*\[PROCESS\] '$1' started \(PID: ([0-9]+),.*/\1/p" watchdog.log |
        tail -n 1
}

is_test_child() {
    local pid=$1
    [[ "$pid" =~ ^[1-9][0-9]*$ ]] &&
        [[ "$(ps -o ppid= -p "$pid" | tr -d '[:space:]')" == "$watchdog_pid" ]]
}

replacement_started() {
    local pid
    pid=$(worker_pid worker-1)
    [[ "$pid" != "$1" ]] && is_test_child "$pid"
}

process_gone() {
    ! kill -0 "$1" 2>/dev/null
}

start_watchdog() {
    "$repo_dir/watchdog" >console.log 2>&1 &
    watchdog_pid=$!
    wait_until log_contains "\\[START\\].*PID: $watchdog_pid\\)"
    wait_until log_contains "\\[PROCESS\\] 'worker-1'"
    wait_until log_contains "\\[PROCESS\\] 'worker-2'"
}

stop_watchdog() {
    local signal=$1
    kill -s "$signal" "$watchdog_pid"
    wait_until process_gone "$watchdog_pid"
    wait "$watchdog_pid"
    watchdog_pid=
}

make -C "$repo_dir" all
mkdir "$test_dir/loop" "$test_dir/normal"
cd "$test_dir/loop"
ln -s "$repo_dir/worker" worker
start_watchdog
first_pid=$(worker_pid worker-1)
second_pid=$(worker_pid worker-2)
is_test_child "$first_pid" && is_test_child "$second_pid" ||
    fail "Both logged PIDs must belong to the test watchdog"
printf 'PASS: normal operation\n'

kill -USR1 "$watchdog_pid"
wait_until log_contains "\\[STATUS\\].*name=worker-1 pid=$first_pid state=RUNNING"
wait_until log_contains "\\[STATUS\\].*name=worker-2 pid=$second_pid state=RUNNING"
printf 'PASS: SIGUSR1 status report\n'

for attempt in 1 2 3; do
    old_pid=$(worker_pid worker-1)
    is_test_child "$old_pid" || fail "Refusing to signal unverified test PID: $old_pid"
    kill -KILL "$old_pid"
    wait_until log_contains "\\[EXIT\\].*PID: $old_pid\\).*signal: 9"
    wait_until log_contains "\\[RESTART\\] 'worker-1'.*\\($attempt/3\\)"
    wait_until replacement_started "$old_pid"
    new_pid=$(worker_pid worker-1)
done
printf 'PASS: unexpected termination and restart\n'

is_test_child "$new_pid" || fail "Refusing to signal unverified test PID: $new_pid"
kill -KILL "$new_pid"
wait_until log_contains "\\[LIMIT\\] 'worker-1'"
kill -USR1 "$watchdog_pid"
wait_until log_contains "\\[STATUS\\].*name=worker-1 pid=-1 state=FAILED restart_count=3"
[[ $(grep -cE "\\[PROCESS\\] 'worker-1'" watchdog.log) == 4 ]] ||
    fail "Expected one initial start and three restarts"
printf 'PASS: restart limit\n'

stop_watchdog TERM
process_gone "$second_pid" || fail "worker-2 survived SIGTERM shutdown"
wait_until log_contains '\[SHUTDOWN\].*Shutdown complete'
printf 'PASS: graceful shutdown via SIGTERM\n'

cd "$test_dir/normal"
export WATCHDOG_TEST_WORKER="$repo_dir/worker"
cat > worker <<'EOF'
#!/bin/sh
if [ "$1" = worker-1 ]; then
    exec "$WATCHDOG_TEST_WORKER" "$1" normal:1
fi
exec "$WATCHDOG_TEST_WORKER" "$@"
EOF
chmod +x worker
start_watchdog
second_pid=$(worker_pid worker-2)
wait_until log_contains "\\[EXIT\\] 'worker-1'.*exit code: 0"
kill -USR1 "$watchdog_pid"
wait_until log_contains '\[STATUS\].*name=worker-1 pid=-1 state=EXITED_NORMAL restart_count=0'
printf 'PASS: normal termination without restart\n'

# SIGINT is the signal Ctrl+C sends to a foreground process.
stop_watchdog INT
process_gone "$second_pid" || fail "worker-2 survived SIGINT shutdown"
wait_until log_contains '\[SHUTDOWN\].*Shutdown complete'
printf 'PASS: graceful shutdown via SIGINT\n'

for event in START PROCESS EXIT RESTART LIMIT STATUS SHUTDOWN; do
    grep -Eq "^\[[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}\] \[$event\] ." \
        "$test_dir/loop/watchdog.log" || fail "Missing timestamped $event record"
done
printf 'PASS: timestamped log records\n'
