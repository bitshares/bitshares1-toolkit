#include <bts/chain/database.hpp>
#include <bts/chain/operations.hpp>
#include <bts/chain/account_index.hpp>
#include <bts/chain/key_object.hpp>
#include <bts/chain/time.hpp>

#include <fc/crypto/digest.hpp>

#include <boost/test/auto_unit_test.hpp>

using namespace bts::chain;

BOOST_AUTO_TEST_CASE( operation_sanity_check )
{
   try {
      operation op = account_create_operation();
      op.get<account_create_operation>().active.add_authority(account_id_type(), 123);
      operation tmp = std::move(op);
      wdump((tmp.which()));
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( genesis_and_persistence_bench )
{
   try {
      genesis_allocation allocation;
      fc::ecc::private_key private_key = fc::ecc::private_key::generate();
      public_key_type the_key = private_key.get_public_key();

#ifdef NDEBUG
      ilog("Running in release mode.");
      const int account_count = 2000000;
#else
      ilog("Running in debug mode.");
      const int account_count = 30000;
#endif

      for( int i = 0; i < account_count; ++i )
         allocation.emplace_back(the_key, BTS_INITIAL_SUPPLY / account_count);

      fc::temp_directory data_dir(fc::current_path());
      int accounts = 0;

      {
         database db;
         db.open(data_dir.path(), allocation);

         accounts = db.get_account_index().size();
         BOOST_CHECK(accounts >= account_count);
         for( int i = 11; i < account_count + 11; ++i)
            BOOST_CHECK(account_id_type(i)(db)->balances(db)->get_balance(asset_id_type()).amount == BTS_INITIAL_SUPPLY / account_count);

         fc::time_point start_time = fc::time_point::now();
         db.close();
         ilog("Closed database in ${t} milliseconds.", ("t", (fc::time_point::now() - start_time).count() / 1000));
      }
      {
         database db;

         fc::time_point start_time = fc::time_point::now();
         db.open(data_dir.path());
         ilog("Opened database in ${t} milliseconds.", ("t", (fc::time_point::now() - start_time).count() / 1000));

         BOOST_CHECK(db.get_account_index().size() == accounts);
         for( int i = 11; i < account_count + 11; ++i)
            BOOST_CHECK(account_id_type(i)(db)->balances(db)->get_balance(asset_id_type()).amount == BTS_INITIAL_SUPPLY / account_count);

         start_simulated_time( bts::chain::now() );

#ifdef NDEBUG
         int blocks_to_produce = 100000;
#else
         int blocks_to_produce = 1000;
#endif
         int blocks_out = 0;
         auto delegate_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("genesis")) );
         auto ad = db.get_global_properties()->active_delegates;
         advance_simulated_time_to( db.get_next_generation_time( ad[blocks_out % ad.size()] ) );
         auto b =  db.generate_block( delegate_priv_key, ad[blocks_out++ % ad.size()], ~0 );

         start_time = fc::time_point::now();
         for( int i = 0; i < blocks_to_produce; ++i )
         {
            signed_transaction trx;
            trx.operations.emplace_back(transfer_operation({account_id_type(i + 11), account_id_type(), asset(1), asset(1), vector<char>()}));
            db.push_transaction(trx, ~0);

            ad = db.get_global_properties()->active_delegates;
            advance_simulated_time_to( db.get_next_generation_time( ad[blocks_out % ad.size()] ) );
            b =  db.generate_block( delegate_priv_key, ad[blocks_out++ % ad.size()], ~0 );
         }
         ilog("Pushed ${c} blocks (1 op each, no validation) in ${t} milliseconds.",
              ("c", blocks_out)("t", (fc::time_point::now() - start_time).count() / 1000));

         start_time = fc::time_point::now();
         db.close();
         ilog("Closed database in ${t} milliseconds.", ("t", (fc::time_point::now() - start_time).count() / 1000));

         start_time = fc::time_point::now();
         advance_simulated_time_to( now() + fc::seconds(BTS_MAX_BLOCK_INTERVAL) );
         db.reindex(data_dir.path(), allocation);
         ilog("Replayed database in ${t} milliseconds.", ("t", (fc::time_point::now() - start_time).count() / 1000));

         BOOST_CHECK(db.get_account_index().size() == accounts);
         for( int i = 0; i < blocks_to_produce; ++i )
            BOOST_CHECK(account_id_type(i + 11)(db)->balances(db)->get_balance(asset_id_type()).amount == BTS_INITIAL_SUPPLY / account_count - 2);
      }

   } catch(fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}
