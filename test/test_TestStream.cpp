#include <boost/test/unit_test.hpp>

#include <iodrivers_base/Driver.hpp>
#include <iodrivers_base/FixtureBoostTest.hpp>
#include <iodrivers_base/Exceptions.hpp>

using namespace iodrivers_base;
using namespace std;

BOOST_AUTO_TEST_SUITE(TestStreamSuite)

struct Driver : iodrivers_base::Driver
{
public:
    Driver()
        : iodrivers_base::Driver(100) {}

    vector<uint8_t> openURIData;

    void openURI(string const& uri)
    {
        iodrivers_base::Driver::openURI(uri);
        uint8_t buffer[100];
        try {
            size_t packet_size = readPacket(buffer, 100);
            openURIData = vector<uint8_t>(buffer, buffer + packet_size);
        } catch (iodrivers_base::TimeoutError) {
        }
    }

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

struct FixtureNoOpen : iodrivers_base::Fixture<Driver>
{
};

BOOST_FIXTURE_TEST_CASE(it_allows_to_send_data_for_the_benefit_of_openURI_itself, FixtureNoOpen)
{
    uint8_t data[] = { 0, 1, 2, 3 };
    pushDataToDriver(data, data + 4);
    driver.openURI("test://");
    BOOST_REQUIRE(driver.openURIData == vector<uint8_t>(data, data + 4));
}

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
    vector<uint8_t> received = readDataFromDriver();
    BOOST_REQUIRE(received == vector<uint8_t>(data, data + 4));
}

BOOST_FIXTURE_TEST_CASE(it_accumulates_unread_bytes, Fixture)
{
    uint8_t data[] = { 0, 1, 2, 3 };
    writePacket(data, 2);
    writePacket(data + 2, 2);
    vector<uint8_t> received = readDataFromDriver();
    BOOST_REQUIRE(received == vector<uint8_t>(data, data + 4));
}

BOOST_FIXTURE_TEST_CASE(it_does_not_repeat_data_already_read_from_the_device, Fixture)
{
    uint8_t data[] = { 0, 1, 2, 3 };
    writePacket(data, 2);
    readDataFromDriver();
    writePacket(data + 2, 2);
    vector<uint8_t> received = readDataFromDriver();
    BOOST_REQUIRE(received == vector<uint8_t>(data + 2, data + 4));
}

BOOST_FIXTURE_TEST_CASE(it_matches_expecation_with_data_sent_to_device, Fixture)
{
    IODRIVERS_BASE_MOCK();
    uint8_t exp[] = { 0, 1, 2, 3 };
    uint8_t rep[] = { 3, 2, 1, 0 };
    EXPECT_REPLY(vector<uint8_t>(exp, exp + 4),vector<uint8_t>(rep, rep + 4));
    writePacket(exp,4);
    vector<uint8_t> received = readPacket();
    BOOST_REQUIRE(received == vector<uint8_t>(rep,rep+4));
}

BOOST_FIXTURE_TEST_CASE(it_fails_expecation_with_data_sent_to_device, Fixture)
{
    IODRIVERS_BASE_MOCK();
    uint8_t exp[] = { 0, 1, 2, 3 };
    uint8_t msg[] = { 0, 1, 2, 4 };
    uint8_t rep[] = { 3, 2, 1, 0 };
    EXPECT_REPLY(vector<uint8_t>(exp, exp + 4),vector<uint8_t>(rep, rep + 4));
    BOOST_REQUIRE_THROW(writePacket(msg,4), invalid_argument); 
    
}

BOOST_FIXTURE_TEST_CASE(it_tries_to_set_expectation_without_calling_mock_context, Fixture)
{
    uint8_t exp[] = { 0, 1, 2, 3 };
    uint8_t rep[] = { 3, 2, 1, 0 };
    BOOST_REQUIRE_THROW(EXPECT_REPLY(vector<uint8_t>(exp, exp + 4),vector<uint8_t>(rep, rep + 4)), MockContextException);
}

BOOST_FIXTURE_TEST_CASE(it_matches_more_than_one_expecation, Fixture)
{
    IODRIVERS_BASE_MOCK();
    uint8_t exp1[] = { 0, 1, 2, 3 };
    uint8_t rep1[] = { 3, 2, 1, 0 };
    uint8_t exp2[] = { 0, 1, 2, 3, 4 };
    uint8_t rep2[] = { 4, 3, 2, 1, 0 };
    EXPECT_REPLY(vector<uint8_t>(exp1, exp1 + 4),vector<uint8_t>(rep1, rep1 + 4));
    EXPECT_REPLY(vector<uint8_t>(exp2, exp2 + 5),vector<uint8_t>(rep2, rep2 + 5));
    writePacket(exp1,4);
    vector<uint8_t> received_1 = readPacket();
    BOOST_REQUIRE(received_1 == vector<uint8_t>(rep1,rep1+4));
    
    writePacket(exp2,5);
    vector<uint8_t> received_2 = readPacket();
    for(size_t i =0; i<received_2.size(); i++)
    BOOST_REQUIRE(received_2 == vector<uint8_t>(rep2,rep2+5));
}

BOOST_FIXTURE_TEST_CASE(it_does_not_matches_all_expecations, Fixture)
{
    IODRIVERS_BASE_MOCK();
    uint8_t exp1[] = { 0, 1, 2, 3 };
    uint8_t rep1[] = { 3, 2, 1, 0 };
    uint8_t exp2[] = { 0, 1, 2, 3, 4 };
    uint8_t rep2[] = { 4, 3, 2, 1, 0 };
    EXPECT_REPLY(vector<uint8_t>(exp1, exp1 + 4),vector<uint8_t>(rep1, rep1 + 4));
    EXPECT_REPLY(vector<uint8_t>(exp2, exp2 + 5),vector<uint8_t>(rep2, rep2 + 5));
    writePacket(exp1,4);
    vector<uint8_t> received_1 = readPacket();
    BOOST_REQUIRE(received_1 == vector<uint8_t>(rep1,rep1+4));
    BOOST_REQUIRE_THROW(validateExpectationsAreEmpty(), TestEndsWithExpectationsLeftException);
    clearExpectations();
}

BOOST_FIXTURE_TEST_CASE(it_sends_more_messages_than_expecations_set, Fixture)
{
    IODRIVERS_BASE_MOCK();
    uint8_t exp1[] = { 0, 1, 2, 3 };
    uint8_t rep1[] = { 3, 2, 1, 0 };
    uint8_t exp2[] = { 0, 1, 2, 3, 4 };
    EXPECT_REPLY(vector<uint8_t>(exp1, exp1 + 4),vector<uint8_t>(rep1, rep1 + 4));
    writePacket(exp1,4);
    vector<uint8_t> received_1 = readPacket();
    BOOST_REQUIRE(received_1 == vector<uint8_t>(rep1,rep1+4));
    
    BOOST_REQUIRE_THROW(writePacket(exp2,5),runtime_error);
}

BOOST_FIXTURE_TEST_CASE(mock_modes_can_be_used_in_sequence, Fixture)
{
    { IODRIVERS_BASE_MOCK();
        uint8_t exp[] = { 0, 1, 2, 3 };
        uint8_t rep[] = { 3, 2, 1, 0 };
        EXPECT_REPLY(vector<uint8_t>(exp, exp + 4),vector<uint8_t>(rep, rep + 4));
        writePacket(exp,4);
        vector<uint8_t> received = readPacket();
        BOOST_REQUIRE(received == vector<uint8_t>(rep,rep+4));
    }

    { IODRIVERS_BASE_MOCK();
        uint8_t exp[] = { 3, 2, 1, 0 };
        uint8_t rep[] = { 0, 1, 2, 3 };
        EXPECT_REPLY(vector<uint8_t>(exp, exp + 4),vector<uint8_t>(rep, rep + 4));
        writePacket(exp,4);
        vector<uint8_t> received = readPacket();
        BOOST_REQUIRE(received == vector<uint8_t>(rep,rep+4));
    }
}

struct DriverClassNameDriver : Driver
{
    virtual int extractPacket(uint8_t const* buffer, size_t buffer_length) const
    {
        return min(buffer_length, static_cast<size_t>(1));
    }
};

struct DriverClassNameTestFixture : iodrivers_base::Fixture<DriverClassNameDriver>
{
};

BOOST_FIXTURE_TEST_CASE(the_mock_mode_can_be_used_with_a_driver_class_not_called_Driver, DriverClassNameTestFixture)
{
    IODRIVERS_BASE_MOCK();
    uint8_t exp[] = { 0, 1, 2, 3 };
    uint8_t rep[] = { 3, 2, 1, 0 };
    EXPECT_REPLY(vector<uint8_t>(exp, exp + 4),vector<uint8_t>(rep, rep + 4));
    writePacket(exp,4);
    vector<uint8_t> received = readPacket();

    // This is an indirect test for the type of the driver used in the test. The
    // DriverClassNameDriver returns one-byte "packets" while Driver returns the
    // whole buffer
    BOOST_REQUIRE_EQUAL(1, received.size());
}

struct openURIMockTestDriver : public Driver
{
    vector<uint8_t> open(string const& uri)
    {
        Driver::openURI(uri);
        uint8_t data[4] = { 0, 1, 2, 3 };
        writePacket(data, 4);

        uint8_t read[100];
        size_t packet_size = readPacket(read, 100);
        return vector<uint8_t>(read, read + packet_size);
    }
};

struct openURIMockTestFixture : iodrivers_base::Fixture<openURIMockTestDriver>
{
};

BOOST_FIXTURE_TEST_CASE(the_mock_mode_can_be_used_to_test_openURI_itself, openURIMockTestFixture)
{ IODRIVERS_BASE_MOCK();
    uint8_t data[] = { 0, 1, 2, 3 };
    vector<uint8_t> packet(data, data + 4);
    EXPECT_REPLY(packet, packet);
    BOOST_REQUIRE(packet == driver.open("test://"));
}

BOOST_AUTO_TEST_SUITE_END()

