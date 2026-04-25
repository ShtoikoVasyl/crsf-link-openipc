#include "core/bridge_app.hpp"

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

namespace {

std::string toLower(std::string value) {
    for (char& ch : value) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return value;
}

core::TransportMode parseTransportMode(const std::string& value) {
    const std::string normalized = toLower(value);
    if (normalized == "tcp") {
        return core::TransportMode::Tcp;
    }
    if (normalized == "udp") {
        return core::TransportMode::Udp;
    }
    throw std::runtime_error("Unsupported transport: " + value);
}

core::ForwardMode parseForwardMode(const std::string& value) {
    const std::string normalized = toLower(value);
    if (normalized == "raw") {
        return core::ForwardMode::Raw;
    }
    if (normalized == "crsf" || normalized == "frame" || normalized == "frames") {
        return core::ForwardMode::CrsfFrames;
    }
    throw std::runtime_error("Unsupported mode: " + value);
}

std::atomic<core::BridgeApp*> g_app {nullptr};

void printUsage(const char* program_name) {
    std::cout
        << "Usage: " << program_name << " --host <control-pc-ip> [options]\n"
        << "Options:\n"
        << "  --host <ip>                 Control/configuration PC IPv4 address\n"
        << "  --port <value>              TCP/UDP port. Default: 9000\n"
        << "  --serial <path>             UART device. Default: /dev/ttyS2\n"
        << "  --baud <value>              UART baud rate. Default: 420000\n"
        << "  --transport <tcp|udp>       Network transport. Default: tcp\n"
        << "  --mode <raw|crsf>           Forward raw bytes or validated CRSF frames. Default: raw\n"
        << "  --connect-timeout-ms <n>    Connect timeout. Default: 2000\n"
        << "  --reconnect-delay-ms <n>    Delay before reconnect. Default: 1000\n"
        << "  --poll-timeout-ms <n>       Poll timeout. Default: 250\n"
        << "  --verbose                   Enable stderr logs\n"
        << "  --help                      Show this help message\n";
}

int parseInt(const std::string& value, const std::string& flag) {
    try {
        std::size_t position = 0;
        const int parsed = std::stoi(value, &position);
        if (position != value.size()) {
            throw std::runtime_error("trailing characters");
        }
        return parsed;
    } catch (const std::exception&) {
        throw std::runtime_error("Invalid integer for " + flag + ": " + value);
    }
}

core::AppConfig parseArguments(int argc, char** argv) {
    core::AppConfig config;
    if (const char* env_host = std::getenv("GROUND_HOST")) {
        config.host = env_host;
    }
    if (const char* env_port = std::getenv("GROUND_PORT")) {
        config.port = static_cast<std::uint16_t>(parseInt(env_port, "GROUND_PORT"));
    }
    if (const char* env_serial = std::getenv("SERIAL_PORT")) {
        config.serial_device = env_serial;
    }
    if (const char* env_baud = std::getenv("SERIAL_BAUD")) {
        config.baud_rate = parseInt(env_baud, "SERIAL_BAUD");
    }
    if (const char* env_transport = std::getenv("BRIDGE_TRANSPORT")) {
        config.transport = parseTransportMode(env_transport);
    }
    if (const char* env_mode = std::getenv("BRIDGE_MODE")) {
        config.mode = parseForwardMode(env_mode);
    }

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            std::exit(0);
        }
        if (arg == "--verbose") {
            config.verbose = true;
            continue;
        }
        if (i + 1 >= argc) {
            throw std::runtime_error("Missing value for " + arg);
        }

        const std::string value = argv[++i];
        if (arg == "--host") {
            config.host = value;
        } else if (arg == "--port") {
            config.port = static_cast<std::uint16_t>(parseInt(value, arg));
        } else if (arg == "--serial") {
            config.serial_device = value;
        } else if (arg == "--baud") {
            config.baud_rate = parseInt(value, arg);
        } else if (arg == "--transport") {
            config.transport = parseTransportMode(value);
        } else if (arg == "--mode") {
            config.mode = parseForwardMode(value);
        } else if (arg == "--connect-timeout-ms") {
            config.connect_timeout_ms = parseInt(value, arg);
        } else if (arg == "--reconnect-delay-ms") {
            config.reconnect_delay_ms = parseInt(value, arg);
        } else if (arg == "--poll-timeout-ms") {
            config.poll_timeout_ms = parseInt(value, arg);
        } else {
            throw std::runtime_error("Unknown option: " + arg);
        }
    }

    if (config.host.empty()) {
        throw std::runtime_error("Control/configuration PC host is required. Pass --host or set GROUND_HOST.");
    }
    if (config.port == 0) {
        throw std::runtime_error("Port must be greater than 0.");
    }
    if (config.baud_rate <= 0) {
        throw std::runtime_error("Baud rate must be greater than 0.");
    }
    if (config.connect_timeout_ms <= 0 || config.reconnect_delay_ms < 0 || config.poll_timeout_ms < 0) {
        throw std::runtime_error("Timeout values must be non-negative and connect timeout must be greater than 0.");
    }

    return config;
}

void handleSignal(int) {
    if (core::BridgeApp* app = g_app.load()) {
        app->requestStop();
    }
}

}  // namespace

int main(int argc, char** argv) {
    try {
        auto config = parseArguments(argc, argv);
        core::BridgeApp app(std::move(config));
        g_app.store(&app);

        std::signal(SIGINT, handleSignal);
        std::signal(SIGTERM, handleSignal);

        return app.run();
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return 1;
    }
}
