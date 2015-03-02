#include <bts/chain/database.hpp>
#include <bts/chain/operations.hpp>
#include <bts/chain/account_index.hpp>
#include <bts/chain/key_object.hpp>
#include <bts/chain/simple_index.hpp>

#include <fc/crypto/digest.hpp>

#include "database_fixture.hpp"

using namespace bts::chain;

BOOST_FIXTURE_TEST_SUITE( invariant_checks, database_fixture )

BOOST_AUTO_TEST_CASE( share_supply )
{
   wdump( (sizeof(operation) ) );

   auto current_supply = db.get_base_asset()->dynamic_asset_data_id(db)->current_supply;
   BOOST_CHECK(db.get_base_asset()->max_supply == current_supply);

   simple_index<account_balance_object>& balance_index = dynamic_cast<simple_index<account_balance_object>&>(db.get_index<account_balance_object>());
   //What do I do now?
   BOOST_CHECK(false);
   for( const auto& a : db.get_account_index() )
   {
   }
   for( const auto& a : balance_index )
   {
   }
}

BOOST_AUTO_TEST_SUITE_END()
