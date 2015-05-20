#include <bts/chain/database.hpp>
#include <bts/chain/operations.hpp>
#include <bts/chain/key_object.hpp>
#include <bts/chain/account_object.hpp>

#include <bts/time/time.hpp>

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
      fc::time_point_sec now( BTS_GENESIS_TIMESTAMP );

#ifdef NDEBUG
      ilog("Running in release mode.");
      const int account_count = 2000000;
      const int blocks_to_produce = 1000000;
#else
      ilog("Running in debug mode.");
      const int account_count = 30000;
      const int blocks_to_produce = 1000;
#endif

      for( int i = 0; i < account_count; ++i )
         allocation.emplace_back(public_key_type(fc::ecc::private_key::regenerate(fc::digest(i)).get_public_key()),
                                 BTS_INITIAL_SUPPLY / account_count);

      fc::temp_directory data_dir(fc::current_path());

      {
         database db;
         db.open(data_dir.path(), allocation);

         for( int i = 11; i < account_count + 11; ++i)
            BOOST_CHECK(db.get_balance(account_id_type(i), asset_id_type()).amount == BTS_INITIAL_SUPPLY / account_count);

         fc::time_point start_time = fc::time_point::now();
         db.close();
         ilog("Closed database in ${t} milliseconds.", ("t", (fc::time_point::now() - start_time).count() / 1000));
      }
      {
         database db;

         fc::time_point start_time = fc::time_point::now();
         db.open(data_dir.path());
         ilog("Opened database in ${t} milliseconds.", ("t", (fc::time_point::now() - start_time).count() / 1000));

         for( int i = 11; i < account_count + 11; ++i)
            BOOST_CHECK(db.get_balance(account_id_type(i), asset_id_type()).amount == BTS_INITIAL_SUPPLY / account_count);

         int blocks_out = 0;
         auto delegate_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("genesis")) );
         auto aw = db.get_global_properties().active_witnesses;
         now += db.block_interval();
         auto b =  db.generate_block( now, db.get_scheduled_witness( now )->second, delegate_priv_key, ~0 );

         start_time = fc::time_point::now();
         for( int i = 0; i < blocks_to_produce; ++i )
         {
            signed_transaction trx;
            trx.operations.emplace_back(transfer_operation({asset(1), account_id_type(i + 11), account_id_type(), asset(1), memo_data()}));
            db.push_transaction(trx, ~0);

            aw = db.get_global_properties().active_witnesses;
            now += db.block_interval();
            b =  db.generate_block( now, db.get_scheduled_witness( now )->second, delegate_priv_key, ~0 );
         }
         ilog("Pushed ${c} blocks (1 op each, no validation) in ${t} milliseconds.",
              ("c", blocks_out)("t", (fc::time_point::now() - start_time).count() / 1000));

         start_time = fc::time_point::now();
         db.close();
         ilog("Closed database in ${t} milliseconds.", ("t", (fc::time_point::now() - start_time).count() / 1000));
      }
      {
         database db;

         auto start_time = fc::time_point::now();
         now += BTS_MAX_BLOCK_INTERVAL;
         wlog( "about to start reindex..." );
         db.reindex(data_dir.path(), allocation);
         ilog("Replayed database in ${t} milliseconds.", ("t", (fc::time_point::now() - start_time).count() / 1000));

         for( int i = 0; i < blocks_to_produce; ++i )
            BOOST_CHECK(db.get_balance(account_id_type(i + 11), asset_id_type()).amount == BTS_INITIAL_SUPPLY / account_count - 2);
      }

   } catch(fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}
