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
            auto ad = db.get_global_properties()->active_delegates;
            advance_simulated_time_to( db.get_next_generation_time(  ad[i%ad.size()] ) );
            auto b =  db.generate_block( delegate_priv_key, ad[i%ad.size()] );
         }
         db.close();
      }
      {
         database db;
         db.open(data_dir.path() );
         db.push_undo_state();
         db.push_undo_state();
         BOOST_CHECK( db.head_block_num() == 100 );
         auto delegate_priv_key  = fc::ecc::private_key::regenerate(fc::sha256::hash(string("genesis")) );
         for( uint32_t i = 0; i < 100; ++i )
         {
            auto ad = db.get_global_properties()->active_delegates;
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
            auto ad = db.get_global_properties()->active_delegates;
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
            auto ad = db.get_global_properties()->active_delegates;
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
         auto ad = db1.get_global_properties()->active_delegates;
         advance_simulated_time_to( db1.get_next_generation_time(  ad[i%ad.size()] ) );
         ilog( "db1.gen" );
         auto b =  db1.generate_block( delegate_priv_key, ad[i%ad.size()] );
         try {
            ilog( "db2.push" );
            db2.push_block(b);
         } FC_CAPTURE_AND_RETHROW( ("db2") );
      }
      for( uint32_t i = 20; i < 23; ++i )
      {
         auto ad1 = db1.get_global_properties()->active_delegates;
         advance_simulated_time_to( db1.get_next_generation_time(  ad1[i%ad1.size()] ) );
         ilog( "db1.gen" );
         auto b =  db1.generate_block( delegate_priv_key, ad1[i%ad1.size()] );
      }
      for( uint32_t i = 23; i < 29; ++i )
      {
         auto ad2 = db2.get_global_properties()->active_delegates;
         advance_simulated_time_to( db2.get_next_generation_time(  ad2[i%ad2.size()] ) );
         ilog( "db2.gen" );
         auto b =  db2.generate_block( delegate_priv_key, ad2[i%ad2.size()] );
         ilog( "db1.push" );
         db1.push_block(b);
      }
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_SUITE_END()
