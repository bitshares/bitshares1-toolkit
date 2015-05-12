
#include <boost/test/unit_test.hpp>

#include <bts/chain/database.hpp>
#include <bts/chain/operations.hpp>

#include <bts/chain/account_object.hpp>
#include <bts/chain/asset_object.hpp>
#include <bts/chain/key_object.hpp>
#include <bts/chain/delegate_object.hpp>

#include <bts/db/simple_index.hpp>

#include <fc/crypto/digest.hpp>
#include "../common/database_fixture.hpp"

using namespace bts::chain;
using namespace bts::db;

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

BOOST_AUTO_TEST_CASE( serialization_tests )
{
   key_object k;
   k.id = object_id<protocol_ids, key_object_type>(unsigned_int(2));
   BOOST_CHECK(fc::json::from_string(fc::json::to_string(k.id)).as<key_id_type>() == k.id);
   BOOST_CHECK(fc::json::from_string(fc::json::to_string(k.id)).as<object_id_type>() == k.id);
   BOOST_CHECK((fc::json::from_string(fc::json::to_string(k.id)).as<object_id<protocol_ids, key_object_type>>() == k.id));
   public_key_type public_key = fc::ecc::private_key::generate().get_public_key();
   k.key_data = address(public_key);
   BOOST_CHECK(k.key_address() == address(public_key));
}

BOOST_AUTO_TEST_SUITE_END()
