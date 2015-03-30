#pragma once
#include <bts/chain/types.hpp>
#include <bts/chain/operations.hpp>

namespace bts { namespace chain {

   /**
    *  All transactions are sets of operations that must be
    *  applied atomically.  Transactions must refer to a recent
    *  block that defines the context of the operation so that
    *  they assert a known binding to the object id's referenced
    *  in the transaction.
    *
    *  Rather than specify a full block number, we only specify
    *  the lower 16 bits of the block number which means you can
    *  reference any block within the last 65,536 blocks which
    *  is 3.5 days with a 5 second block interval or 18 hours
    *  with a 1 second interval.
    *
    *  All transactions must expire so that the network does
    *  not have to maintain a permanent record of all transactions
    *  ever published.  Because the network processes transactions
    *  rapidly there is no need to have an expiration time more
    *  than 1 round in the future.  If there are 101 delegates and
    *  a 5 second interval that means 8 minutes.  We can therefore
    *  specify an expiration time as the number of block intervals
    *  since the reference block's time.
    *
    *  Note: The number of block intervals is different than
    *  the number of blocks.  In effect the maximum period that
    *  a transaction is theoretically valid is 18 hours (1 sec interval) to
    *  3.5 days (5 sec interval) if the reference block was
    *  the most recent block.
    *
    *  The block prefix is the first 4 bytes of the block hash of
    *  the reference block number.
    *
    *  Note: A transaction cannot be migrated between forks outside
    *  the period of ref_block_num.time to (ref_block_num.time + rel_exp * interval).
    *  This fact can be used to protect market orders which should specify
    *  a relatively short re-org window of perhaps less than 1 minute.  Normal
    *  payments should probably have a longer re-org window to ensure
    *  their transaction can still go through in the event of a
    *  momentary disruption in service.
    */
   struct transaction
   {
      /** most recent block number with the same lower bits */
      uint16_t           ref_block_num    = 0;
      /** first 32 bits of the ref_block hash */
      uint32_t           ref_block_prefix = 0;
      /** block intervals since ref_block.time_stamp */
      unsigned_int       relative_expiration = 1;
      vector<operation>  operations;

      digest_type digest()const;
      transaction_id_type id()const;
      void validate() const;

      void set_expiration( const block_id_type& reference_block, unsigned_int lifetime_intervals = 3 )
      {
         ref_block_num = ntohl(reference_block._hash[0]);
         ref_block_prefix = reference_block._hash[1];
         relative_expiration = lifetime_intervals;
      }
   };

   struct signed_transaction : public transaction
   {
      signed_transaction( const transaction& trx = transaction() )
         : transaction(trx){}
      vector<signature_type> signatures;
   };

   /**
    *  When processing a transaction some operations generate
    *  new object IDs and these IDs cannot be known until the
    *  transaction is actually included into a block.  When a
    *  block is produced these new ids are captured and included
    *  with every transaction.  The index in operation_results should
    *  correspond to the same index in operations.
    *
    *  If an operation did not create any new object IDs then 0
    *  should be returned.
    */
   struct processed_transaction : public signed_transaction
   {
      processed_transaction( const signed_transaction& trx = signed_transaction() )
         : signed_transaction(trx){}

      vector<operation_result> operation_results;
   };


} }

FC_REFLECT( bts::chain::transaction, (ref_block_num)(ref_block_prefix)(relative_expiration)(operations) )
FC_REFLECT_DERIVED( bts::chain::signed_transaction, (bts::chain::transaction), (signatures) )
FC_REFLECT_DERIVED( bts::chain::processed_transaction, (bts::chain::signed_transaction), (operation_results) )

