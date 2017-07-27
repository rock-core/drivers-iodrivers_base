#ifndef IODRIVERS_BASE_BOOST_FIXTURE_HPP
#define IODRIVERS_BASE_BOOST_FIXTURE_HPP

#include <iodrivers_base/TestStream.hpp>
#include <vector>
#include <iodrivers_base/Exceptions.hpp>


#ifdef IODRIVERS_BASE_FIXTURE_BOOST_FRAMEWORK
    #include <boost/test/unit_test.hpp>
#endif

#ifdef IODRIVERS_BASE_FIXTURE_GTEST_FRAMEWORK
   #include <gtest/gtest.h>
#endif

#define IODRIVERS_BASE_MOCK() iodrivers_base::Fixture<Driver>::MockContext __context(this);


namespace iodrivers_base
{
    
    /** A fixture class designed to ease testing of iodrivers_base drivers in
     * boost and GTest
     *
     * It creates a given Driver class, which must be opened with the test://
     * URI. It then provides helper methods to provide access to the underlying
     * TestStream.
     *
     * readPacket/writePacket/pushDataToDevice/readDataFromDevice are then
     * available within the test.
     *
     * In boost-test:
     *
     * <code>
     * BOOST_FIXTURE_TEST(MyTest, Fixture<MyDriver>)
     * {
     *   MyDriver.openURI("test://");
     *   uint8_t buffer[4] = { 0, 1, 2, 3 };
     *   pushDataToDriver(buffer, buffer + 2);
     *   auto packet = readPacket();
     *   // Check that the packet matches the expected extraction
     * }
     * </code>
     *
     * In GTest:
     *
     * <code>
     * struct DriverTest : ::testing::Test, iodrivers_base::Fixture<MyDriver>
     * {
     *    Fixture()
     *    {
     *       // Optional: open here
     *       // driver.openURI("test://")
     *    }
     *    
     *    virtual void TearDown()
     *    {
     *      tearDownMock();
     *    }
     * }
     * 
     * TEST_F(DriverTest, it_handles_an_invalid_packet)
     * {
     *   MyDriver.openURI("test://");
     *   uint8_t buffer[4] = { 0, 1, 2, 3 };
     *   pushDataToDriver(buffer, buffer + 2);
     *   auto packet = readPacket();
     *   // Check that the packet matches the expected extraction
     * }
     * </code>
     */
    template<typename Driver>
    struct Fixture
    {
        std::vector<uint8_t> packetBuffer;
        Driver driver;

        Fixture()
        {
            packetBuffer.resize(driver.MAX_PACKET_SIZE);
        }

        /** Get the underlying TestStream
         */
        TestStream* getStream() const
        {
            return dynamic_cast<TestStream*>(driver.getMainStream());
        }

        /** Read a packet from the driver and return it as an std::vector
         */
        std::vector<uint8_t> readPacket()
        {
            size_t size = driver.readPacket(packetBuffer.data(), packetBuffer.size());
            std::vector<uint8_t> packet;
            packet.insert(packet.end(), packetBuffer.begin(), packetBuffer.begin() + size);
            return packet;
        }

        /** Write data to the driver */
        void writePacket(uint8_t const* buffer, size_t size)
        {
            driver.writePacket(buffer, size);
        }

        /** Push data to the driver "as-if" it was coming from the device
         */
        void pushDataToDriver(std::vector<uint8_t> const& data)
        {
            return getStream()->pushDataToDriver(data);
        }

        /** Helper method to allow passing any kind of uint8_t range
         *
         * <code>
         * uint8_t packet[] = { 0, 1, 2, 3 };
         * pushDataToDriver(packet, packet + sizeof(packet));
         * </code>
         */
        template<typename Iterator>
        void pushDataToDriver(Iterator begin, Iterator end)
        {
            std::vector<uint8_t> buffer(begin, end);
            pushDataToDriver(buffer);
        }

        /** Read data that the driver sent to the device
         */
        std::vector<uint8_t> readDataFromDriver()
        {
            return getStream()->readDataFromDriver();
        }

        /** Return the number of bytes currently queued in the driver's internal
         * buffer
         *
         * This is useful mainly when testing extractPacket
         *
         * <code>
         * TEST_F(DriverTest, KeepsSingleBytes)
         * {
         *   uint8_t data = 0x01;
         *   pushDataToDriver(&data, &data + 1);
         *   ASSERT_THROW(TimeoutError, readPacket());
         *   ASSERT_EQUAL(1, getQueuedBytes());
         * }
         * </code>
         */
        unsigned int getQueuedBytes() const
        {
            return driver.getStatus().queued_bytes;
        }
        void EXPECT_REPLY(std::vector<uint8_t> const& expectation, std::vector<uint8_t> const& reply)
        {
            getStream()->EXPECT_REPLY(expectation,reply);
        }

        /**
         * Check if the test has any expectation set and throw if positive.
         * It should be used to check if the test reached its end without 
         * any expecation left only
         */
        void expectationsIsEmpty()
        {
            if(!getStream()->expectationsIsEmpty())
                throw TestEndsWithExcepetionsLeftException();
        }
        
        void enableMockMode()
        {
            getStream()->enableMockMode();
        }
        
        void clearExpectations()
        {
            getStream()->clearExpectations();
        }
        
        /** 
          * GTEST FAIL assertion can only be used in void-returning functions.
          * Constructors and Destructors are not considered void-returning functions, 
          * according to the C++ language specification, and so you may not use fatal assertions in them.
          * Using a Fatal assertion on these method would leave the object in a partially state. 
          * I was then decided to use the tear down to finalize the tests in GTEST.
          * In BOOST the teardown method can be normally called in the destructor method
         */
        void tearDownMock()
        {
            try
            {
                expectationsIsEmpty();
            }
            catch(TestEndsWithExcepetionsLeftException e)    
            {
                #ifdef IODRIVERS_BASE_FIXTURE_GTEST_FRAMEWORK
                ADD_FAILURE() << "IODRIVERS_BASE_MOCK Error: Test reached its end without satisfying all expecations.";
                #endif
                #ifdef IODRIVERS_BASE_FIXTURE_BOOST_FRAMEWORK
                BOOST_ERROR("IODRIVERS_BASE_MOCK Error: Test reached its end without satisfying all expecations.");
                #endif
            }
        }
        
        class MockContext
        { 
        public:
            MockContext() {};
            Fixture* fixture;
            MockContext(Fixture* fixture):
            fixture(fixture)
            {
                fixture->enableMockMode();
            }

            ~MockContext()
            {
                #ifdef IODRIVERS_BASE_FIXTURE_BOOST_FRAMEWORK
                fixture->tearDownMock();
                #endif
            }
            
        };
    };
}

#endif
