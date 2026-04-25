# crsf-link-openipc

`crsf-link-openipc` is the OpenIPC-side project in the CRSF link pair. Its current binary name remains `openipc_ip_bridge`, and it bridges data between:

- UART on the camera side, default `/dev/ttyS2`
- Ethernet network on the control/configuration computer side

Supported modes:

- `tcp + raw`: transparent byte stream bridge
- `tcp + crsf`: forwards only valid CRSF frames
- `udp + raw`: forwards raw UART chunks as UDP datagrams
- `udp + crsf`: forwards one valid CRSF frame per UDP datagram

Default lab addresses used in the examples:

- camera: `192.168.1.10`
- control/configuration computer: `192.168.1.11`

Note:

- `GROUND_HOST` means the IP of the computer used to configure/control the camera
- it is not meant as a separate ground-unit IP label

## Do we need CRSF parsing on the camera?

Not always. For a pure socket-to-UART bridge, parsing on the camera is not required.

That is why the default mode is now:

- `--transport tcp`
- `--mode raw`

`--mode crsf` is optional and useful only when you want:

- CRSF CRC filtering before forwarding
- frame boundaries preserved over UDP
- protection against garbage bytes on one side of the link

## Recommended setup

For direct camera-to-ground Ethernet, the simplest setup is:

- `crsf-link-ground` runs on the control/configuration computer
- camera runs `openipc_ip_bridge` and connects to the control/configuration computer IP

Recommended default:

- `tdi_console_reader`: `transport: tcp`
- `tdi_console_reader`: `tcp_length_prefix: false`
- `openipc_ip_bridge`: `--transport tcp --mode raw`

## Build on your workstation

```bash
cd /Users/vasylshtoiko/Desktop/Fpv/fpv-ip-opt/ip_bridge
cmake -S . -B build
cmake --build build -j
```

Binary after build:

```bash
./build/openipc_ip_bridge
```

For your OpenIPC camera (`armv7l Linux`), do not deploy the macOS build from `build/`.
You need an ARMv7 Linux build, for example:

```bash
cd /Users/vasylshtoiko/Desktop/Fpv/fpv-ip-opt/ip_bridge
./scripts/build_openipc_armv7.sh
```

That produces:

```bash
./build-armv7/openipc_ip_bridge
```

## Main options

```bash
./build/openipc_ip_bridge \
  --host 192.168.1.11 \
  --port 9000 \
  --transport tcp \
  --mode raw \
  --serial /dev/ttyS2 \
  --baud 420000 \
  --verbose
```

Parameters:

- `--host` control/configuration computer IPv4 address
- `--port` TCP or UDP port, default `9000`
- `--transport tcp|udp`
- `--mode raw|crsf`
- `--serial` UART device, default `/dev/ttyS2`
- `--baud` UART speed, default `420000`

Environment variables:

- `GROUND_HOST`
- `GROUND_PORT`
- `BRIDGE_TRANSPORT`
- `BRIDGE_MODE`
- `SERIAL_PORT`
- `SERIAL_BAUD`

## Control computer setup

For TCP raw mode with `tdi_console_reader`, use:

```yaml
transport: tcp
listen_address: 0.0.0.0
listen_port: 9000
tcp_length_prefix: false
serial_baud: 420000
raw_mode: true
```

For UDP mode with `tdi_console_reader`, use:

```yaml
transport: udp
listen_address: 0.0.0.0
listen_port: 9000
serial_baud: 420000
raw_mode: true
```

If you intentionally use `--mode crsf` on the camera, then `raw_mode: false` on the control/configuration computer side is the cleaner pairing.

## Installation on OpenIPC through SSH

Copy the binary:

```bash
scp ./build-armv7/openipc_ip_bridge root@192.168.1.10:/tmp/openipc_ip_bridge
```

Connect by SSH:

```bash
ssh root@192.168.1.10
```

Install on the camera:

```sh
mount -o remount,rw /
cp /tmp/openipc_ip_bridge /usr/bin/openipc_ip_bridge
chmod +x /usr/bin/openipc_ip_bridge
```

Quick manual run:

```sh
/usr/bin/openipc_ip_bridge \
  --host 192.168.1.11 \
  --port 9000 \
  --transport tcp \
  --mode raw \
  --serial /dev/ttyS2 \
  --baud 420000 \
  --verbose
```

## Installation through UART console

If network on the camera is not available, connect by USB-UART adapter to the OpenIPC console.

Typical serial parameters:

- baud `115200`
- `8N1`

Open console from Linux/macOS host:

```bash
screen /dev/ttyUSB0 115200
```

Or:

```bash
picocom -b 115200 /dev/ttyUSB0
```

After login on the camera, you can upload the binary over network later, or paste commands for manual setup.

If the camera already has network but SSH is unavailable, you can still use UART console to run:

```sh
mount -o remount,rw /
/usr/bin/openipc_ip_bridge --host 192.168.1.11 --transport tcp --mode raw --serial /dev/ttyS2 --baud 420000 --verbose
```

## Autostart on OpenIPC

The helper script is:

[`scripts/openipc_direct_eth_bridge.sh`](scripts/openipc_direct_eth_bridge.sh)

Automated host-side deploy script:

- [`scripts/deploy_openipc_from_host.sh`](scripts/deploy_openipc_from_host.sh)

Step-by-step guide for the deploy script:

- [`docs/DEPLOY_SCRIPT_STEP_BY_STEP.md`](docs/DEPLOY_SCRIPT_STEP_BY_STEP.md)

Ready-made init files are also included:

- `deploy/openipc/openipc_ip_bridge.init`
- `deploy/openipc/S95openipc_ip_bridge`
- `deploy/openipc/openipc_ip_bridge.env.example`

Example:

```sh
export CAMERA_IP_CIDR=192.168.1.10/24
export GROUND_HOST=192.168.1.11
export GROUND_PORT=9000
export BRIDGE_TRANSPORT=tcp
export BRIDGE_MODE=raw
export SERIAL_PORT=/dev/ttyS2
export SERIAL_BAUD=420000
sh /root/openipc_direct_eth_bridge.sh
```

To install the script:

```bash
scp ./scripts/openipc_direct_eth_bridge.sh root@192.168.1.10:/root/openipc_direct_eth_bridge.sh
ssh root@192.168.1.10 'chmod +x /root/openipc_direct_eth_bridge.sh'
```

You can then call it from your OpenIPC startup mechanism, for example from `rc.local` or another local init hook used on your image.

Example install as init service on the camera:

```bash
scp ./build-armv7/openipc_ip_bridge root@192.168.1.10:/tmp/openipc_ip_bridge
scp ./scripts/openipc_direct_eth_bridge.sh root@192.168.1.10:/tmp/openipc_direct_eth_bridge.sh
scp ./deploy/openipc/openipc_ip_bridge.init root@192.168.1.10:/tmp/openipc_ip_bridge.init
scp ./deploy/openipc/S95openipc_ip_bridge root@192.168.1.10:/tmp/S95openipc_ip_bridge
scp ./deploy/openipc/openipc_ip_bridge.env.example root@192.168.1.10:/tmp/openipc_ip_bridge.env
```

Then on the camera:

```sh
mount -o remount,rw /
cp /tmp/openipc_ip_bridge /usr/bin/openipc_ip_bridge
cp /tmp/openipc_direct_eth_bridge.sh /usr/bin/openipc_direct_eth_bridge.sh
cp /tmp/openipc_ip_bridge.init /etc/init.d/openipc_ip_bridge
cp /tmp/S95openipc_ip_bridge /etc/rc.d/S95openipc_ip_bridge
cp /tmp/openipc_ip_bridge.env /etc/openipc_ip_bridge.env
chmod +x /usr/bin/openipc_ip_bridge /usr/bin/openipc_direct_eth_bridge.sh
chmod +x /etc/init.d/openipc_ip_bridge /etc/rc.d/S95openipc_ip_bridge
```

Edit `/etc/openipc_ip_bridge.env` with your IPs and mode, then start:

```sh
/etc/init.d/openipc_ip_bridge start
```

Check status and logs:

```sh
/etc/init.d/openipc_ip_bridge status
tail -f /tmp/openipc_ip_bridge.out /tmp/openipc_ip_bridge.err
```

## One-command deploy from your workstation

From this project directory:

```bash
chmod +x ./scripts/deploy_openipc_from_host.sh
chmod +x ./scripts/build_openipc_armv7.sh
./scripts/build_openipc_armv7.sh
./scripts/deploy_openipc_from_host.sh \
  --target-host 192.168.1.10 \
  --ground-host 192.168.1.11 \
  --transport tcp \
  --mode raw \
  --serial /dev/ttyS2 \
  --baud 420000 \
  --build-dir ./build-armv7 \
  --target-arch armv7l \
  --camera-ip-cidr 192.168.1.10/24
```

This script:

- deploys the ARMv7 Linux build of `openipc_ip_bridge`
- uploads all required files over `ssh/scp`
- installs the binary and init files on OpenIPC
- writes `/etc/openipc_ip_bridge.env`
- starts the service

Detailed walkthrough:

- [`docs/DEPLOY_SCRIPT_STEP_BY_STEP.md`](docs/DEPLOY_SCRIPT_STEP_BY_STEP.md)

## Direct Ethernet example

Control/configuration computer:

- IP: `192.168.1.11/24`
- `tdi_console_reader` listening on port `9000`

Camera:

- IP: `192.168.1.10/24`
- bridge connects to `192.168.1.11:9000`

## Notes

- `tcp + raw` is the best default for a transparent socket-to-UART bridge
- `udp + raw` is the lightest mode, but UDP can drop datagrams
- `udp + crsf` is the most sensible UDP mode if you want packet boundaries
- `420000` baud is configured on Linux/OpenIPC using `termios2/BOTHER` when needed
