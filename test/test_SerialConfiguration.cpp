#include <boost/test/unit_test.hpp>

#include <iodrivers_base/URI.hpp>
#include <iodrivers_base/SerialConfiguration.hpp>

using namespace std;
using namespace iodrivers_base;

BOOST_AUTO_TEST_SUITE(SerialConfiguration_fromURI)

BOOST_AUTO_TEST_CASE(it_sets_the_byte_size_from_the_options) {
    URI uri("", "", 0, { { "byte_size", "5" } });
    auto conf = SerialConfiguration::fromURI(uri);
    BOOST_TEST(conf.byte_size == BITS_5);
}

BOOST_AUTO_TEST_CASE(it_does_not_change_the_byte_size_if_the_option_is_unset) {
    URI uri("", "", 0, { });
    auto conf = SerialConfiguration::fromURI(uri);
    BOOST_TEST(conf.byte_size == BITS_8);
}

BOOST_AUTO_TEST_CASE(it_throws_if_the_byte_size_is_invalid) {
    URI uri("", "", 0, { { "byte_size", "4" } });
    BOOST_REQUIRE_THROW(SerialConfiguration::fromURI(uri), std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(it_does_not_change_the_stop_bits_if_the_option_is_unset) {
    URI uri("", "", 0, {});
    auto conf = SerialConfiguration::fromURI(uri);
    BOOST_TEST(conf.stop_bits == STOP_BITS_ONE);
}

BOOST_AUTO_TEST_CASE(it_sets_the_stop_bit_to_1_from_the_stop_option) {
    URI uri("", "", 0, { { "stop_bits", "1" } });
    auto conf = SerialConfiguration::fromURI(uri);
    BOOST_TEST(conf.stop_bits == STOP_BITS_ONE);
}

BOOST_AUTO_TEST_CASE(it_sets_the_stop_bit_to_2_from_the_stop_option) {
    URI uri("", "", 0, { { "stop_bits", "2" } });
    auto conf = SerialConfiguration::fromURI(uri);
    BOOST_TEST(conf.stop_bits == STOP_BITS_TWO);
}

BOOST_AUTO_TEST_CASE(it_throws_if_the_stop_bits_option_is_invalid) {
    URI uri("", "", 0, { { "stop_bits", "0" } });
    BOOST_REQUIRE_THROW(SerialConfiguration::fromURI(uri), std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(it_returns_no_parity_by_default) {
    URI uri("", "", 0, {});
    auto conf = SerialConfiguration::fromURI(uri);
    BOOST_TEST(conf.parity == PARITY_NONE);
}

BOOST_AUTO_TEST_CASE(it_sets_no_parity) {
    URI uri("", "", 0, { { "parity", "none" } });
    auto conf = SerialConfiguration::fromURI(uri);
    BOOST_TEST(conf.parity == PARITY_NONE);
}

BOOST_AUTO_TEST_CASE(it_sets_even_parity) {
    URI uri("", "", 0, { { "parity", "even" } });
    auto conf = SerialConfiguration::fromURI(uri);
    BOOST_TEST(conf.parity == PARITY_EVEN);
}

BOOST_AUTO_TEST_CASE(it_sets_odd_parity) {
    URI uri("", "", 0, { { "parity", "odd" } });
    auto conf = SerialConfiguration::fromURI(uri);
    BOOST_TEST(conf.parity == PARITY_ODD);
}

BOOST_AUTO_TEST_CASE(it_throws_if_the_parity_argument_is_invalid) {
    URI uri("", "", 0, { { "parity", "something" } });
    BOOST_REQUIRE_THROW(SerialConfiguration::fromURI(uri), std::invalid_argument);
}

BOOST_AUTO_TEST_SUITE_END()
