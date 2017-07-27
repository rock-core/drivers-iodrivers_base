#include <gtest/gtest.h>
#include <gtest/gtest-spi.h>
#include <iodrivers_base/Driver.hpp>
#include <iodrivers_base/FixtureGtest.hpp>
#include <iodrivers_base/Exceptions.hpp>

using namespace std;

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

struct DriverTest : public ::testing::Test, public iodrivers_base::Fixture<Driver>
{
    DriverTest()
    {
        driver.openURI("test://");
    }
    
    virtual void TearDown()
    {
        tearDownMock();
    }
   
};

TEST_F(DriverTest, it_matches_expecation_with_data_sent_to_device)
{
    IODRIVERS_BASE_MOCK();
    uint8_t exp[] = { 0, 1, 2, 3 };
    uint8_t rep[] = { 3, 2, 1, 0 };
    EXPECT_REPLY(vector<uint8_t>(exp, exp + 4),vector<uint8_t>(rep, rep + 4));
    writePacket(exp,4);
    vector<uint8_t> received = readPacket();
    ASSERT_EQ(received, vector<uint8_t>(rep,rep+4));
}

TEST_F(DriverTest, it_fails_expecation_with_data_sent_to_device)
{
    IODRIVERS_BASE_MOCK();
    uint8_t exp[] = { 0, 1, 2, 3 };
    uint8_t msg[] = { 0, 1, 2, 4 };
    uint8_t rep[] = { 3, 2, 1, 0 };
    EXPECT_REPLY(vector<uint8_t>(exp, exp + 4),vector<uint8_t>(rep, rep + 4));
    EXPECT_THROW(writePacket(msg,4), std::invalid_argument); 
    
}

TEST_F(DriverTest, it_tries_to_set_expectation_without_calling_mock_context)
{
    uint8_t exp[] = { 0, 1, 2, 3 };
    uint8_t rep[] = { 3, 2, 1, 0 };
     EXPECT_THROW(EXPECT_REPLY(vector<uint8_t>(exp, exp + 4),vector<uint8_t>(rep, rep + 4)), MockContextException);
}

TEST_F(DriverTest, it_matches_expecation_more_than_one_expecation)
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
    ASSERT_EQ(received_1, vector<uint8_t>(rep1,rep1+4));
    
    writePacket(exp2,5);
    vector<uint8_t> received_2 = readPacket();
    for(size_t i =0; i<received_2.size(); i++)
    ASSERT_EQ(received_2, vector<uint8_t>(rep2,rep2+5));
}

TEST_F(DriverTest, it_does_not_matches_all_expecations)
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
    ASSERT_EQ(received_1, vector<uint8_t>(rep1,rep1+4));
    EXPECT_NONFATAL_FAILURE(tearDownMock(),"IODRIVERS_BASE_MOCK Error: Test reached its end without satisfying all expecations.");
    clearExpectations();
}

TEST_F(DriverTest ,it_sends_more_messages_than_expecations_set)
{
    IODRIVERS_BASE_MOCK();
    uint8_t exp1[] = { 0, 1, 2, 3 };
    uint8_t rep1[] = { 3, 2, 1, 0 };
    uint8_t exp2[] = { 0, 1, 2, 3, 4 };
    EXPECT_REPLY(vector<uint8_t>(exp1, exp1 + 4),vector<uint8_t>(rep1, rep1 + 4));
    writePacket(exp1,4);
    vector<uint8_t> received_1 = readPacket();
    ASSERT_EQ(received_1, vector<uint8_t>(rep1,rep1+4));
    
    ASSERT_THROW(writePacket(exp2,5),std::runtime_error);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
