#ifndef IODRIVERS_BASE_SERIAL_CONFIGURATION_HPP
#define IODRIVERS_BASE_SERIAL_CONFIGURATION_HPP

namespace iodrivers_base {
    enum ByteSize {
        BITS_5 = 5,
        BITS_6 = 6,
        BITS_7 = 7,
        BITS_8 = 8
    };

    enum ParityChecking {
        PARITY_NONE = 'N',
        PARITY_EVEN = 'E',
        PARITY_ODD = 'O'
    };

    enum StopBits {
        STOP_BITS_ONE = 1,
        STOP_BITS_TWO = 2
    };

    struct URI;

    /** This struct holds a serial port configuration */
    struct SerialConfiguration {
        SerialConfiguration()
            : byte_size(BITS_8)
            , parity(PARITY_NONE)
            , stop_bits(STOP_BITS_ONE) { }

        ByteSize byte_size;
        ParityChecking parity;
        StopBits stop_bits;

        /** Create a serial configuration from the options of an URI
         *
         * The following parameters are recognized:
         * - byte size: any value from 5 to 8
         * - parity: either none, even or odd
         * - stop: 1 or 2
         */
        static SerialConfiguration fromURI(URI const& uri);
    };
}

#endif
