#include <bts/chain/database.hpp>
#include <bts/chain/operations.hpp>
#include <bts/chain/time.hpp>
#include <bts/chain/key_object.hpp>
#include <bts/chain/account_object.hpp>

#include <fc/crypto/digest.hpp>

#include "../common/database_fixture.hpp"

using namespace bts::chain;

BOOST_AUTO_TEST_SUITE(block_tests)

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
            auto aw = db.get_global_properties().active_witnesses;
            advance_simulated_time_to( db.get_next_generation_time(  aw[i%aw.size()] ) );
            auto b =  db.generate_block( delegate_priv_key, aw[i%aw.size()] );
         }
         db.close();
      }
      {
         wlog( "------------------------------------------------" );
         database db;
         db.open(data_dir.path() );
         BOOST_CHECK_EQUAL( db.head_block_num(), 100 );
         auto delegate_priv_key  = fc::ecc::private_key::regenerate(fc::sha256::hash(string("genesis")) );
         for( uint32_t i = 0; i < 100; ++i )
         {
            auto aw = db.get_global_properties().active_witnesses;
            advance_simulated_time_to( db.get_next_generation_time(  aw[i%aw.size()] ) );
            auto b = db.generate_block( delegate_priv_key, aw[i%aw.size()] );
         }
         BOOST_CHECK_EQUAL( db.head_block_num(), 200 );
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
            auto aw = db.get_global_properties().active_witnesses;
            advance_simulated_time_to( db.get_next_generation_time(  aw[i%aw.size()] ) );
            auto b =  db.generate_block( delegate_priv_key, aw[i%aw.size()] );
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
            auto aw = db.get_global_properties().active_witnesses;
            advance_simulated_time_to( db.get_next_generation_time(  aw[i%aw.size()] ) );
            auto b =  db.generate_block( delegate_priv_key, aw[i%aw.size()] );
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
         auto aw = db1.get_global_properties().active_witnesses;
         advance_simulated_time_to( db1.get_next_generation_time(  aw[i%aw.size()] ) );
         auto b =  db1.generate_block( delegate_priv_key, aw[i%aw.size()] );
         try {
            db2.push_block(b);
         } FC_CAPTURE_AND_RETHROW( ("db2") );
      }
      for( uint32_t i = 20; i < 23; ++i )
      {
         auto ad1 = db1.get_global_properties().active_witnesses;
         advance_simulated_time_to( db1.get_next_generation_time(  ad1[i%ad1.size()] ) );
         auto b =  db1.generate_block( delegate_priv_key, ad1[i%ad1.size()] );
      }
      for( uint32_t i = 23; i < 26; ++i )
      {
         auto ad2 = db2.get_global_properties().active_witnesses;
         advance_simulated_time_to( db2.get_next_generation_time(  ad2[i%ad2.size()] ) );
         auto b =  db2.generate_block( delegate_priv_key, ad2[i%ad2.size()] );
         db1.push_block(b);
      }

      //The two databases are on distinct forks now, but at the same height. Make a block on db1, make it invalid, then
      //pass it to db2 and assert that db2 doesn't switch to the new fork.
      signed_block good_block;
      BOOST_CHECK_EQUAL(db1.head_block_num(), 23);
      {
         auto ad2 = db2.get_global_properties().active_witnesses;
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
         account_create_operation cop;
         cop.name = "nathan";
         trx.operations.push_back(cop);
         trx.signatures.push_back(delegate_priv_key.sign_compact(fc::digest((transaction&)trx)));
         db.push_transaction(trx);

         auto aw = db.get_global_properties().active_witnesses;
         advance_simulated_time_to( db.get_next_generation_time(  aw[db.head_block_num()%aw.size()] ) );
         auto b =  db.generate_block( delegate_priv_key, aw[db.head_block_num()%aw.size()] );

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
      account_create_operation cop;
      cop.name = "nathan";
      trx.operations.push_back(cop);
      trx.signatures.push_back(delegate_priv_key.sign_compact(fc::digest((transaction&)trx)));
      db1.push_transaction(trx);

      auto aw = db1.get_global_properties().active_witnesses;
      advance_simulated_time_to( db1.get_next_generation_time(  aw[db1.head_block_num()%aw.size()] ) );
      auto b =  db1.generate_block( delegate_priv_key, aw[db1.head_block_num()%aw.size()] );

      BOOST_CHECK(nathan_id(db1).name == "nathan");

      aw = db2.get_global_properties().active_witnesses;
      advance_simulated_time_to( db2.get_next_generation_time(  aw[db2.head_block_num()%aw.size()] ) );
      b =  db2.generate_block( delegate_priv_key, aw[db2.head_block_num()%aw.size()] );
      db1.push_block(b);
      aw = db2.get_global_properties().active_witnesses;
      advance_simulated_time_to( db2.get_next_generation_time(  aw[db2.head_block_num()%aw.size()] ) );
      b =  db2.generate_block( delegate_priv_key, aw[db2.head_block_num()%aw.size()] );
      db1.push_block(b);

      BOOST_CHECK_THROW(nathan_id(db1), fc::exception);

      db2.push_transaction(trx);

      aw = db2.get_global_properties().active_witnesses;
      advance_simulated_time_to( db2.get_next_generation_time(  aw[db2.head_block_num()%aw.size()] ) );
      b =  db2.generate_block( delegate_priv_key, aw[db2.head_block_num()%aw.size()] );
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
      account_create_operation cop;
      cop.name = "nathan";
      trx.operations.push_back(cop);
      trx.signatures.push_back(delegate_priv_key.sign_compact(fc::digest((transaction&)trx)));
      db1.push_transaction(trx);

      trx = decltype(trx)();
      trx.relative_expiration = 1000;
      trx.operations.push_back(transfer_operation({account_id_type(), nathan_id, asset(500)}));
      trx.signatures.push_back(delegate_priv_key.sign_compact(fc::digest((transaction&)trx)));
      db1.push_transaction(trx);

      BOOST_CHECK_THROW(db1.push_transaction(trx), fc::exception);

      auto aw = db1.get_global_properties().active_witnesses;
      advance_simulated_time_to( db1.get_next_generation_time(  aw[db1.head_block_num()%aw.size()] ) );
      auto b =  db1.generate_block( delegate_priv_key, aw[db1.head_block_num()%aw.size()] );
      db2.push_block(b);

      BOOST_CHECK_THROW(db1.push_transaction(trx), fc::exception);
      BOOST_CHECK_THROW(db2.push_transaction(trx), fc::exception);
      BOOST_CHECK_EQUAL(nathan_id(db1).balances(db1).get_balance(asset_id_type()).amount.value, 500);
      BOOST_CHECK_EQUAL(nathan_id(db2).balances(db2).get_balance(asset_id_type()).amount.value, 500);
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

      auto aw = db1.get_global_properties().active_witnesses;
      advance_simulated_time_to( db1.get_next_generation_time(  aw[db1.head_block_num()%aw.size()] ) );
      auto b =  db1.generate_block( delegate_priv_key, aw[db1.head_block_num()%aw.size()] );

      signed_transaction trx;
      trx.ref_block_num = db1.head_block_num();
      //This transaction must be in the next block after its reference, or it is invalid.
      trx.relative_expiration = 1;

      account_id_type nathan_id = account_idx.get_next_id();
      account_create_operation cop;
      cop.name = "nathan";
      trx.operations.push_back(cop);
      trx.signatures.push_back(delegate_priv_key.sign_compact(fc::digest((transaction&)trx)));

      //ref_block_prefix isn't set, so we should see an exception here.
      BOOST_REQUIRE_THROW(db1.push_transaction(trx), fc::exception);
      trx.ref_block_prefix = db1.head_block_id()._hash[1];
      trx.signatures.clear();
      trx.signatures.push_back(delegate_priv_key.sign_compact(fc::digest((transaction&)trx)));
      db1.push_transaction(trx);

      aw = db1.get_global_properties().active_witnesses;
      advance_simulated_time_to( db1.get_next_generation_time(  aw[db1.head_block_num()%aw.size()] ) );
      b =  db1.generate_block( delegate_priv_key, aw[db1.head_block_num()%aw.size()] );

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

BOOST_FIXTURE_TEST_CASE( maintenance_interval, database_fixture )
{
   try {
      auto delegate_priv_key  = fc::ecc::private_key::regenerate(fc::sha256::hash(string("genesis")) );

      generate_block();
      BOOST_CHECK_EQUAL(db.head_block_num(), 1);
      BOOST_CHECK_EQUAL(account_id_type()(db).owner.weight_threshold, 6);

      fc::time_point_sec maintenence_time = db.get_dynamic_global_properties().next_maintenance_time;
      BOOST_CHECK_GT(maintenence_time.sec_since_epoch(), db.head_block_time().sec_since_epoch());
      BOOST_CHECK_GT(maintenence_time.sec_since_epoch(), bts::chain::now().sec_since_epoch());
      auto initial_properties = db.get_global_properties();
      const account_object& nathan = create_account("nathan");
      const delegate_object nathans_delegate = create_delegate(nathan);
      {
         account_update_operation op;
         op.account = nathan.id;
         op.vote = nathan.votes;
         op.vote->insert(nathans_delegate.vote);
         trx.operations.push_back(op);
         db.push_transaction(trx, ~0);
         trx.operations.clear();
      }
      transfer(account_id_type()(db), nathan, asset(5000));

      generate_blocks(maintenence_time - initial_properties.parameters.block_interval);
      BOOST_CHECK_EQUAL(db.get_global_properties().parameters.maximum_transaction_size,
                        initial_properties.parameters.maximum_transaction_size);
      BOOST_CHECK_EQUAL(db.get_dynamic_global_properties().next_maintenance_time.sec_since_epoch(),
                        db.head_block_time().sec_since_epoch() + db.get_global_properties().parameters.block_interval);
      //Technically the next check could fail because the shuffled witness list happens to match the original list.
      //This should be exceedingly rare, though.
      BOOST_CHECK(db.get_global_properties().active_witnesses != initial_properties.active_witnesses);
      BOOST_CHECK(db.get_global_properties().active_delegates == initial_properties.active_delegates);

      generate_block();

      auto new_properties = db.get_global_properties();
      BOOST_CHECK_EQUAL(account_id_type()(db).owner.weight_threshold, 6);
      BOOST_CHECK(new_properties.active_delegates != initial_properties.active_delegates);
      BOOST_CHECK(std::find(new_properties.active_delegates.begin(),
                            new_properties.active_delegates.end(), nathans_delegate.id) !=
                  new_properties.active_delegates.end());
      BOOST_CHECK_EQUAL(db.get_dynamic_global_properties().next_maintenance_time.sec_since_epoch(),
                        maintenence_time.sec_since_epoch() + new_properties.parameters.maintenance_interval);
      maintenence_time = db.get_dynamic_global_properties().next_maintenance_time;
      BOOST_CHECK_GT(maintenence_time.sec_since_epoch(), db.head_block_time().sec_since_epoch());
      BOOST_CHECK_GT(maintenence_time.sec_since_epoch(), bts::chain::now().sec_since_epoch());
      db.close();
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

/**
 *  Orders should specify a valid expiration time and they will ba automatically canceled if not filled by that time.
 *  This feature allows people to safely submit orders that have a limited lifetime, which is essential to some
 *  traders.
 */
BOOST_FIXTURE_TEST_CASE( short_order_expiration, database_fixture )
{ try {
   //Get a sane head block time
   generate_block();

   auto* test = &create_bitasset("TEST");
   auto* core = &asset_id_type()(db);
   auto* nathan = &create_account("nathan");
   auto* genesis = &account_id_type()(db);

   transfer(*genesis, *nathan, core->amount(50000));

   BOOST_CHECK_EQUAL( get_balance(*nathan, *core), 50000 );

   short_order_create_operation op;
   op.seller = nathan->id;
   op.amount_to_sell = test->amount(500);
   op.collateral = core->amount(500);
   op.expiration = db.head_block_time() + fc::seconds(10);
   trx.operations.push_back(op);
   auto ptrx = db.push_transaction(trx, ~0);

   BOOST_CHECK_EQUAL( get_balance(*nathan, *core), 49500 );

   auto ptrx_id = ptrx.operation_results.back().get<object_id_type>();
   auto short_index = db.get_index_type<short_order_index>().indices();
   auto short_itr = short_index.begin();
   BOOST_REQUIRE( short_itr != short_index.end() );
   BOOST_REQUIRE( short_itr->id == ptrx_id );
   BOOST_REQUIRE( db.find_object(short_itr->id) );
   BOOST_CHECK_EQUAL( get_balance(*nathan, *core), 49500 );
   auto id = short_itr->id;

   generate_blocks(op.expiration);
   test = &get_asset("TEST");
   core = &asset_id_type()(db);
   nathan = &get_account("nathan");
   genesis = &account_id_type()(db);

   BOOST_CHECK(db.find_object(id) == nullptr);
   BOOST_CHECK_EQUAL( get_balance(*nathan, *core), 50000 );
} FC_LOG_AND_RETHROW() }

BOOST_FIXTURE_TEST_CASE( limit_order_expiration, database_fixture )
{ try {
   //Get a sane head block time
   generate_block();

   auto* test = &create_bitasset("TEST");
   auto* core = &asset_id_type()(db);
   auto* nathan = &create_account("nathan");
   auto* genesis = &account_id_type()(db);

   transfer(*genesis, *nathan, core->amount(50000));

   BOOST_CHECK_EQUAL( get_balance(*nathan, *core), 50000 );

   limit_order_create_operation op;
   op.seller = nathan->id;
   op.amount_to_sell = core->amount(500);
   op.min_to_receive = test->amount(500);
   op.expiration = db.head_block_time() + fc::seconds(10);
   trx.operations.push_back(op);
   auto ptrx = db.push_transaction(trx, ~0);

   BOOST_CHECK_EQUAL( get_balance(*nathan, *core), 49500 );

   auto ptrx_id = ptrx.operation_results.back().get<object_id_type>();
   auto limit_index = db.get_index_type<limit_order_index>().indices();
   auto limit_itr = limit_index.begin();
   BOOST_REQUIRE( limit_itr != limit_index.end() );
   BOOST_REQUIRE( limit_itr->id == ptrx_id );
   BOOST_REQUIRE( db.find_object(limit_itr->id) );
   BOOST_CHECK_EQUAL( get_balance(*nathan, *core), 49500 );
   auto id = limit_itr->id;

   generate_blocks(op.expiration);
   test = &get_asset("TEST");
   core = &asset_id_type()(db);
   nathan = &get_account("nathan");
   genesis = &account_id_type()(db);

   BOOST_CHECK(db.find_object(id) == nullptr);
   BOOST_CHECK_EQUAL( get_balance(*nathan, *core), 50000 );
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_SUITE_END()
