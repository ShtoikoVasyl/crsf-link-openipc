#!/bin/sh
set -eu

ENV_FILE="${ENV_FILE:-/etc/openipc_ip_bridge.env}"
if [ -f "${ENV_FILE}" ]; then
    # shellcheck disable=SC1090
    . "${ENV_FILE}"
fi

BRIDGE_BIN="${BRIDGE_BIN:-/usr/bin/openipc_ip_bridge}"
ETH_IFACE="${ETH_IFACE:-eth0}"
GROUND_HOST="${GROUND_HOST:-192.168.1.11}"
GROUND_PORT="${GROUND_PORT:-9000}"
BRIDGE_TRANSPORT="${BRIDGE_TRANSPORT:-tcp}"
BRIDGE_MODE="${BRIDGE_MODE:-raw}"
SERIAL_PORT="${SERIAL_PORT:-/dev/ttyS2}"
SERIAL_BAUD="${SERIAL_BAUD:-420000}"
CONNECT_TIMEOUT_MS="${CONNECT_TIMEOUT_MS:-2000}"
RECONNECT_DELAY_MS="${RECONNECT_DELAY_MS:-1000}"
POLL_TIMEOUT_MS="${POLL_TIMEOUT_MS:-250}"
VERBOSE="${VERBOSE:-1}"

# Optional static addressing for direct Ethernet cable setups.
# GROUND_HOST is the IP of the computer used to configure/control the camera.
CAMERA_IP_CIDR="${CAMERA_IP_CIDR:-}"

if [ -n "${CAMERA_IP_CIDR}" ]; then
    ip addr replace "${CAMERA_IP_CIDR}" dev "${ETH_IFACE}"
    ip link set "${ETH_IFACE}" up
fi

child_pid=""

terminate() {
    if [ -n "${child_pid}" ]; then
        kill "${child_pid}" 2>/dev/null || true
        wait "${child_pid}" 2>/dev/null || true
        child_pid=""
    fi
    exit 0
}

trap terminate INT TERM

while :; do
    if [ "${VERBOSE}" = "1" ]; then
        "${BRIDGE_BIN}" \
            --host "${GROUND_HOST}" \
            --port "${GROUND_PORT}" \
            --transport "${BRIDGE_TRANSPORT}" \
            --mode "${BRIDGE_MODE}" \
            --serial "${SERIAL_PORT}" \
            --baud "${SERIAL_BAUD}" \
            --connect-timeout-ms "${CONNECT_TIMEOUT_MS}" \
            --reconnect-delay-ms "${RECONNECT_DELAY_MS}" \
            --poll-timeout-ms "${POLL_TIMEOUT_MS}" \
            --verbose &
    else
        "${BRIDGE_BIN}" \
            --host "${GROUND_HOST}" \
            --port "${GROUND_PORT}" \
            --transport "${BRIDGE_TRANSPORT}" \
            --mode "${BRIDGE_MODE}" \
            --serial "${SERIAL_PORT}" \
            --baud "${SERIAL_BAUD}" \
            --connect-timeout-ms "${CONNECT_TIMEOUT_MS}" \
            --reconnect-delay-ms "${RECONNECT_DELAY_MS}" \
            --poll-timeout-ms "${POLL_TIMEOUT_MS}" &
    fi

    child_pid=$!
    wait "${child_pid}" || true
    child_pid=""
    sleep 1
done
