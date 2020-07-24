#include <boost/test/unit_test.hpp>

#include <iodrivers_base/URI.hpp>

using namespace std;
using namespace iodrivers_base;

BOOST_AUTO_TEST_SUITE(URISuite)

BOOST_AUTO_TEST_CASE(it_parses_a_full_string) {
    auto uri = URI::parse("sch://host:200?some=option&other=value");
    BOOST_TEST(uri.getScheme() == "sch");
    BOOST_TEST(uri.getHost() == "host");
    BOOST_TEST(uri.getPort() == 200);

    URI::Options options {
        { "some", "option" },
        { "other", "value" }
    };
    BOOST_TEST(uri.getOptions() == options);
}

BOOST_AUTO_TEST_CASE(it_parses_a_string_with_port_without_options) {
    auto uri = URI::parse("sch://host:200");
    BOOST_TEST(uri.getScheme() == "sch");
    BOOST_TEST(uri.getHost() == "host");
    BOOST_TEST(uri.getPort() == 200);
    BOOST_TEST(uri.getOptions().empty());
}

BOOST_AUTO_TEST_CASE(it_parses_a_string_without_port_with_options) {
    auto uri = URI::parse("sch://host?some=option&other=value");
    BOOST_TEST(uri.getScheme() == "sch");
    BOOST_TEST(uri.getHost() == "host");
    BOOST_TEST(uri.getPort() == 0);

    URI::Options options {
        { "some", "option" },
        { "other", "value" }
    };
    BOOST_TEST(uri.getOptions() == options);
}

BOOST_AUTO_TEST_CASE(it_parses_a_string_without_port_without_options) {
    auto uri = URI::parse("sch://host");
    BOOST_TEST(uri.getScheme() == "sch");
    BOOST_TEST(uri.getHost() == "host");
    BOOST_TEST(uri.getPort() == 0);
    BOOST_TEST(uri.getOptions().empty());
}

BOOST_AUTO_TEST_CASE(it_parses_a_string_with_port_and_options_but_no_host) {
    auto uri = URI::parse("sch://:200?some=option&other=value");
    BOOST_TEST(uri.getScheme() == "sch");
    BOOST_TEST(uri.getHost() == "");
    BOOST_TEST(uri.getPort() == 200);

    URI::Options options {
        { "some", "option" },
        { "other", "value" }
    };
    BOOST_TEST(uri.getOptions() == options);
}

BOOST_AUTO_TEST_CASE(it_parses_a_string_with_only_a_port) {
    auto uri = URI::parse("sch://:200");
    BOOST_TEST(uri.getScheme() == "sch");
    BOOST_TEST(uri.getHost() == "");
    BOOST_TEST(uri.getPort() == 200);
    BOOST_TEST(uri.getOptions().empty());
}

BOOST_AUTO_TEST_CASE(it_parses_a_string_with_only_options) {
    auto uri = URI::parse("sch://?some=option&other=value");
    BOOST_TEST(uri.getScheme() == "sch");
    BOOST_TEST(uri.getHost() == "");
    BOOST_TEST(uri.getPort() == 0);

    URI::Options options {
        { "some", "option" },
        { "other", "value" }
    };
    BOOST_TEST(uri.getOptions() == options);
}

BOOST_AUTO_TEST_CASE(it_parses_a_string_with_nothing_but_the_scheme) {
    auto uri = URI::parse("sch://");
    BOOST_TEST(uri.getScheme() == "sch");
    BOOST_TEST(uri.getHost() == "");
    BOOST_TEST(uri.getPort() == 0);
    BOOST_TEST(uri.getOptions().empty());
}

BOOST_AUTO_TEST_CASE(it_throws_if_the_port_is_not_a_number) {
    BOOST_REQUIRE_THROW(
        URI::parse("sch://:some"), std::invalid_argument
    );
}

BOOST_AUTO_TEST_CASE(it_throws_if_the_port_has_trailing_charaters) {
    BOOST_REQUIRE_THROW(
        URI::parse("sch://:200some"), std::invalid_argument
    );
}

BOOST_AUTO_TEST_CASE(it_throws_if_the_scheme_misses_a_slash) {
    BOOST_REQUIRE_THROW(
        URI::parse("sch:/"), std::invalid_argument
    );
}

BOOST_AUTO_TEST_CASE(it_throws_if_the_scheme_misses_a_colon) {
    BOOST_REQUIRE_THROW(
        URI::parse("sch//"), std::invalid_argument
    );
}

BOOST_AUTO_TEST_CASE(it_throws_if_the_uri_has_a_trailing_colon) {
    BOOST_REQUIRE_THROW(
        URI::parse("sch://:"), std::invalid_argument
    );
}

BOOST_AUTO_TEST_CASE(it_throws_if_the_uri_has_a_trailing_question_mark) {
    BOOST_REQUIRE_THROW(
        URI::parse("sch://?"), std::invalid_argument
    );
}

BOOST_AUTO_TEST_CASE(it_throws_if_the_uri_has_a_trailing_ampersand) {
    BOOST_REQUIRE_THROW(
        URI::parse("sch://?some=key&"), std::invalid_argument
    );
}

BOOST_AUTO_TEST_CASE(it_throws_if_an_options_is_missing_a_value) {
    BOOST_REQUIRE_THROW(
        URI::parse("sch://?some"), std::invalid_argument
    );
}

BOOST_AUTO_TEST_CASE(it_throws_if_an_options_is_missing_a_value_even_when_followed_by_another_option) {
    BOOST_REQUIRE_THROW(
        URI::parse("sch://?some&key=value"), std::invalid_argument
    );
}

BOOST_AUTO_TEST_SUITE_END()