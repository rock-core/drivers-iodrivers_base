#include <iodrivers_base/IOListener.hpp>

using namespace iodrivers_base;


IOListener::~IOListener() {}

std::vector<boost::uint8_t> BufferListener::flushRead()
{
    std::vector<boost::uint8_t> ret;
    ret.swap(m_readBuffer);
    return ret;
}
std::vector<boost::uint8_t> BufferListener::flushWrite()
{
    std::vector<boost::uint8_t> ret;
    ret.swap(m_writeBuffer);
    return ret;
}

/** Used to pass data that has been written to the device to the
 * listener
 */
void BufferListener::writeData(boost::uint8_t const* data, size_t size)
{
    m_writeBuffer.insert(m_writeBuffer.end(), data, data + size);
}
/** Used to pass data that has been read from the device to the listener
*/
void BufferListener::readData(boost::uint8_t const* data, size_t size)
{
    m_readBuffer.insert(m_readBuffer.end(), data, data + size);
}


