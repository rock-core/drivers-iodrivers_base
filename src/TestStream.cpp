#include <iodrivers_base/TestStream.hpp>
#include <iodrivers_base/Exceptions.hpp>
#include <cstring>
#include <sstream>
#include <iomanip>

using namespace std;
using namespace iodrivers_base;

TestStream::TestStream()
    : m_mock_mode(false)
    , m_eof(false)
{
}

/** Push data to the driver */
void TestStream::pushDataToDriver(vector<uint8_t> const& data)
{
    to_driver.insert(to_driver.end(), data.begin(), data.end());
}

/** Read all data that the device driver has written since the last
 * call to readDataFromDriver
 */
vector<uint8_t> TestStream::readDataFromDriver()
{
    vector<uint8_t> temp;
    temp.swap(from_driver);
    return temp;
}

bool TestStream::waitRead(base::Time const& timeout)
{
    return !to_driver.empty();
}
bool TestStream::waitWrite(base::Time const& timeout)
{
    return true;
}

void TestStream::EXPECT_REPLY(std::vector<uint8_t> const& expectation, std::vector<uint8_t> const& reply)
{
    if(!m_mock_mode)
        throw MockContextException();
    expectations.push_back(expectation);
    replies.push_back(reply);
}


size_t TestStream::read(uint8_t* buffer, size_t buffer_size)
{
    size_t read_size = min(to_driver.size(), buffer_size);
    std::memcpy(buffer, to_driver.data(), read_size);
    to_driver.erase(to_driver.begin(), to_driver.begin() + read_size);
    return read_size;
}

size_t TestStream::write(uint8_t const* buffer, size_t buffer_size)
{
    if(m_mock_mode)
    {
        from_driver.insert(from_driver.end(), buffer, buffer + buffer_size);
        if(expectations.empty())
        {
            std::stringstream msg;
            msg << "Message received, but there are no expectations left:\n";
            for (std::vector<uint8_t>::const_iterator it = from_driver.begin(); it != from_driver.end(); ++it)
                msg << " " << setfill('0') << setw(2) << hex << static_cast<int>(*it);
            throw std::runtime_error(msg.str());
        }

        if(from_driver == expectations.front())
        {
            to_driver.insert(to_driver.end(), replies.front().begin(), replies.front().end());
            from_driver.clear();
            expectations.pop_front();
            replies.pop_front();
        }
        else
        {
            std::vector<uint8_t> const& expected = expectations.front();
            std::stringstream msg;
            msg << "IODRIVERS_BASE_MOCK failure";
            msg << "\nExpected";
            for (std::vector<uint8_t>::const_iterator it = expected.begin(); it != expected.end(); ++it)
                msg << " " << setfill('0') << setw(2) << hex << static_cast<int>(*it);
            msg << "\nBut got ";
            for (std::vector<uint8_t>::const_iterator it = from_driver.begin(); it != from_driver.end(); ++it)
                msg << " " << setfill('0') << setw(2) << hex << static_cast<int>(*it);

            expectations.clear();
            replies.clear();
            throw std::invalid_argument(msg.str());
        }
        return buffer_size;
    }
    else
    {
        from_driver.insert(from_driver.end(), buffer, buffer + buffer_size);
        return buffer_size;
    }
}

void TestStream::clear()
{
    to_driver.clear();
}

void TestStream::clearExpectations()
{
    expectations.clear();
    replies.clear();
}


bool TestStream::expectationsAreEmpty()
{
    return(expectations.empty());
}

void TestStream::setMockMode(bool mode)
{
    m_mock_mode = mode;
    from_driver.clear();
}

bool TestStream::eof() const
{
    return m_eof;
}

void TestStream::setEOF(bool flag)
{
    m_eof = flag;
}
