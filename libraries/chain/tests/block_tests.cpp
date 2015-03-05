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
      }
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}
BOOST_AUTO_TEST_SUITE_END()
