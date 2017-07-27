#include <iodrivers_base/TestStream.hpp>
#include <iodrivers_base/Exceptions.hpp>
#include <cstring>

using namespace std;
using namespace iodrivers_base;

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

void TestStream::waitRead(base::Time const& timeout)
{
    if (to_driver.empty())
        throw TimeoutError(TimeoutError::NONE, "no data in to_device");
}
void TestStream::waitWrite(base::Time const& timeout)
{
}

void TestStream::EXPECT_REPLY(std::vector<uint8_t> const& expectation, std::vector<uint8_t> const& reply)
{
    if(!mock_mode)
        throw MockContextException();
    std::vector<uint8_t> exp;
    exp.insert(exp.end(),expectation.begin(),expectation.end());
    std::vector<uint8_t> rep = reply;
    expectations.push_back(exp);
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
    if(mock_mode)
    {
        from_driver.clear();
        from_driver.insert(from_driver.end(), buffer, buffer + buffer_size);
        if(expectations.size() > 0)
        {   
            if(from_driver == expectations[0])
            {
                to_driver.insert(to_driver.end(), replies[0].begin(), replies[0].end());
                expectations.erase(expectations.begin());
                replies.erase(replies.begin());
            }
            else
            {
                expectations.erase(expectations.begin(),expectations.end());
                replies.erase(replies.begin(),replies.end());
                throw std::invalid_argument("Message received doesn't match expectation set");
            }
            return buffer_size;
        }
        else
            throw std::runtime_error("Message received without any expecation left.");
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


bool TestStream::expectationsIsEmpty()
{
    return(!expectations.size()>0);
}

void TestStream::enableMockMode()
{
    mock_mode = true;
}