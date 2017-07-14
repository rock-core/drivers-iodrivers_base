#include <boost/test/unit_test.hpp>

#include <iodrivers_base/Driver.hpp>
#include <iodrivers_base/Fixture.hpp>

using namespace iodrivers_base;
using namespace std;

BOOST_AUTO_TEST_SUITE(TestStreamSuite)

struct Driver : iodrivers_base::Driver
{
public:
    Driver()
        : iodrivers_base::Driver(100) {}

    int extractPacket(uint8_t const* buffer, size_t size) const
    {
        return size;
    }
};

struct Fixture : iodrivers_base::Fixture<Driver>
{
    Fixture()
    {
        driver.openURI("test://");
    }
};


BOOST_FIXTURE_TEST_CASE(it_sends_data_to_the_Driver, Fixture)
{
    uint8_t data[] = { 0, 1, 2, 3 };
    pushDataToDriver(data, data + 4);
    vector<uint8_t> buffer = readPacket();
    BOOST_REQUIRE(buffer == vector<uint8_t>(data, data + 4));
}

BOOST_FIXTURE_TEST_CASE(it_accumulates_bytes_not_read_by_the_driver, Fixture)
{
    uint8_t data[] = { 0, 1, 2, 3 };
    pushDataToDriver(data, data + 2);
    pushDataToDriver(data + 2, data + 4);
    vector<uint8_t> buffer = readPacket();
    BOOST_REQUIRE(buffer == vector<uint8_t>(data, data + 4));
}

BOOST_FIXTURE_TEST_CASE(it_does_not_repeat_data_already_read_by_the_Driver, Fixture)
{
    uint8_t data[] = { 0, 1, 2, 3 };
    pushDataToDriver(data, data + 2);
    readPacket();
    pushDataToDriver(data + 2, data + 4);
    vector<uint8_t> buffer = readPacket();
    BOOST_REQUIRE(buffer == vector<uint8_t>(data + 2, data + 4));
}

BOOST_FIXTURE_TEST_CASE(it_times_out_instantly, Fixture)
{
    BOOST_REQUIRE_THROW(readPacket(), TimeoutError);
}

BOOST_FIXTURE_TEST_CASE(it_gives_access_to_the_bytes_sent_by_the_driver, Fixture)
{
    uint8_t data[] = { 0, 1, 2, 3 };
    writePacket(data, 4);
    std::vector<uint8_t> received = readDataFromDriver();
    BOOST_REQUIRE(received == vector<uint8_t>(data, data + 4));
}

BOOST_FIXTURE_TEST_CASE(it_accumulates_unread_bytes, Fixture)
{
    uint8_t data[] = { 0, 1, 2, 3 };
    writePacket(data, 2);
    writePacket(data + 2, 2);
    std::vector<uint8_t> received = readDataFromDriver();
    BOOST_REQUIRE(received == vector<uint8_t>(data, data + 4));
}

BOOST_FIXTURE_TEST_CASE(it_does_not_repeat_data_already_read_from_the_device, Fixture)
{
    uint8_t data[] = { 0, 1, 2, 3 };
    writePacket(data, 2);
    readDataFromDriver();
    writePacket(data + 2, 2);
    std::vector<uint8_t> received = readDataFromDriver();
    BOOST_REQUIRE(received == vector<uint8_t>(data + 2, data + 4));
}

BOOST_AUTO_TEST_SUITE_END()

