#include <bts/chain/database.hpp>
#include <bts/chain/operations.hpp>
#include <bts/chain/account_object.hpp>
#include <bts/chain/asset_object.hpp>
#include <bts/chain/key_object.hpp>
#include <bts/chain/delegate_object.hpp>
#include <bts/chain/simple_index.hpp>

#include <fc/crypto/digest.hpp>
#include "database_fixture.hpp"

using namespace bts::chain;

BOOST_FIXTURE_TEST_SUITE( basic_unit_tests, database_fixture )

BOOST_AUTO_TEST_CASE( price_test )
{
    BOOST_CHECK( price::max(0,1) > price::min(0,1) );
    BOOST_CHECK( price::max(1,0) > price::min(1,0) );
    BOOST_CHECK( price::max(0,1) >= price::min(0,1) );
    BOOST_CHECK( price::max(1,0) >= price::min(1,0) );
    BOOST_CHECK( price::max(0,1) >= price::max(0,1) );
    BOOST_CHECK( price::max(1,0) >= price::max(1,0) );
    BOOST_CHECK( price::min(0,1) < price::max(0,1) );
    BOOST_CHECK( price::min(1,0) < price::max(1,0) );
    BOOST_CHECK( price::min(0,1) <= price::max(0,1) );
    BOOST_CHECK( price::min(1,0) <= price::max(1,0) );
    BOOST_CHECK( price::min(0,1) <= price::min(0,1) );
    BOOST_CHECK( price::min(1,0) <= price::min(1,0) );
    BOOST_CHECK( price::min(1,0) != price::max(1,0) );
    BOOST_CHECK( ~price::max(0,1) != price::min(0,1) );
    BOOST_CHECK( ~price::min(0,1) != price::max(0,1) );
    BOOST_CHECK( ~price::max(0,1) == price::min(1,0) );
    BOOST_CHECK( ~price::min(0,1) == price::max(1,0) );
    BOOST_CHECK( ~price::max(0,1) < ~price::min(0,1) );
    BOOST_CHECK( ~price::max(0,1) <= ~price::min(0,1) );
}
BOOST_AUTO_TEST_SUITE_END()
