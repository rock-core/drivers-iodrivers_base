#include <cstring>
#include <stdexcept>

#include <iodrivers_base/SerialConfiguration.hpp>
#include <iodrivers_base/URI.hpp>

using namespace iodrivers_base;

SerialConfiguration SerialConfiguration::fromURI(URI const& uri) {
    SerialConfiguration result;

    auto options = uri.getOptions();
    auto byte_size_s = options["byte_size"];
    if (!byte_size_s.empty()) {
        int byte_size = stoi(byte_size_s);
        if (byte_size < 5 || byte_size > 8) {
            throw std::invalid_argument(
                "invalid byte_size parameter " + byte_size_s + "in URI, "\
                "expected a value between 5 and 8 (inclusive)"
            );
        }
        result.byte_size = static_cast<ByteSize>(byte_size);
    }

    auto parity = options["parity"];
    if (parity == "none") {
        result.parity = PARITY_NONE;
    }
    else if (parity == "even") {
        result.parity = PARITY_EVEN;
    }
    else if (parity == "odd") {
        result.parity = PARITY_ODD;
    }
    else if (parity != "") {
        throw std::invalid_argument(
            "invalid parity parameter " + parity + "in URI, expected one of "\
            "none, even or odd"
        );
    }

    auto stop = options["stop_bits"];
    if (stop == "1") {
        result.stop_bits = STOP_BITS_ONE;
    }
    else if (stop == "2") {
        result.stop_bits = STOP_BITS_TWO;
    }
    else if (!stop.empty()) {
        throw std::invalid_argument(
            "invalid stop parameter " + stop + " in URI, expected 1 or 2"
        );
    }

    return result;
}