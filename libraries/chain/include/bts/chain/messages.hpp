#pragma once
#include <bts/chain/block.hpp>

namespace bts { namespace chain {

   enum message_type_enum
   {
      trx_message_type          = 1000,
      block_message_type        = 1001
   };

   struct trx_message
   {
      static const message_type_enum type;

      signed_transaction trx;
      trx_message() {}
      trx_message(signed_transaction transaction) :
        trx(std::move(transaction))
      {}
   };

   struct block_message
   {
      static const message_type_enum type;

      block_message(){}
      block_message(const signed_block& blk )
      :block(blk),block_id(blk.id()){}

      signed_block    block;
      block_id_type block_id;

   };

} } // bts::chain

FC_REFLECT_ENUM( bts::chain::message_type_enum, (trx_message_type)(block_message_type) )
FC_REFLECT( bts::chain::trx_message, (trx) )
FC_REFLECT( bts::chain::block_message, (block)(block_id) )
