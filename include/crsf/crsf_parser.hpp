#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace crsf {

struct ParsedFrame {
    std::vector<std::uint8_t> bytes;
    bool crc_ok {false};
};

class Parser {
public:
    std::optional<ParsedFrame> pushByte(std::uint8_t byte);
    void reset();

private:
    static constexpr std::size_t kMaxFrameSize = 64;

    std::vector<std::uint8_t> buffer_;
    std::size_t expected_size_ {0};
};

}  // namespace crsf
