#include <bts/chain/database.hpp>
#include <bts/chain/operations.hpp>
#include <bts/chain/account_index.hpp>
#include <bts/chain/key_object.hpp>

#include <fc/crypto/digest.hpp>

#include <boost/test/auto_unit_test.hpp>
#include <boost/filesystem.hpp>

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

BOOST_AUTO_TEST_CASE( genesis_allocation_30k )
{
   try {
      genesis_allocation allocation;
      public_key_type the_key = fc::ecc::private_key::generate().get_public_key();

#ifdef NDEBUG
      ilog("Running in release mode.");
      const int account_count = 2000000;
#else
      ilog("Running in debug mode.");
      const int account_count = 30000;
#endif

      for( int i = 0; i < account_count; ++i )
         allocation.emplace_back(the_key, BTS_INITIAL_SUPPLY / account_count);

      fc::temp_directory data_dir(boost::filesystem::current_path());
      int accounts = 0;

      {
         database db;
         db.open(data_dir.path(), allocation);

         accounts = db.get_account_index().size();
         BOOST_CHECK(accounts >= account_count);

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

         start_time = fc::time_point::now();
         db.close();
         ilog("Closed database in ${t} milliseconds.", ("t", (fc::time_point::now() - start_time).count() / 1000));
      }

   } catch(fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}
