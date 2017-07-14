#ifndef IODRIVERS_BASE_HPP
#define IODRIVERS_BASE_HPP

#include <iodrivers_base/IOStream.hpp>
#include <vector>

namespace iodrivers_base
{
    /** A IOStream meant to be used to test iodrivers_base functionality
     * from outside
     *
     * It maintains two vectors, one the "to device" buffer and one the "from
     * buffer" device. All communications are synchronous, that is waitRead will
     * throw right away if no data is available.
     * waitWrite never fails.
     */

    class TestStream : public IOStream
    {
        std::vector<uint8_t> to_device;
        std::vector<uint8_t> from_device;

    public:
        /** Push data to the driver "as-if" it was coming from the device
         */
        void pushDataToDriver(std::vector<uint8_t> const& data);

        /** Read data that the driver sent to the device
         *
         * This contains only data sent since the last call to
         * readDataFromDriver
         */
        std::vector<uint8_t> readDataFromDriver();

        void waitRead(base::Time const& timeout);
        void waitWrite(base::Time const& timeout);
        size_t read(uint8_t* buffer, size_t buffer_size);
        size_t write(uint8_t const* buffer, size_t buffer_size);
        void clear();
    };
}

#endif
