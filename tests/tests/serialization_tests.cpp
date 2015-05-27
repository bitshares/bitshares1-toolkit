
#include <boost/test/unit_test.hpp>

#include <bts/chain/database.hpp>
#include <bts/chain/operations.hpp>

#include <bts/chain/key_object.hpp>

#include <fc/crypto/digest.hpp>
#include <fc/reflect/variant.hpp>

#include "../common/database_fixture.hpp"

using namespace bts::chain;

BOOST_FIXTURE_TEST_SUITE( operation_unit_tests, database_fixture )

BOOST_AUTO_TEST_CASE( serialization_raw_test )
{
   try {
      make_account();
      auto packed = fc::raw::pack( trx );
      signed_transaction unpacked = fc::raw::unpack<signed_transaction>(packed);
      unpacked.validate();
      BOOST_CHECK( trx.digest() == unpacked.digest() );
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}
BOOST_AUTO_TEST_CASE( serialization_json_test )
{
   try {
      make_account();
      fc::variant packed(trx);
      signed_transaction unpacked = packed.as<signed_transaction>();
      unpacked.validate();
      BOOST_CHECK( trx.digest() == unpacked.digest() );
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( json_tests )
{
   try {
   auto var = fc::json::variants_from_string( "10.6 " );
   wdump((var));
   var = fc::json::variants_from_string( "10.5" );
   wdump((var));
   } catch ( const fc::exception& e )
   {
      edump((e.to_detail_string()));
      throw;
   }
}



BOOST_AUTO_TEST_SUITE_END()
