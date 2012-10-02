#ifndef IODRIVERS_BASE_DRIVER_HPP
#define IODRIVERS_BASE_DRIVER_HPP

#include <boost/cstdint.hpp>
#include <vector>

namespace iodrivers_base
{
    /** Base class for objects that are 'plugged in' a Driver class and get
     * passed all the data that passes through
     */
    class IOListener
    {
    public:
        virtual ~IOListener();

        /** Used to pass data that has been written to the device to the
         * listener
         */
        virtual void writeData(boost::uint8_t const* data, size_t size) = 0;
        /** Used to pass data that has been read from the device to the listener
         */
        virtual void readData(boost::uint8_t const* data, size_t size) = 0;
    };

    /** Implementation of an IOListener that stores the data in a buffer
     */
    class BufferListener : public IOListener
    {
        std::vector<boost::uint8_t> m_writeBuffer;
        std::vector<boost::uint8_t> m_readBuffer;
    public:
        std::vector<boost::uint8_t> flushRead();
        std::vector<boost::uint8_t> flushWrite();

        /** Used to pass data that has been written to the device to the
         * listener
         */
        virtual void writeData(boost::uint8_t const* data, size_t size);
        /** Used to pass data that has been read from the device to the listener
         */
        virtual void readData(boost::uint8_t const* data, size_t size);
    };
}

#endif

