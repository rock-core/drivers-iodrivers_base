#include <iodrivers_base/TestStream.hpp>
#include <iodrivers_base/Exceptions.hpp>
#include <cstring>

using namespace std;
using namespace iodrivers_base;

/** Push data to the device */
void TestStream::pushDataToDriver(vector<uint8_t> const& data)
{
    to_device.insert(to_device.end(), data.begin(), data.end());
}

/** Read all data that the device driver has written since the last
 * call to readDataFromDriver
 */
vector<uint8_t> TestStream::readDataFromDriver()
{
    vector<uint8_t> temp;
    temp.swap(from_device);
    return temp;
}

void TestStream::waitRead(base::Time const& timeout)
{
    if (to_device.empty())
        throw TimeoutError(TimeoutError::NONE, "no data in to_device");
}
void TestStream::waitWrite(base::Time const& timeout)
{
}

size_t TestStream::read(uint8_t* buffer, size_t buffer_size)
{
    size_t read_size = min(to_device.size(), buffer_size);
    std::memcpy(buffer, to_device.data(), read_size);
    to_device.erase(to_device.begin(), to_device.begin() + read_size);
    return read_size;
}

size_t TestStream::write(uint8_t const* buffer, size_t buffer_size)
{
    from_device.insert(from_device.end(), buffer, buffer + buffer_size);
    return buffer_size;
}

void TestStream::clear()
{
    to_device.clear();
}

