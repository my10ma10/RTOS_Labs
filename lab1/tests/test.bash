#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/../build}"
SERVER="$BUILD_DIR/src/server"
CLIENT="$BUILD_DIR/src/client"
SOCKET_PATH="/tmp/uds_file"
TMP="$(mktemp -d "${TMPDIR:-/tmp}/uds-ipc-test.XXXXXX")"
SERVER_PID=""
CLIENT_PIDS=()
FIFO_FDS=()
PASSED=0
FAILED=0

cleanup() {
    for fd in "${FIFO_FDS[@]}"; do
        eval "exec ${fd}>&-" || true
    done

    for pid in "${CLIENT_PIDS[@]}"; do
        kill "$pid" 2>/dev/null || true
    done

    if [[ -n "$SERVER_PID" ]]; then
        kill "$SERVER_PID" 2>/dev/null || true
    fi

    wait 2>/dev/null || true
    rm -f "$SOCKET_PATH"
    rm -rf "$TMP"
}

trap cleanup EXIT INT TERM

pass() {
    printf 'PASS: %s\n' "$1"
    PASSED=$((PASSED + 1))
}

fail() {
    printf 'FAIL: %s\n' "$1" >&2
    FAILED=$((FAILED + 1))
}

wait_text() {
    local file="$1"
    local text="$2"
    local timeout="${3:-5}"
    local start=$SECONDS

    while (( SECONDS - start < timeout )); do
        if [[ -f "$file" ]] && grep -Fq -- "$text" "$file"; then
            return 0
        fi
        sleep 0.05
    done

    return 1
}

wait_connections() {
    local count="$1"
    local timeout="${2:-4}"
    local start=$SECONDS

    while (( SECONDS - start < timeout )); do
        local connected
        connected="$(grep -Fc 'client connected' "$TMP/server.log" 2>/dev/null || true)"

        if (( connected >= count )); then
            return 0
        fi

        sleep 0.05
    done

    return 1
}

start_server() {
    local delay="${1:-0}"
    local start

    rm -f "$SOCKET_PATH"
    "$SERVER" "$delay" >"$TMP/server.log" 2>&1 &
    SERVER_PID=$!
    start=$SECONDS

    while (( SECONDS - start < 4 )); do
        if [[ -S "$SOCKET_PATH" ]]; then
            sleep 0.1
            return 0
        fi

        if ! kill -0 "$SERVER_PID" 2>/dev/null; then
            cat "$TMP/server.log" >&2
            return 1
        fi

        sleep 0.05
    done

    cat "$TMP/server.log" >&2
    return 1
}

stop_server() {
    for pid in "${CLIENT_PIDS[@]}"; do
        kill "$pid" 2>/dev/null || true
    done
    CLIENT_PIDS=()

    for fd in "${FIFO_FDS[@]}"; do
        eval "exec ${fd}>&-" || true
    done
    FIFO_FDS=()

    if [[ -n "$SERVER_PID" ]]; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
        SERVER_PID=""
    fi

    rm -f "$SOCKET_PATH"
    sleep 0.1
}

start_client() {
    local name="$1"
    local priority="$2"
    local fifo="$TMP/$name.in"
    local output="$TMP/$name.out"

    mkfifo "$fifo"
    exec {client_fifo_fd}<>"$fifo"

    "$CLIENT" "$priority" <"$fifo" >"$output" 2>&1 &
    CLIENT_PIDS+=("$!")
    FIFO_FDS+=("$client_fifo_fd")

    printf -v "FD_$name" '%s' "$client_fifo_fd"
    printf -v "OUT_$name" '%s' "$output"
}

send_line() {
    printf '%s\n' "$2" >&"$1"
}

check_sync() {
    start_client sync 7

    if ! wait_connections 1 || ! wait_text "$OUT_sync" 'Enter message to server'; then
        fail 'клиент подключается к серверу'
        return
    fi

    send_line "$FD_sync" 'sync-test-payload'

    if wait_text "$OUT_sync" 'Confirm: server received: sync-test-payload'; then
        pass 'синхронный запрос получает ответ с отправленным текстом'
    else
        fail 'синхронный запрос получает ответ с отправленным текстом'
    fi
}

check_async() {
    start_client async_a 1
    start_client async_b 2

    if ! wait_connections 3; then
        fail 'несколько клиентов подключаются одновременно'
        return
    fi

    send_line "$FD_async_a" 'notify-one'

    if wait_text "$OUT_async_a" 'Async notifications: 1' \
        && wait_text "$OUT_async_b" 'Async notifications: 1'; then
        pass 'eventfd асинхронно уведомляет подключённых клиентов'
    else
        fail 'eventfd асинхронно уведомляет подключённых клиентов'
    fi
}

check_priority() {
    stop_server

    if ! start_server 5000; then
        fail 'сервер запускается для проверки приоритетов'
        return
    fi

    start_client starter 0
    start_client low 1
    start_client high 10

    if ! wait_connections 3; then
        fail 'клиенты подключаются для проверки приоритетов'
        return
    fi

    send_line "$FD_starter" 'priority-starter'

    if ! wait_text "$TMP/server.log" 'processing request' 2; then
        fail 'сервер начал обработку стартового запроса'
        return
    fi

    send_line "$FD_low" 'priority-low'
    send_line "$FD_high" 'priority-high'

    if ! wait_text "$OUT_high" 'Confirm: server received: priority-high' 7 \
        || ! wait_text "$OUT_low" 'Confirm: server received: priority-low' 7; then
        fail 'оба клиента получили ответы на запросы'
        return
    fi

    local high_line
    local low_line

    high_line="$(grep -nE 'processing request.*priority=10([[:space:]]|$)' "$TMP/server.log" | head -n 1 | cut -d: -f1 || true)"
    low_line="$(grep -nE 'processing request.*priority=1([[:space:]]|$)' "$TMP/server.log" | head -n 1 | cut -d: -f1 || true)"  

    if [[ -n "$high_line" && -n "$low_line" && "$high_line" -lt "$low_line" ]]; then
        pass 'сервер обрабатывает ожидающие запросы по убыванию приоритета'
    else
        fail 'сервер обрабатывает ожидающие запросы по убыванию приоритета'
    fi
}

check_timer() {
    stop_server

    if ! start_server 0; then
        fail 'сервер запускается для проверки таймера'
        return
    fi

    start_client timer 5

    if ! wait_connections 1 || ! wait_text "$OUT_timer" 'Enter message to server'; then
        fail 'клиент таймера подключается'
        return
    fi

    send_line "$FD_timer" ':timer 1'

    if wait_text "$OUT_timer" 'Timer scheduled for 1 second(s)' \
        && wait_text "$OUT_timer" 'Async notifications: 1' 5; then
        pass 'таймер отвечает синхронно и позже отправляет eventfd-уведомление'
    else
        fail 'таймер отвечает синхронно и позже отправляет eventfd-уведомление'
    fi
}

if [[ ! -x "$SERVER" || ! -x "$CLIENT" ]]; then
    printf 'Не найдены исполняемые файлы. Сначала собери проект:\n'
    printf '  cmake -S "%s" -B "%s"\n' "$ROOT" "$BUILD_DIR"
    printf '  cmake --build "%s"\n' "$BUILD_DIR"
    exit 2
fi

if ! start_server 0; then
    exit 1
fi

check_sync
check_async
check_priority
check_timer
stop_server

printf '\nРезультат: пройдено %d, ошибок %d\n' "$PASSED" "$FAILED"
(( FAILED == 0 ))