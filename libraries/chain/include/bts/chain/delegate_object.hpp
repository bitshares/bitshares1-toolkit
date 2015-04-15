#pragma once
#include <bts/chain/asset.hpp>
#include <bts/db/object.hpp>

namespace bts { namespace chain {
   using namespace bts::db;

   class account_object;

   class delegate_object : public abstract_object<delegate_object>
   {
      public:
         static const uint8_t space_id = protocol_ids;
         static const uint8_t type_id  = delegate_object_type;

         account_id_type                delegate_account;
         fee_schedule_type              fee_schedule;
         share_type                     witness_pay               = BTS_DEFAULT_WITNESS_PAY;
         uint8_t                        block_interval_sec        = BTS_DEFAULT_BLOCK_INTERVAL;
         uint32_t                       maintenance_interval_sec  = BTS_DEFAULT_MAINTENANCE_INTERVAL;
         uint32_t                       max_transaction_size      = BTS_DEFAULT_MAX_TRANSACTION_SIZE;
         uint32_t                       max_block_size            = BTS_DEFAULT_MAX_BLOCK_SIZE;
         uint16_t                       max_undo_history_size     = BTS_DEFAULT_MAX_UNDO_HISTORY;
         uint32_t                       max_sec_until_expiration  = BTS_DEFAULT_MAX_TIME_UNTIL_EXPIRATION;
         delegate_feeds_id_type         feeds;
         vote_tally_id_type             vote;
   };

   class witness_object : public abstract_object<witness_object>
   {
      public:
         static const uint8_t space_id = protocol_ids;
         static const uint8_t type_id = witness_object_type;

         account_id_type                witness_account;
         key_id_type                    signing_key;
         secret_hash_type               next_secret;
         secret_hash_type               last_secret;
         share_type                     accumulated_income;
         vote_tally_id_type             vote;
   };

   class vote_tally_object : public abstract_object<vote_tally_object>
   {
      public:
         static const uint8_t space_id = implementation_ids;
         static const uint8_t  type_id = impl_vote_tally_object_type;

         share_type  total_votes;
   };

   /**
    * @class delegate_feeds_object
    * @brief tracks price feeds published by a particular delegate
    *
    */
   class delegate_feeds_object : public abstract_object<delegate_feeds_object>
   {
      public:
         static const uint8_t space_id = implementation_ids;
         static const uint8_t type_id  = impl_delegate_feeds_object_type;

         const price_feed* get_feed(asset_id_type base , asset_id_type quote)const;
         price_feed& set_feed( const price_feed& p );

         flat_set<price_feed> feeds;
   };

} } // bts::chain

FC_REFLECT_DERIVED( bts::chain::delegate_object, (bts::db::object),
                    (delegate_account)
                    (fee_schedule)
                    (witness_pay)
                    (block_interval_sec)
                    (maintenance_interval_sec)
                    (max_block_size)
                    (max_transaction_size)
                    (max_undo_history_size)
                    (max_sec_until_expiration)
                    (feeds)
                    (vote) )

FC_REFLECT_DERIVED( bts::chain::witness_object, (bts::db::object),
                    (witness_account)
                    (signing_key)
                    (next_secret)
                    (last_secret)
                    (accumulated_income)
                    (vote) )

FC_REFLECT_DERIVED( bts::chain::vote_tally_object, (bts::db::object), (total_votes) )
FC_REFLECT_DERIVED( bts::chain::delegate_feeds_object, (bts::db::object), (feeds) )
