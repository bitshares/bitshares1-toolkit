#pragma once
#include <bts/chain/types.hpp>
#include <bts/chain/operations.hpp>

namespace bts { namespace chain {

   /**
    *  All transactions are sets of operations that must be
    *  applied atomicaly.  They must expire no more than 24 hours
    *  after the block_prefix specified.  The block prefix is the
    *  first 4 bytes of the block hash and is used to ensure the
    *  transaction is being executed in the appropriate context.
    */
   struct transaction 
   {
      fc::time_point_sec expiration;
      uint32_t           ref_block_num    = 0;
      uint32_t           ref_block_prefix = 0;
      vector<operation>  operations;
   };

   struct signed_transaction : public transaction
   {
      vector<signature_type> signatures;
   };

   /**
    *  When processing a transaction some operations generate
    *  new object IDs and these IDs cannot be known until the
    *  transaction is actually included into a block.  When a
    *  block is produced these new ids are captured and included
    *  with every transaction.  The index in operation_results should
    *  corespond to the same index in operations.  
    *
    *  If an operation did not create any new object IDs then 0
    *  should be returned.
    */
   struct processed_transaction : public signed_transaction
   {
      processed_transaction(){}
      processed_transaction( const signed_transaction& trx )
      :signed_transaction(trx){}

      vector<object_id_type> operation_results;
   };


} }

FC_REFLECT( bts::chain::transaction, (expiration)(ref_block_num)(ref_block_prefix)(operations) )
FC_REFLECT_DERIVED( bts::chain::signed_transaction, (bts::chain::transaction), (signatures) )
FC_REFLECT_DERIVED( bts::chain::processed_transaction, (bts::chain::signed_transaction), (operation_results) )

