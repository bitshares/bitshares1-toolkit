#include <bts/chain/database.hpp>
#include <bts/chain/operations.hpp>
#include <bts/chain/time.hpp>
#include <bts/chain/key_object.hpp>
#include <bts/chain/account_object.hpp>

#include <fc/crypto/digest.hpp>

#include "database_fixture.hpp"

using namespace bts::chain;

BOOST_AUTO_TEST_CASE( generate_empty_blocks )
{
   try {
      fc::temp_directory data_dir;
      {
         database db;
         db.open(data_dir.path(), genesis_allocation() );

         start_simulated_time( bts::chain::now() );

         auto delegate_priv_key  = fc::ecc::private_key::regenerate(fc::sha256::hash(string("genesis")) );
         for( uint32_t i = 0; i < 100; ++i )
         {
            auto ad = db.get_global_properties().active_delegates;
            advance_simulated_time_to( db.get_next_generation_time(  ad[i%ad.size()] ) );
            auto b =  db.generate_block( delegate_priv_key, ad[i%ad.size()] );
         }
         db.close();
      }
      {
         wlog( "------------------------------------------------" );
         database db;
         db.open(data_dir.path() );
         BOOST_CHECK( db.head_block_num() == 100 );
         auto delegate_priv_key  = fc::ecc::private_key::regenerate(fc::sha256::hash(string("genesis")) );
         for( uint32_t i = 0; i < 100; ++i )
         {
            auto ad = db.get_global_properties().active_delegates;
            advance_simulated_time_to( db.get_next_generation_time(  ad[i%ad.size()] ) );
            auto b = db.generate_block( delegate_priv_key, ad[i%ad.size()] );
         }
         BOOST_CHECK( db.head_block_num() == 200 );
      }
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( undo_block )
{
   try {
      fc::temp_directory data_dir;
      {
         database db;
         db.open(data_dir.path(), genesis_allocation() );

         start_simulated_time( bts::chain::now() );

         auto delegate_priv_key  = fc::ecc::private_key::regenerate(fc::sha256::hash(string("genesis")) );
         for( uint32_t i = 0; i < 5; ++i )
         {
            auto ad = db.get_global_properties().active_delegates;
            advance_simulated_time_to( db.get_next_generation_time(  ad[i%ad.size()] ) );
            auto b =  db.generate_block( delegate_priv_key, ad[i%ad.size()] );
         }
         BOOST_CHECK( db.head_block_num() == 5 );
         db.pop_block();
         BOOST_CHECK( db.head_block_num() == 4 );
         db.pop_block();
         BOOST_CHECK( db.head_block_num() == 3 );
         db.pop_block();
         BOOST_CHECK( db.head_block_num() == 2 );
         for( uint32_t i = 0; i < 5; ++i )
         {
            auto ad = db.get_global_properties().active_delegates;
            advance_simulated_time_to( db.get_next_generation_time(  ad[i%ad.size()] ) );
            auto b =  db.generate_block( delegate_priv_key, ad[i%ad.size()] );
         }
         BOOST_CHECK( db.head_block_num() == 7 );
      }
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( fork_blocks )
{
   try {
      fc::temp_directory data_dir1;
      fc::temp_directory data_dir2;

      database db1;
      db1.open( data_dir1.path(), genesis_allocation() );
      database db2;
      db2.open( data_dir2.path(), genesis_allocation() );

      start_simulated_time( bts::chain::now() );

      auto delegate_priv_key  = fc::ecc::private_key::regenerate(fc::sha256::hash(string("genesis")) );
      for( uint32_t i = 0; i < 20; ++i )
      {
         auto ad = db1.get_global_properties().active_delegates;
         advance_simulated_time_to( db1.get_next_generation_time(  ad[i%ad.size()] ) );
         auto b =  db1.generate_block( delegate_priv_key, ad[i%ad.size()] );
         try {
            db2.push_block(b);
         } FC_CAPTURE_AND_RETHROW( ("db2") );
      }
      for( uint32_t i = 20; i < 23; ++i )
      {
         auto ad1 = db1.get_global_properties().active_delegates;
         advance_simulated_time_to( db1.get_next_generation_time(  ad1[i%ad1.size()] ) );
         auto b =  db1.generate_block( delegate_priv_key, ad1[i%ad1.size()] );
      }
      for( uint32_t i = 23; i < 26; ++i )
      {
         auto ad2 = db2.get_global_properties().active_delegates;
         advance_simulated_time_to( db2.get_next_generation_time(  ad2[i%ad2.size()] ) );
         auto b =  db2.generate_block( delegate_priv_key, ad2[i%ad2.size()] );
         db1.push_block(b);
      }

      //The two databases are on distinct forks now, but at the same height. Make a block on db1, make it invalid, then
      //pass it to db2 and assert that db2 doesn't switch to the new fork.
      signed_block good_block;
      BOOST_CHECK_EQUAL(db1.head_block_num(), 23);
      {
         auto ad2 = db2.get_global_properties().active_delegates;
         advance_simulated_time_to( db2.get_next_generation_time(  ad2[db2.head_block_num()%ad2.size()] ) );
         auto b =  db2.generate_block( delegate_priv_key, ad2[db2.head_block_num()%ad2.size()] );
         good_block = b;
         b.transactions.emplace_back(signed_transaction());
         b.transactions.back().operations.emplace_back(transfer_operation());
         b.sign(delegate_priv_key);
         BOOST_CHECK_EQUAL(b.block_num(), 24);
         BOOST_CHECK_THROW(db1.push_block(b), fc::exception);
      }
      BOOST_CHECK_EQUAL(db1.head_block_num(), 23);

      db1.push_block(good_block);
      BOOST_CHECK_EQUAL(db1.head_block_id().str(), db2.head_block_id().str());
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( undo_pending )
{
   try {
      fc::temp_directory data_dir;
      {
         database db;
         db.open(data_dir.path());

         start_simulated_time( bts::chain::now() );
         auto delegate_priv_key  = fc::ecc::private_key::regenerate(fc::sha256::hash(string("genesis")) );
         const bts::db::index& account_idx = db.get_index(protocol_ids, account_object_type);

         signed_transaction trx;
         trx.relative_expiration = 1000;
         account_id_type nathan_id = account_idx.get_next_id();
         trx.operations.push_back(account_create_operation({account_id_type(), asset(), "nathan"}));
         trx.signatures.push_back(delegate_priv_key.sign_compact(fc::digest((transaction&)trx)));
         db.push_transaction(trx);

         auto ad = db.get_global_properties().active_delegates;
         advance_simulated_time_to( db.get_next_generation_time(  ad[db.head_block_num()%ad.size()] ) );
         auto b =  db.generate_block( delegate_priv_key, ad[db.head_block_num()%ad.size()] );

         BOOST_CHECK(nathan_id(db).name == "nathan");

         trx = decltype(trx)();
         trx.relative_expiration = 1000;
         trx.operations.push_back(transfer_operation({account_id_type(), nathan_id, asset(5000), asset(1)}));
         trx.signatures.push_back(delegate_priv_key.sign_compact(fc::digest((transaction&)trx)));
         db.push_transaction(trx);
         trx = decltype(trx)();
         trx.relative_expiration = 1001;
         trx.operations.push_back(transfer_operation({account_id_type(), nathan_id, asset(5000), asset(1)}));
         trx.signatures.push_back(delegate_priv_key.sign_compact(fc::digest((transaction&)trx)));
         db.push_transaction(trx);

         BOOST_CHECK(nathan_id(db).balances(db).get_balance(asset_id_type()).amount == 10000);
         db.clear_pending();
         BOOST_CHECK(nathan_id(db).balances(db).get_balance(asset_id_type()).amount == 0);
      }
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( switch_forks_undo_create )
{
   try {
      fc::temp_directory dir1,
                         dir2;
      database db1,
               db2;
      db1.open(dir1.path());
      db2.open(dir2.path());

      start_simulated_time(bts::chain::now());
      auto delegate_priv_key  = fc::ecc::private_key::regenerate(fc::sha256::hash(string("genesis")) );
      const bts::db::index& account_idx = db1.get_index(protocol_ids, account_object_type);

      signed_transaction trx;
      trx.relative_expiration = 1000;
      account_id_type nathan_id = account_idx.get_next_id();
      trx.operations.push_back(account_create_operation({account_id_type(), asset(), "nathan"}));
      trx.signatures.push_back(delegate_priv_key.sign_compact(fc::digest((transaction&)trx)));
      db1.push_transaction(trx);

      auto ad = db1.get_global_properties().active_delegates;
      advance_simulated_time_to( db1.get_next_generation_time(  ad[db1.head_block_num()%ad.size()] ) );
      auto b =  db1.generate_block( delegate_priv_key, ad[db1.head_block_num()%ad.size()] );

      BOOST_CHECK(nathan_id(db1).name == "nathan");

      ad = db2.get_global_properties().active_delegates;
      advance_simulated_time_to( db2.get_next_generation_time(  ad[db2.head_block_num()%ad.size()] ) );
      b =  db2.generate_block( delegate_priv_key, ad[db2.head_block_num()%ad.size()] );
      db1.push_block(b);
      ad = db2.get_global_properties().active_delegates;
      advance_simulated_time_to( db2.get_next_generation_time(  ad[db2.head_block_num()%ad.size()] ) );
      b =  db2.generate_block( delegate_priv_key, ad[db2.head_block_num()%ad.size()] );
      db1.push_block(b);

      BOOST_CHECK_THROW(nathan_id(db1), fc::exception);

      db2.push_transaction(trx);

      ad = db2.get_global_properties().active_delegates;
      advance_simulated_time_to( db2.get_next_generation_time(  ad[db2.head_block_num()%ad.size()] ) );
      b =  db2.generate_block( delegate_priv_key, ad[db2.head_block_num()%ad.size()] );
      db1.push_block(b);

      BOOST_CHECK(nathan_id(db1).name == "nathan");
      BOOST_CHECK(nathan_id(db2).name == "nathan");
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( duplicate_transactions )
{
   try {
      fc::temp_directory dir1,
                         dir2;
      database db1,
               db2;
      db1.open(dir1.path());
      db2.open(dir2.path());

      start_simulated_time(bts::chain::now());
      auto delegate_priv_key  = fc::ecc::private_key::regenerate(fc::sha256::hash(string("genesis")) );
      const bts::db::index& account_idx = db1.get_index(protocol_ids, account_object_type);

      signed_transaction trx;
      trx.relative_expiration = 1000;
      account_id_type nathan_id = account_idx.get_next_id();
      trx.operations.push_back(account_create_operation({account_id_type(), asset(), "nathan"}));
      trx.signatures.push_back(delegate_priv_key.sign_compact(fc::digest((transaction&)trx)));
      db1.push_transaction(trx);

      trx = decltype(trx)();
      trx.relative_expiration = 1000;
      trx.operations.push_back(transfer_operation({account_id_type(), nathan_id, asset(500)}));
      trx.signatures.push_back(delegate_priv_key.sign_compact(fc::digest((transaction&)trx)));
      db1.push_transaction(trx);

      BOOST_CHECK_THROW(db1.push_transaction(trx), fc::exception);

      auto ad = db1.get_global_properties().active_delegates;
      advance_simulated_time_to( db1.get_next_generation_time(  ad[db1.head_block_num()%ad.size()] ) );
      auto b =  db1.generate_block( delegate_priv_key, ad[db1.head_block_num()%ad.size()] );
      db2.push_block(b);

      BOOST_CHECK_THROW(db1.push_transaction(trx), fc::exception);
      BOOST_CHECK_THROW(db2.push_transaction(trx), fc::exception);
      BOOST_CHECK(nathan_id(db1).balances(db1).get_balance(asset_id_type()).amount == 500);
      BOOST_CHECK(nathan_id(db2).balances(db1).get_balance(asset_id_type()).amount == 500);
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( tapos )
{
   try {
      fc::temp_directory dir1,
                         dir2;
      database db1,
               db2;
      db1.open(dir1.path());
      db2.open(dir2.path());

      start_simulated_time(bts::chain::now());
      auto delegate_priv_key  = fc::ecc::private_key::regenerate(fc::sha256::hash(string("genesis")) );
      const bts::db::index& account_idx = db1.get_index(protocol_ids, account_object_type);

      auto ad = db1.get_global_properties().active_delegates;
      advance_simulated_time_to( db1.get_next_generation_time(  ad[db1.head_block_num()%ad.size()] ) );
      auto b =  db1.generate_block( delegate_priv_key, ad[db1.head_block_num()%ad.size()] );

      signed_transaction trx;
      trx.ref_block_num = db1.head_block_num();
      //This transaction must be in the next block after its reference, or it is invalid.
      trx.relative_expiration = 1;

      account_id_type nathan_id = account_idx.get_next_id();
      trx.operations.push_back(account_create_operation({account_id_type(), asset(), "nathan"}));
      trx.signatures.push_back(delegate_priv_key.sign_compact(fc::digest((transaction&)trx)));

      //ref_block_prefix isn't set, so we should see an exception here.
      BOOST_REQUIRE_THROW(db1.push_transaction(trx), fc::exception);
      trx.ref_block_prefix = db1.head_block_id()._hash[1];
      trx.signatures.clear();
      trx.signatures.push_back(delegate_priv_key.sign_compact(fc::digest((transaction&)trx)));
      db1.push_transaction(trx);

      ad = db1.get_global_properties().active_delegates;
      advance_simulated_time_to( db1.get_next_generation_time(  ad[db1.head_block_num()%ad.size()] ) );
      b =  db1.generate_block( delegate_priv_key, ad[db1.head_block_num()%ad.size()] );

      trx.operations.clear();
      trx.signatures.clear();
      trx.operations.push_back(transfer_operation({account_id_type(), nathan_id, asset(50)}));
      trx.signatures.push_back(delegate_priv_key.sign_compact(fc::digest((transaction&)trx)));
      //relative_expiration is 1, but ref block is 2 blocks old, so this should fail.
      BOOST_REQUIRE_THROW(db1.push_transaction(trx), fc::exception);
      trx.relative_expiration = 2;
      trx.signatures.clear();
      trx.signatures.push_back(delegate_priv_key.sign_compact(fc::digest((transaction&)trx)));
      db1.push_transaction(trx);
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( maintenance_interval )
{
   try {
      fc::temp_directory data_dir;
      auto delegate_priv_key  = fc::ecc::private_key::regenerate(fc::sha256::hash(string("genesis")) );
      {
         database db;
         db.open(data_dir.path(), genesis_allocation() );

         start_simulated_time( bts::chain::now() );
         auto ad = db.get_global_properties().active_delegates;
         advance_simulated_time_to( db.get_next_generation_time(  ad[db.head_block_num()%ad.size()] ) );
         auto b =  db.generate_block( delegate_priv_key, ad[db.head_block_num()%ad.size()] );

         fc::time_point_sec maintanence_time = db.head_block_time() + fc::seconds(db.get_global_properties().maintenance_interval);
         auto initial_properties = db.get_global_properties();

         for( auto del : initial_properties.active_delegates )
         {
            signed_transaction trx;
            trx.ref_block_prefix = db.head_block_id()._hash[1];
            trx.ref_block_num = 1;
            trx.relative_expiration = 1;
            delegate_update_operation op;
            op.delegate_id = del;
            op.max_transaction_size = 3005;
            trx.operations.push_back(op);
            trx.signatures.push_back(delegate_priv_key.sign_compact(fc::digest((transaction&)trx)));
            db.push_transaction(trx);
         }

         while( maintanence_time >= db.head_block_time() )
         {
            ad = db.get_global_properties().active_delegates;
            advance_simulated_time_to( db.get_next_generation_time(  ad[db.head_block_num()%ad.size()] ) );
            b =  db.generate_block( delegate_priv_key, ad[db.head_block_num()%ad.size()],
                  database::skip_delegate_signature | database::skip_tapos_check );
         }

         ad = db.get_global_properties().active_delegates;
         advance_simulated_time_to( db.get_next_generation_time(  ad[db.head_block_num()%ad.size()] ) );
         b =  db.generate_block( delegate_priv_key, ad[db.head_block_num()%ad.size()] );

         BOOST_CHECK_EQUAL( db.get_global_properties().maximum_transaction_size, 3005 );
         db.close();
      }
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}
