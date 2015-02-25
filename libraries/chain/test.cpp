#include <bts/chain/database.hpp>
#include <bts/chain/asset_operations.hpp>

using namespace bts::chain;
int main( int argc, char** argv )
{
   try {
      fc::ecc::public_key dans_key;

      database db;
      db.init_genesis();

      signed_block first_block;
      first_block.block_num = 1;

      create_account_operation op;
      op.name = "dan";
      op.active = op.owner;

   } 
   catch ( const fc::exception& e )
   {
      elog( "${e}", ("e",e.to_detail_string() ) );
   }
   return 0;
}
