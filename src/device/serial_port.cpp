#include "device/serial_port.hpp"

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <system_error>

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#if defined(TDI_PLATFORM_MACOS)
#include <IOKit/serial/ioss.h>
#include <sys/ioctl.h>
#include <termios.h>
#endif

#if defined(TDI_PLATFORM_LINUX)
#include <asm/termbits.h>
#include <sys/ioctl.h>
#endif

namespace device {
namespace {

[[noreturn]] void throwSystemError(const std::string& message) {
    throw std::system_error(errno, std::generic_category(), message);
}

#if defined(TDI_PLATFORM_MACOS)
speed_t pickStandardBaud(int baud_rate) {
    switch (baud_rate) {
        case 9600:
            return B9600;
        case 19200:
            return B19200;
        case 38400:
            return B38400;
        case 57600:
            return B57600;
        case 115200:
            return B115200;
#ifdef B230400
        case 230400:
            return B230400;
#endif
#ifdef B420000
        case 420000:
            return B420000;
#endif
#ifdef B460800
        case 460800:
            return B460800;
#endif
        default:
            throw std::runtime_error("Unsupported baud rate for standard termios path: " + std::to_string(baud_rate));
    }
}
#endif

void configurePort(int fd, int baud_rate) {
#if defined(TDI_PLATFORM_MACOS)
    termios tty {};
    if (tcgetattr(fd, &tty) != 0) {
        throwSystemError("tcgetattr failed");
    }

    cfmakeraw(&tty);
    tty.c_cflag |= static_cast<unsigned long>(CLOCAL | CREAD);
    tty.c_cflag &= static_cast<unsigned long>(~CSTOPB);
    tty.c_cflag &= static_cast<unsigned long>(~CRTSCTS);
    tty.c_cflag &= static_cast<unsigned long>(~PARENB);
    tty.c_cflag &= static_cast<unsigned long>(~CSIZE);
    tty.c_cflag |= CS8;
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0;

    const speed_t fallback = B115200;
    if (cfsetispeed(&tty, fallback) != 0 || cfsetospeed(&tty, fallback) != 0) {
        throwSystemError("cfsetispeed/cfsetospeed failed");
    }
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        throwSystemError("tcsetattr failed");
    }
    speed_t speed = static_cast<speed_t>(baud_rate);
    if (ioctl(fd, IOSSIOSPEED, &speed) == -1) {
        throwSystemError("IOSSIOSPEED failed");
    }
#elif defined(TDI_PLATFORM_LINUX)
    struct termios2 tio2 {};
    if (ioctl(fd, TCGETS2, &tio2) != 0) {
        throwSystemError("TCGETS2 failed");
    }
    tio2.c_cflag &= ~CBAUD;
    tio2.c_cflag &= ~(PARENB | CSTOPB | CSIZE | CRTSCTS);
    tio2.c_cflag |= BOTHER | CLOCAL | CREAD | CS8;
    tio2.c_iflag = 0;
    tio2.c_oflag = 0;
    tio2.c_lflag = 0;
    tio2.c_cc[VMIN] = 0;
    tio2.c_cc[VTIME] = 0;
    tio2.c_ispeed = static_cast<unsigned int>(baud_rate);
    tio2.c_ospeed = static_cast<unsigned int>(baud_rate);
    if (ioctl(fd, TCSETS2, &tio2) != 0) {
        throwSystemError("TCSETS2 failed");
    }
#else
    static_cast<void>(fd);
    static_cast<void>(baud_rate);
    throw std::runtime_error("Unsupported platform");
#endif
}

}  // namespace

SerialPort::~SerialPort() {
    close();
}

void SerialPort::open(const std::string& path, int baud_rate) {
    close();

    fd_ = ::open(path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) {
        throwSystemError("open failed for " + path);
    }

    try {
        configurePort(fd_, baud_rate);
        actual_baud_rate_ = baud_rate;
    } catch (...) {
        ::close(fd_);
        fd_ = -1;
        actual_baud_rate_ = 0;
        throw;
    }
}

void SerialPort::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    actual_baud_rate_ = 0;
}

bool SerialPort::isOpen() const noexcept {
    return fd_ >= 0;
}

int SerialPort::fd() const noexcept {
    return fd_;
}

int SerialPort::actualBaudRate() const noexcept {
    return actual_baud_rate_;
}

std::size_t SerialPort::readSome(std::vector<std::uint8_t>& buffer, std::size_t max_bytes) {
    if (fd_ < 0) {
        throw std::runtime_error("Serial port is not open");
    }

    buffer.resize(max_bytes);
    const ssize_t bytes_read = ::read(fd_, buffer.data(), buffer.size());
    if (bytes_read < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            buffer.clear();
            return 0;
        }
        throwSystemError("read failed");
    }

    buffer.resize(static_cast<std::size_t>(bytes_read));
    return static_cast<std::size_t>(bytes_read);
}

bool SerialPort::writeAll(const std::vector<std::uint8_t>& data) {
    if (fd_ < 0) {
        return false;
    }

    std::size_t offset = 0;
    while (offset < data.size()) {
        const ssize_t written = ::write(fd_, data.data() + offset, data.size() - offset);
        if (written < 0) {
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
        offset += static_cast<std::size_t>(written);
    }
    return true;
}

}  // namespace device
