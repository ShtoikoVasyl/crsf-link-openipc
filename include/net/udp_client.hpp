#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace net {

class UdpClient {
public:
    UdpClient() = default;
    ~UdpClient();

    UdpClient(const UdpClient&) = delete;
    UdpClient& operator=(const UdpClient&) = delete;

    bool connectTo(const std::string& host, std::uint16_t port, int timeout_ms);
    void close();
    bool isConnected() const noexcept;
    int fd() const noexcept;
    std::size_t readSome(std::vector<std::uint8_t>& buffer, std::size_t max_bytes);
    bool sendAll(const std::vector<std::uint8_t>& data);

private:
    int fd_ {-1};
};

}  // namespace net
