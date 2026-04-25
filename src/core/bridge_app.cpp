#include "core/bridge_app.hpp"

#include <chrono>
#include <iostream>
#include <poll.h>
#include <string>
#include <thread>
#include <utility>

namespace core {
namespace {

class TcpNetworkPeer final : public BridgeApp::NetworkPeer {
public:
    bool connectTo(const std::string& host, std::uint16_t port, int timeout_ms) override {
        return client_.connectTo(host, port, timeout_ms);
    }

    void close() override {
        client_.close();
    }

    bool isConnected() const noexcept override {
        return client_.isConnected();
    }

    int fd() const noexcept override {
        return client_.fd();
    }

    std::size_t readSome(std::vector<std::uint8_t>& buffer, std::size_t max_bytes) override {
        return client_.readSome(buffer, max_bytes);
    }

    bool sendAll(const std::vector<std::uint8_t>& data) override {
        return client_.sendAll(data);
    }

private:
    net::TcpClient client_;
};

class UdpNetworkPeer final : public BridgeApp::NetworkPeer {
public:
    bool connectTo(const std::string& host, std::uint16_t port, int timeout_ms) override {
        return client_.connectTo(host, port, timeout_ms);
    }

    void close() override {
        client_.close();
    }

    bool isConnected() const noexcept override {
        return client_.isConnected();
    }

    int fd() const noexcept override {
        return client_.fd();
    }

    std::size_t readSome(std::vector<std::uint8_t>& buffer, std::size_t max_bytes) override {
        return client_.readSome(buffer, max_bytes);
    }

    bool sendAll(const std::vector<std::uint8_t>& data) override {
        return client_.sendAll(data);
    }

private:
    net::UdpClient client_;
};

}  // namespace

BridgeApp::BridgeApp(AppConfig config)
    : config_(std::move(config)),
      network_peer_(createNetworkPeer()) {}

int BridgeApp::run() {
    while (!stop_requested_.load()) {
        if (!ensureSerialConnected() || !ensureNetworkConnected()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(config_.reconnect_delay_ms));
            continue;
        }

        const int loop_result = loopOnce();
        if (loop_result < 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(config_.reconnect_delay_ms));
        }
    }

    closeNetwork();
    closeSerial();
    return 0;
}

void BridgeApp::requestStop() {
    stop_requested_.store(true);
}

bool BridgeApp::ensureSerialConnected() {
    if (serial_port_.isOpen()) {
        return true;
    }

    try {
        serial_port_.open(config_.serial_device, config_.baud_rate);
        log("Serial connected: " + config_.serial_device + " @" + std::to_string(serial_port_.actualBaudRate()));
        return true;
    } catch (const std::exception& ex) {
        log("Serial connect failed: " + std::string(ex.what()));
        closeSerial();
        return false;
    }
}

bool BridgeApp::ensureNetworkConnected() {
    if (network_peer_ && network_peer_->isConnected()) {
        return true;
    }

    if (network_peer_ && network_peer_->connectTo(config_.host, config_.port, config_.connect_timeout_ms)) {
        log(std::string(transportName()) + " connected: " + config_.host + ":" + std::to_string(config_.port));
        return true;
    }

    log(std::string(transportName()) + " connect failed: " + config_.host + ":" + std::to_string(config_.port));
    closeNetwork();
    return false;
}

int BridgeApp::loopOnce() {
    pollfd descriptors[2] {};
    descriptors[0].fd = serial_port_.fd();
    descriptors[0].events = POLLIN;
    descriptors[1].fd = network_peer_->fd();
    descriptors[1].events = POLLIN;

    const int poll_result = ::poll(descriptors, 2, config_.poll_timeout_ms);
    if (poll_result < 0) {
        log("poll failed");
        closeNetwork();
        closeSerial();
        return -1;
    }

    if ((descriptors[0].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
        log("Serial disconnected");
        closeSerial();
        return -1;
    }

    if ((descriptors[1].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
        log(std::string(transportName()) + " disconnected");
        closeNetwork();
        return -1;
    }

    if ((descriptors[0].revents & POLLIN) != 0 && !handleSerialReadable()) {
        return -1;
    }

    if ((descriptors[1].revents & POLLIN) != 0 && !handleSocketReadable()) {
        return -1;
    }

    return 0;
}

bool BridgeApp::handleSerialReadable() {
    const std::size_t bytes_read = serial_port_.readSome(io_buffer_, 512);

    if (config_.mode == ForwardMode::Raw) {
        if (bytes_read == 0) {
            return true;
        }
        if (!network_peer_->sendAll(std::vector<std::uint8_t>(io_buffer_.begin(), io_buffer_.begin() + static_cast<std::ptrdiff_t>(bytes_read)))) {
            log(std::string("Failed to send raw bytes to ") + transportName() + " peer");
            closeNetwork();
            return false;
        }
        return true;
    }

    for (std::size_t i = 0; i < bytes_read; ++i) {
        auto frame = serial_parser_.pushByte(io_buffer_[i]);
        if (!frame.has_value() || !frame->crc_ok) {
            continue;
        }
        if (!network_peer_->sendAll(frame->bytes)) {
            log(std::string("Failed to send CRSF frame to ") + transportName() + " peer");
            closeNetwork();
            return false;
        }
    }
    return true;
}

bool BridgeApp::handleSocketReadable() {
    const std::size_t bytes_read = network_peer_->readSome(io_buffer_, 512);
    if (bytes_read == 0) {
        log(std::string(transportName()) + " peer closed the connection");
        closeNetwork();
        return false;
    }

    if (config_.mode == ForwardMode::Raw) {
        if (!serial_port_.writeAll(std::vector<std::uint8_t>(io_buffer_.begin(), io_buffer_.begin() + static_cast<std::ptrdiff_t>(bytes_read)))) {
            log("Failed to write raw bytes to UART");
            closeSerial();
            return false;
        }
        return true;
    }

    for (std::size_t i = 0; i < bytes_read; ++i) {
        auto frame = socket_parser_.pushByte(io_buffer_[i]);
        if (!frame.has_value() || !frame->crc_ok) {
            continue;
        }
        if (!serial_port_.writeAll(frame->bytes)) {
            log("Failed to write CRSF frame to UART");
            closeSerial();
            return false;
        }
    }
    return true;
}

std::unique_ptr<BridgeApp::NetworkPeer> BridgeApp::createNetworkPeer() const {
    if (config_.transport == TransportMode::Udp) {
        return std::make_unique<UdpNetworkPeer>();
    }
    return std::make_unique<TcpNetworkPeer>();
}

void BridgeApp::closeNetwork() {
    if (network_peer_) {
        network_peer_->close();
    }
    socket_parser_.reset();
}

void BridgeApp::closeSerial() {
    serial_port_.close();
    serial_parser_.reset();
}

void BridgeApp::log(const std::string& message) const {
    if (config_.verbose) {
        std::cerr << "[openipc_ip_bridge] " << message << '\n';
    }
}

const char* BridgeApp::transportName() const noexcept {
    return config_.transport == TransportMode::Udp ? "UDP" : "TCP";
}

const char* BridgeApp::modeName() const noexcept {
    return config_.mode == ForwardMode::CrsfFrames ? "crsf" : "raw";
}

}  // namespace core
