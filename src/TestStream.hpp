#ifndef IODRIVERS_BASE_HPP
#define IODRIVERS_BASE_HPP

#include <iodrivers_base/IOStream.hpp>
#include <vector>
#include <list>

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
        std::vector<uint8_t> to_driver;
        std::vector<uint8_t> from_driver;
        std::list<std::vector<uint8_t> > expectations;
        std::list<std::vector<uint8_t> > replies;
        bool mock_mode;       

    public:
        TestStream(): mock_mode(false)
        {}
        
        /** Push data to the driver "as-if" it was coming from the device
         */
        void pushDataToDriver(std::vector<uint8_t> const& data);

        /** Read data that the driver sent to the device
         *
         * This contains only data sent since the last call to
         * readDataFromDriver
         */
        std::vector<uint8_t> readDataFromDriver();

        /**
         * Set a expectation and reply pair and also puts the driver in
         * mock mode. Should be used after setting up IODRIVERS_BASE_MOCK().
         * @param expecation is the message that is expected to be sent by the driver
         * @param reply reply message to be received by the driver
         */
        void EXPECT_REPLY(std::vector<uint8_t> const& expectation, std::vector<uint8_t> const& reply);
        void waitRead(base::Time const& timeout);
        void waitWrite(base::Time const& timeout);
        size_t read(uint8_t* buffer, size_t buffer_size);
        size_t write(uint8_t const* buffer, size_t buffer_size);
        void clear();
        bool expectationsAreEmpty();
        void setMockMode(bool mode);
        void clearExpectations();
    };
}

#endif
