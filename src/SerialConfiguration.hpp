#ifndef IODRIVERS_BASE_SERIAL_CONFIGURATION_HPP
#define IODRIVERS_BASE_SERIAL_CONFIGURATION_HPP

namespace iodrivers_base {
    enum ByteSize {
        BITS_5,
        BITS_6,
        BITS_7,
        BITS_8
    };

    enum ParityChecking {
        PARITY_NONE,
        PARITY_EVEN,
        PARITY_ODD
    };

    enum StopBits {
        STOP_BITS_ONE,
        STOP_BITS_TWO
    };

    /** This struct holds a serial port configuration */
    struct SerialConfiguration {
        SerialConfiguration()
            : byte_size(BITS_7)
            , parity(PARITY_NONE)
            , stop_bits(STOP_BITS_ONE) { }

        ByteSize byte_size;
        ParityChecking parity;
        StopBits stop_bits;
    };
}

#endif

