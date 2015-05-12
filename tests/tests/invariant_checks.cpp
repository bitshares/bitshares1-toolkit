
#include <boost/test/unit_test.hpp>

#include <bts/chain/database.hpp>
#include <bts/chain/operations.hpp>

#include <bts/chain/key_object.hpp>
#include <bts/chain/account_object.hpp>
#include <bts/chain/short_order_object.hpp>
#include <bts/chain/limit_order_object.hpp>

#include <fc/crypto/digest.hpp>

using namespace bts::chain;

BOOST_AUTO_TEST_SUITE( invariant_checks )

BOOST_AUTO_TEST_CASE( share_supply )
{
   try {
      database db;

      int count = 1;
      //Find a number of genesis recipients we can't distribute evenly to
      while( BTS_INITIAL_SUPPLY % ++count == 0 );
      genesis_allocation genesis;
      for( int i = 0; i < count; ++i )
         genesis.push_back(std::make_pair(public_key_type(fc::ecc::private_key::regenerate(fc::digest(fc::to_string(i))).get_public_key()), BTS_INITIAL_SUPPLY / count));
      db.init_genesis(genesis);

      BOOST_CHECK( db.get_balance(account_id_type(), asset_id_type()).amount == 0 );
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_SUITE_END()
