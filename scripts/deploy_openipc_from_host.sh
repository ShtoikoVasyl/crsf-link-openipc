#!/bin/sh
set -eu

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
PROJECT_ROOT="$(CDPATH= cd -- "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${PROJECT_ROOT}/build-armv7}"

TARGET_HOST="${TARGET_HOST:-192.168.1.10}"
TARGET_USER="${TARGET_USER:-root}"
TARGET_PORT="${TARGET_PORT:-22}"
TARGET_PREFIX="${TARGET_USER}@${TARGET_HOST}"
TARGET_TMP_DIR="${TARGET_TMP_DIR:-/tmp/openipc_ip_bridge_deploy}"

GROUND_HOST="${GROUND_HOST:-192.168.1.11}"
GROUND_PORT="${GROUND_PORT:-9000}"
BRIDGE_TRANSPORT="${BRIDGE_TRANSPORT:-tcp}"
BRIDGE_MODE="${BRIDGE_MODE:-raw}"
SERIAL_PORT="${SERIAL_PORT:-/dev/ttyS2}"
SERIAL_BAUD="${SERIAL_BAUD:-420000}"
CONNECT_TIMEOUT_MS="${CONNECT_TIMEOUT_MS:-2000}"
RECONNECT_DELAY_MS="${RECONNECT_DELAY_MS:-1000}"
POLL_TIMEOUT_MS="${POLL_TIMEOUT_MS:-250}"
ETH_IFACE="${ETH_IFACE:-eth0}"
CAMERA_IP_CIDR="${CAMERA_IP_CIDR:-192.168.1.10/24}"
VERBOSE="${VERBOSE:-1}"
TARGET_ARCH="${TARGET_ARCH:-armv7l}"

SSH_OPTS="-p ${TARGET_PORT} -o StrictHostKeyChecking=accept-new"
SSH_CONTROL_PATH=""

cleanup() {
    if [ -n "${SSH_CONTROL_PATH}" ]; then
        ssh ${SSH_OPTS} -o ControlPath="${SSH_CONTROL_PATH}" -O exit "${TARGET_PREFIX}" >/dev/null 2>&1 || true
        rm -f "${SSH_CONTROL_PATH}" >/dev/null 2>&1 || true
    fi
}

trap cleanup EXIT INT TERM

show_usage() {
    cat <<EOF
Usage: $(basename "$0") [options]

Build locally, upload to OpenIPC, install service files, write /etc/openipc_ip_bridge.env,
and optionally start the service.
GROUND_HOST is the IP of the computer used to configure/control the camera.

Options:
  --target-host <ip>         Camera IP. Default: ${TARGET_HOST}
  --target-user <user>       SSH user. Default: ${TARGET_USER}
  --target-port <port>       SSH port. Default: ${TARGET_PORT}
  --ground-host <ip>         Control/configuration PC IP. Default: ${GROUND_HOST}
  --ground-port <port>       Control/configuration computer port. Default: ${GROUND_PORT}
  --transport <tcp|udp>      Bridge transport. Default: ${BRIDGE_TRANSPORT}
  --mode <raw|crsf>          Bridge mode. Default: ${BRIDGE_MODE}
  --serial <path>            Camera UART device. Default: ${SERIAL_PORT}
  --baud <value>             Camera UART baud. Default: ${SERIAL_BAUD}
  --target-arch <arch>       Expected camera arch. Default: ${TARGET_ARCH}
  --build-dir <path>         Build directory. Default: ${BUILD_DIR}
  --eth-iface <name>         Camera Ethernet iface. Default: ${ETH_IFACE}
  --camera-ip-cidr <cidr>    Optional static camera IP. Default: ${CAMERA_IP_CIDR}
  --no-camera-ip             Do not write CAMERA_IP_CIDR into env
  --skip-build               Reuse existing ${BUILD_DIR}/openipc_ip_bridge
  --no-start                 Install files but do not start service
  --help                     Show this help
EOF
}

SKIP_BUILD=0
START_SERVICE=1

while [ "$#" -gt 0 ]; do
    case "$1" in
        --target-host)
            TARGET_HOST="$2"
            TARGET_PREFIX="${TARGET_USER}@${TARGET_HOST}"
            shift 2
            ;;
        --target-user)
            TARGET_USER="$2"
            TARGET_PREFIX="${TARGET_USER}@${TARGET_HOST}"
            shift 2
            ;;
        --target-port)
            TARGET_PORT="$2"
            SSH_OPTS="-p ${TARGET_PORT} -o StrictHostKeyChecking=accept-new"
            shift 2
            ;;
        --ground-host)
            GROUND_HOST="$2"
            shift 2
            ;;
        --ground-port)
            GROUND_PORT="$2"
            shift 2
            ;;
        --transport)
            BRIDGE_TRANSPORT="$2"
            shift 2
            ;;
        --mode)
            BRIDGE_MODE="$2"
            shift 2
            ;;
        --serial)
            SERIAL_PORT="$2"
            shift 2
            ;;
        --baud)
            SERIAL_BAUD="$2"
            shift 2
            ;;
        --target-arch)
            TARGET_ARCH="$2"
            shift 2
            ;;
        --build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        --eth-iface)
            ETH_IFACE="$2"
            shift 2
            ;;
        --camera-ip-cidr)
            CAMERA_IP_CIDR="$2"
            shift 2
            ;;
        --no-camera-ip)
            CAMERA_IP_CIDR=""
            shift
            ;;
        --skip-build)
            SKIP_BUILD=1
            shift
            ;;
        --no-start)
            START_SERVICE=0
            shift
            ;;
        --help|-h)
            show_usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            exit 1
            ;;
    esac
done

TARGET_PREFIX="${TARGET_USER}@${TARGET_HOST}"
BIN_PATH="${BUILD_DIR}/openipc_ip_bridge"
LOCAL_ENV_FILE="${BUILD_DIR}/openipc_ip_bridge.env"

build_binary() {
    if [ "${SKIP_BUILD}" = "1" ]; then
        if [ ! -x "${BIN_PATH}" ]; then
            echo "Binary not found: ${BIN_PATH}" >&2
            exit 1
        fi
        return
    fi

    cmake -S "${PROJECT_ROOT}" -B "${BUILD_DIR}"
    cmake --build "${BUILD_DIR}" -j
}

verify_local_binary() {
    if [ ! -f "${BIN_PATH}" ]; then
        echo "Binary not found: ${BIN_PATH}" >&2
        exit 1
    fi

    if command -v file >/dev/null 2>&1; then
        file_output="$(file "${BIN_PATH}")"
        echo "Local binary: ${file_output}"

        case "${TARGET_ARCH}" in
            armv7l|armv7|arm)
                echo "${file_output}" | grep -qi "ELF" || {
                    echo "Refusing to deploy: binary is not ELF/Linux." >&2
                    exit 1
                }
                echo "${file_output}" | grep -Eqi "ARM|arm" || {
                    echo "Refusing to deploy: binary is not an ARM build." >&2
                    exit 1
                }
                ;;
        esac
    fi
}

setup_ssh_master() {
    SSH_CONTROL_PATH="/tmp/ssh-openipc-${TARGET_HOST}-${TARGET_PORT}.sock"
    rm -f "${SSH_CONTROL_PATH}" >/dev/null 2>&1 || true

    echo "Opening SSH session to ${TARGET_PREFIX}:${TARGET_PORT}"
    echo "If password authentication is enabled, enter the camera root password."
    echo "Password input is hidden in the terminal. This is normal."

    ssh ${SSH_OPTS} \
        -o ControlMaster=yes \
        -o ControlPersist=300 \
        -o ControlPath="${SSH_CONTROL_PATH}" \
        "${TARGET_PREFIX}" "exit"

    SSH_OPTS="${SSH_OPTS} -o ControlMaster=auto -o ControlPath=${SSH_CONTROL_PATH}"
}

write_env_file() {
    mkdir -p "${BUILD_DIR}"
    cat > "${LOCAL_ENV_FILE}" <<EOF
GROUND_HOST=${GROUND_HOST}
GROUND_PORT=${GROUND_PORT}
BRIDGE_TRANSPORT=${BRIDGE_TRANSPORT}
BRIDGE_MODE=${BRIDGE_MODE}
SERIAL_PORT=${SERIAL_PORT}
SERIAL_BAUD=${SERIAL_BAUD}
CONNECT_TIMEOUT_MS=${CONNECT_TIMEOUT_MS}
RECONNECT_DELAY_MS=${RECONNECT_DELAY_MS}
POLL_TIMEOUT_MS=${POLL_TIMEOUT_MS}
ETH_IFACE=${ETH_IFACE}
CAMERA_IP_CIDR=${CAMERA_IP_CIDR}
VERBOSE=${VERBOSE}
EOF
}

upload_files() {
    ssh ${SSH_OPTS} "${TARGET_PREFIX}" "mkdir -p '${TARGET_TMP_DIR}'"

    upload_one "${BIN_PATH}" "${TARGET_TMP_DIR}/openipc_ip_bridge"
    upload_one "${PROJECT_ROOT}/scripts/openipc_direct_eth_bridge.sh" "${TARGET_TMP_DIR}/openipc_direct_eth_bridge.sh"
    upload_one "${PROJECT_ROOT}/deploy/openipc/openipc_ip_bridge.init" "${TARGET_TMP_DIR}/openipc_ip_bridge.init"
    upload_one "${PROJECT_ROOT}/deploy/openipc/S95openipc_ip_bridge" "${TARGET_TMP_DIR}/S95openipc_ip_bridge"
    upload_one "${LOCAL_ENV_FILE}" "${TARGET_TMP_DIR}/openipc_ip_bridge.env"
}

upload_one() {
    local_path="$1"
    remote_path="$2"

    echo "Uploading $(basename "${local_path}")"
    ssh ${SSH_OPTS} "${TARGET_PREFIX}" "cat > '${remote_path}'" < "${local_path}"
}

install_remote() {
    ssh ${SSH_OPTS} "${TARGET_PREFIX}" \
        TARGET_TMP_DIR="${TARGET_TMP_DIR}" \
        START_SERVICE="${START_SERVICE}" \
        'sh -s' <<'EOF'
set -eu

mount -o remount,rw / || true
mkdir -p /usr/bin /etc/init.d /etc/rc.d

cp "${TARGET_TMP_DIR}/openipc_ip_bridge" /usr/bin/openipc_ip_bridge
cp "${TARGET_TMP_DIR}/openipc_direct_eth_bridge.sh" /usr/bin/openipc_direct_eth_bridge.sh
cp "${TARGET_TMP_DIR}/openipc_ip_bridge.init" /etc/init.d/openipc_ip_bridge
cp "${TARGET_TMP_DIR}/S95openipc_ip_bridge" /etc/rc.d/S95openipc_ip_bridge
cp "${TARGET_TMP_DIR}/openipc_ip_bridge.env" /etc/openipc_ip_bridge.env

chmod +x /usr/bin/openipc_ip_bridge
chmod +x /usr/bin/openipc_direct_eth_bridge.sh
chmod +x /etc/init.d/openipc_ip_bridge
chmod +x /etc/rc.d/S95openipc_ip_bridge

/etc/init.d/openipc_ip_bridge stop >/dev/null 2>&1 || true

if [ "${START_SERVICE}" = "1" ]; then
    /etc/init.d/openipc_ip_bridge start
    /etc/init.d/openipc_ip_bridge status || true
fi
EOF
}

print_summary() {
    cat <<EOF
Deploy completed.

Target camera:
  ssh ${TARGET_USER}@${TARGET_HOST} -p ${TARGET_PORT}

Installed files:
  /usr/bin/openipc_ip_bridge
  /usr/bin/openipc_direct_eth_bridge.sh
  /etc/init.d/openipc_ip_bridge
  /etc/rc.d/S95openipc_ip_bridge
  /etc/openipc_ip_bridge.env

Useful commands on the camera:
  /etc/init.d/openipc_ip_bridge status
  /etc/init.d/openipc_ip_bridge restart
  tail -f /tmp/openipc_ip_bridge.out /tmp/openipc_ip_bridge.err
EOF
}

build_binary
verify_local_binary
setup_ssh_master
write_env_file
upload_files
install_remote
print_summary
