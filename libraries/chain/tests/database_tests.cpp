
#include <bts/chain/database.hpp>
#include <bts/chain/operations.hpp>
#include <bts/chain/account_index.hpp>
#include <bts/chain/asset_index.hpp>
#include <bts/chain/time.hpp>
#include <bts/chain/key_object.hpp>

#include <fc/crypto/digest.hpp>

#include "database_fixture.hpp"

using namespace bts::chain;

BOOST_FIXTURE_TEST_SUITE( operation_unit_tests, database_fixture )

BOOST_AUTO_TEST_CASE( undo_test )
{
   try {
      database db;
      auto& idx = db.get_index<account_balance_object>();


      auto mo = idx.get_meta_object();
      auto unpacked2 = fc::raw::unpack<index_meta_object>(mo.data);
      wdump( (unpacked2) );
      idx.set_meta_object( mo );

      auto bal_obj1 = db.create<account_balance_object>( [&]( account_balance_object* obj ){
               /* no balances right now */
      });
      auto id1 = bal_obj1->id;

      db.undo();
      db.push_undo_state();
      auto bal_obj2 = db.create<account_balance_object>( [&]( account_balance_object* obj ){
               /* no balances right now */
      });
      auto id2 = bal_obj2->id;
      wdump( (id1)(id2) );
      BOOST_CHECK( id1 == id2 );
   } catch ( const fc::exception& e )
   {
      edump( (e.to_detail_string()) );
      throw;
   }
}

BOOST_AUTO_TEST_SUITE_END()
