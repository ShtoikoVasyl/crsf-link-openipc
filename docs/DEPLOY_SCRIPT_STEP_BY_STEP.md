# Deploy Script Step By Step

This guide explains how to use:

- `scripts/deploy_openipc_from_host.sh`

The script is intended to run on your workstation, for example on your Mac, and it does all of this:

1. builds `openipc_ip_bridge`
2. uploads the binary and service files to the camera over `ssh/scp`
3. installs files into `/usr/bin`, `/etc/init.d`, and `/etc/rc.d`
4. writes `/etc/openipc_ip_bridge.env`
5. starts the service

## 1. Requirements

On your workstation you need:

- `cmake`
- C++ compiler
- `ssh`
- `scp`

The camera must:

- be reachable over the network
- allow `ssh` login as `root`

## 2. Recommended control computer setup

Start the control/configuration computer side first.

Recommended default:

```bash
cd /Users/vasylshtoiko/Desktop/Fpv/fpv-ip-opt/tdi_console_reader
./build/tdi_crsf_reader --config config/config-tcp-raw.yaml
```

This means:

- control/configuration computer IP is usually `192.168.1.11`
- protocol is `tcp`
- mode is `raw`
- port is `9000`

## 3. Build the control computer app if needed

If `tdi_console_reader` is not built yet:

```bash
cd /Users/vasylshtoiko/Desktop/Fpv/fpv-ip-opt/tdi_console_reader
cmake -S . -B build
cmake --build build -j
```

## 4. Go to the camera bridge project

```bash
cd /Users/vasylshtoiko/Desktop/Fpv/fpv-ip-opt/ip_bridge
```

## 5. Build the ARMv7 Linux binary for the camera

Your camera is:

- `armv7l`
- Linux

So you must not deploy the macOS binary built in `./build`.

Build the ARMv7 binary:

```bash
chmod +x ./scripts/build_openipc_armv7.sh
./scripts/build_openipc_armv7.sh
```

Expected output file:

```bash
./build-armv7/openipc_ip_bridge
```

If the script says no cross-compiler is installed, install one such as:

- `messense/macos-cross-toolchains/armv7-unknown-linux-gnueabihf`

On macOS with Homebrew:

```bash
brew tap messense/macos-cross-toolchains
brew install messense/macos-cross-toolchains/armv7-unknown-linux-gnueabihf
```

Then rerun the build script.

## 6. Run the automated deploy script

Typical default case:

```bash
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

What this does:

- builds `./build/openipc_ip_bridge`
- uploads the files to the camera
- installs the OpenIPC init files
- writes `/etc/openipc_ip_bridge.env`
- starts `/etc/init.d/openipc_ip_bridge`

During the first SSH connection the script may show:

```text
root@192.168.1.10's password:
```

Important:

- type the `root` password from the camera
- terminal input is hidden and no symbols appear
- this is normal, just type the password and press `Enter`
- if SSH asks `Are you sure you want to continue connecting`, answer `yes`

The deploy script now opens one SSH master session first, so normally you should enter the password only once per run.

Note for OpenIPC:

- some OpenIPC images do not have `/usr/libexec/sftp-server`
- because of that, modern `scp` from macOS can fail
- this deploy script now uploads files through plain `ssh` and does not require `scp/sftp` support on the camera

## 7. If the binary is already built

You can skip the local rebuild:

```bash
./scripts/deploy_openipc_from_host.sh \
  --target-host 192.168.1.10 \
  --ground-host 192.168.1.11 \
  --transport tcp \
  --mode raw \
  --build-dir ./build-armv7 \
  --skip-build
```

## 8. If you do not want static IP on the camera

If OpenIPC already configures Ethernet for you:

```bash
./scripts/deploy_openipc_from_host.sh \
  --target-host 192.168.1.10 \
  --ground-host 192.168.1.11 \
  --transport tcp \
  --mode raw \
  --no-camera-ip
```

That leaves `CAMERA_IP_CIDR` empty in `/etc/openipc_ip_bridge.env`.

## 9. Check the service on the camera

Connect to the camera:

```bash
ssh root@192.168.1.10
```

Check status:

```sh
/etc/init.d/openipc_ip_bridge status
```

Check logs:

```sh
tail -f /tmp/openipc_ip_bridge.out /tmp/openipc_ip_bridge.err
```

Restart:

```sh
/etc/init.d/openipc_ip_bridge restart
```

Stop:

```sh
/etc/init.d/openipc_ip_bridge stop
```

## 10. Verify installed config

On the camera:

```sh
cat /etc/openipc_ip_bridge.env
```

Typical expected content:

```sh
GROUND_HOST=192.168.1.11
GROUND_PORT=9000
BRIDGE_TRANSPORT=tcp
BRIDGE_MODE=raw
SERIAL_PORT=/dev/ttyS2
SERIAL_BAUD=420000
ETH_IFACE=eth0
CAMERA_IP_CIDR=192.168.1.10/24
```

## 11. UDP examples

For UDP raw:

```bash
./scripts/deploy_openipc_from_host.sh \
  --target-host 192.168.1.10 \
  --ground-host 192.168.1.11 \
  --transport udp \
  --mode raw
```

For UDP CRSF:

```bash
./scripts/deploy_openipc_from_host.sh \
  --target-host 192.168.1.10 \
  --ground-host 192.168.1.11 \
  --transport udp \
  --mode crsf
```

On the control/configuration computer, pair them with:

- `config/config-udp-raw.yaml`
- `config/config-udp-crsf.yaml`

## 12. If SSH uses a different port

Example:

```bash
./scripts/deploy_openipc_from_host.sh \
  --target-host 192.168.1.10 \
  --target-port 2222 \
  --ground-host 192.168.1.11
```

## 13. If you only want to install, but not start

```bash
./scripts/deploy_openipc_from_host.sh \
  --target-host 192.168.1.10 \
  --ground-host 192.168.1.11 \
  --no-start
```

Then start manually later:

```bash
ssh root@192.168.1.10 '/etc/init.d/openipc_ip_bridge start'
```

## 14. Troubleshooting

If `ssh` login fails:

- check camera IP
- check Ethernet link
- try UART console login
- test manual login with `ssh root@192.168.1.10`

If you see:

```text
sh: /usr/libexec/sftp-server: not found
scp: Connection closed
```

that is an OpenIPC server limitation. The deploy script has been updated to avoid `scp`, so just rerun it with the latest version.

If you see that `/usr/bin/openipc_ip_bridge` starts with bytes like:

```text
cf fa ed fe
```

then you uploaded a macOS `Mach-O` binary by mistake. Rebuild for ARMv7 Linux with:

```bash
./scripts/build_openipc_armv7.sh
```

If the service starts but there is no control link:

- verify `/dev/ttyS2` is the right UART
- verify control/configuration computer IP
- verify the control/configuration computer app is already listening
- for the default path, use `tcp + raw` on both sides

If needed, inspect:

```sh
tail -f /tmp/openipc_ip_bridge.out /tmp/openipc_ip_bridge.err
cat /etc/openipc_ip_bridge.env
```
