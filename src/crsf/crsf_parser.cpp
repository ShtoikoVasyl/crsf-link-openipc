#include "crsf/crsf_parser.hpp"

namespace crsf {
namespace {

constexpr std::uint8_t kBroadcastAddress = 0x00;
constexpr std::uint8_t kSyncByte = 0xC8;
constexpr std::uint8_t kExtendedFrameMarker = 0xEE;

std::uint8_t crc8DvbS2(const std::uint8_t* data, std::size_t size) {
    std::uint8_t crc = 0;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x80U) != 0U ? static_cast<std::uint8_t>((crc << 1U) ^ 0xD5U)
                                      : static_cast<std::uint8_t>(crc << 1U);
        }
    }
    return crc;
}

bool looksLikeStartByte(std::uint8_t byte) {
    return byte == kSyncByte || byte == kBroadcastAddress || byte == kExtendedFrameMarker;
}

}  // namespace

std::optional<ParsedFrame> Parser::pushByte(std::uint8_t byte) {
    if (buffer_.empty()) {
        if (!looksLikeStartByte(byte)) {
            return std::nullopt;
        }
        buffer_.push_back(byte);
        return std::nullopt;
    }

    buffer_.push_back(byte);

    if (buffer_.size() == 2) {
        const std::size_t payload_size = buffer_[1];
        if (payload_size < 2 || payload_size + 2 > kMaxFrameSize) {
            reset();
        } else {
            expected_size_ = payload_size + 2;
        }
        return std::nullopt;
    }

    if (expected_size_ == 0 || buffer_.size() < expected_size_) {
        return std::nullopt;
    }

    ParsedFrame frame;
    frame.bytes = buffer_;
    frame.crc_ok = crc8DvbS2(frame.bytes.data() + 2, frame.bytes.size() - 3) == frame.bytes.back();
    reset();
    return frame;
}

void Parser::reset() {
    buffer_.clear();
    expected_size_ = 0;
}

}  // namespace crsf
