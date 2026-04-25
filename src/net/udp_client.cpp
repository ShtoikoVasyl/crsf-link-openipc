#include "net/udp_client.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

namespace net {
namespace {

bool setNonBlocking(int fd) {
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    return ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

}  // namespace

UdpClient::~UdpClient() {
    close();
}

bool UdpClient::connectTo(const std::string& host, std::uint16_t port, int) {
    close();

    fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_ < 0) {
        return false;
    }

    if (!setNonBlocking(fd_)) {
        close();
        return false;
    }

    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (::inet_pton(AF_INET, host.c_str(), &address.sin_addr) != 1) {
        close();
        return false;
    }

    if (::connect(fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        close();
        return false;
    }

    return true;
}

void UdpClient::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool UdpClient::isConnected() const noexcept {
    return fd_ >= 0;
}

int UdpClient::fd() const noexcept {
    return fd_;
}

std::size_t UdpClient::readSome(std::vector<std::uint8_t>& buffer, std::size_t max_bytes) {
    if (fd_ < 0) {
        return 0;
    }

    buffer.resize(max_bytes);
    const ssize_t received = ::recv(fd_, buffer.data(), buffer.size(), 0);
    if (received <= 0) {
        buffer.clear();
        return 0;
    }

    buffer.resize(static_cast<std::size_t>(received));
    return static_cast<std::size_t>(received);
}

bool UdpClient::sendAll(const std::vector<std::uint8_t>& data) {
    if (fd_ < 0) {
        return false;
    }

    const ssize_t sent = ::send(fd_, data.data(), data.size(), 0);
    return sent == static_cast<ssize_t>(data.size());
}

}  // namespace net
