#ifndef IODRIVERS_BASE_BOOST_HPP
#define IODRIVERS_BASE_BOOST_HPP

#include <boost/test/unit_test.hpp>
#include <iodrivers_base/Fixture.hpp>

#define IODRIVERS_BASE_MOCK() iodrivers_base::Fixture<fixture_driver_t>::BoostMockContext __context(this);

namespace iodrivers_base {
    template<typename Driver>
    class Fixture<Driver>::BoostMockContext
    { 
    public:
        Fixture* fixture;
        BoostMockContext(Fixture* fixture): fixture(fixture)
        {
            fixture->setMockMode(true);
        }
        
        void tearDown()
        {
            try
            {
                fixture->validateExpectationsAreEmpty();
            }
            catch(TestEndsWithExpectationsLeftException e)
            {
                BOOST_ERROR("IODRIVERS_BASE_MOCK Error: Test reached its end without satisfying all expecations.");
            }
        }
        
        ~BoostMockContext()
        {
            tearDown();
            fixture->setMockMode(false);
        }
        
    };
}

#endif
