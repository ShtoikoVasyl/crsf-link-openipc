#pragma once

#include "crsf/crsf_parser.hpp"
#include "device/serial_port.hpp"
#include "net/tcp_client.hpp"
#include "net/udp_client.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace core {

enum class TransportMode {
    Tcp,
    Udp
};

enum class ForwardMode {
    Raw,
    CrsfFrames
};

struct AppConfig {
    std::string host;
    std::uint16_t port {9000};
    std::string serial_device {"/dev/ttyS2"};
    int baud_rate {420000};
    TransportMode transport {TransportMode::Tcp};
    ForwardMode mode {ForwardMode::Raw};
    int connect_timeout_ms {2000};
    int reconnect_delay_ms {1000};
    int poll_timeout_ms {250};
    bool verbose {false};
};

class BridgeApp {
public:
    class NetworkPeer {
    public:
        virtual ~NetworkPeer() = default;
        virtual bool connectTo(const std::string& host, std::uint16_t port, int timeout_ms) = 0;
        virtual void close() = 0;
        virtual bool isConnected() const noexcept = 0;
        virtual int fd() const noexcept = 0;
        virtual std::size_t readSome(std::vector<std::uint8_t>& buffer, std::size_t max_bytes) = 0;
        virtual bool sendAll(const std::vector<std::uint8_t>& data) = 0;
    };

    explicit BridgeApp(AppConfig config);

    int run();
    void requestStop();

private:
    bool ensureSerialConnected();
    bool ensureNetworkConnected();
    int loopOnce();
    bool handleSerialReadable();
    bool handleSocketReadable();
    std::unique_ptr<NetworkPeer> createNetworkPeer() const;
    void closeNetwork();
    void closeSerial();
    void log(const std::string& message) const;
    const char* transportName() const noexcept;
    const char* modeName() const noexcept;

    AppConfig config_;
    std::atomic<bool> stop_requested_ {false};
    device::SerialPort serial_port_;
    std::unique_ptr<NetworkPeer> network_peer_;
    crsf::Parser serial_parser_;
    crsf::Parser socket_parser_;
    std::vector<std::uint8_t> io_buffer_;
};

}  // namespace core
