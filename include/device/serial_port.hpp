#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace device {

class SerialPort {
public:
    SerialPort() = default;
    ~SerialPort();

    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;

    void open(const std::string& path, int baud_rate);
    void close();
    bool isOpen() const noexcept;
    int fd() const noexcept;
    int actualBaudRate() const noexcept;
    std::size_t readSome(std::vector<std::uint8_t>& buffer, std::size_t max_bytes);
    bool writeAll(const std::vector<std::uint8_t>& data);

private:
    int fd_ {-1};
    int actual_baud_rate_ {0};
};

}  // namespace device
