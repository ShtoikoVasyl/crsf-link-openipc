#include "net/tcp_client.hpp"

#include <cerrno>
#include <cstring>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
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

bool waitForConnected(int fd, int timeout_ms) {
    pollfd descriptor {};
    descriptor.fd = fd;
    descriptor.events = POLLOUT;

    const int poll_result = ::poll(&descriptor, 1, timeout_ms);
    if (poll_result <= 0) {
        return false;
    }

    int socket_error = 0;
    socklen_t length = sizeof(socket_error);
    return ::getsockopt(fd, SOL_SOCKET, SO_ERROR, &socket_error, &length) == 0 && socket_error == 0;
}

}  // namespace

TcpClient::~TcpClient() {
    close();
}

bool TcpClient::connectTo(const std::string& host, std::uint16_t port, int timeout_ms) {
    close();

    fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0) {
        return false;
    }

    if (!setNonBlocking(fd_)) {
        close();
        return false;
    }

    int flag = 1;
    ::setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    ::setsockopt(fd_, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(flag));

    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (::inet_pton(AF_INET, host.c_str(), &address.sin_addr) != 1) {
        close();
        return false;
    }

    const int connect_result = ::connect(fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address));
    if (connect_result == 0) {
        return true;
    }

    if (errno != EINPROGRESS) {
        close();
        return false;
    }

    if (!waitForConnected(fd_, timeout_ms)) {
        close();
        return false;
    }

    return true;
}

void TcpClient::close() {
    if (fd_ >= 0) {
        ::shutdown(fd_, SHUT_RDWR);
        ::close(fd_);
        fd_ = -1;
    }
}

bool TcpClient::isConnected() const noexcept {
    return fd_ >= 0;
}

int TcpClient::fd() const noexcept {
    return fd_;
}

std::size_t TcpClient::readSome(std::vector<std::uint8_t>& buffer, std::size_t max_bytes) {
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

bool TcpClient::sendAll(const std::vector<std::uint8_t>& data) {
    if (fd_ < 0) {
        return false;
    }

    std::size_t offset = 0;
    while (offset < data.size()) {
        const ssize_t sent = ::send(fd_, data.data() + offset, data.size() - offset, 0);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                pollfd descriptor {};
                descriptor.fd = fd_;
                descriptor.events = POLLOUT;
                if (::poll(&descriptor, 1, 250) < 0) {
                    return false;
                }
                continue;
            }
            return false;
        }
        if (sent == 0) {
            return false;
        }
        offset += static_cast<std::size_t>(sent);
    }
    return true;
}

}  // namespace net
