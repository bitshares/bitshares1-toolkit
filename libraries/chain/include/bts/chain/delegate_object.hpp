#pragma once
#include "asset.hpp"

#include <bts/chain/object.hpp>

namespace bts { namespace chain {
   class account_object;

   class delegate_object : public abstract_object<delegate_object>
   {
      public:
         static const uint8_t space_id = protocol_ids;
         static const uint8_t type_id  = delegate_object_type;

         // updates infrequntly
         account_id_type                delegate_account;
         key_id_type                    signing_key;
         fee_schedule_type              fee_schedule;
         uint8_t                        pay_rate = 0;
         uint8_t                        block_interval_sec        = BTS_DEFAULT_BLOCK_INTERVAL;
         uint32_t                       max_block_size            = BTS_DEFAULT_MAX_BLOCK_SIZE;
         uint32_t                       max_transaction_size      = BTS_DEFAULT_MAX_TRANSACTION_SIZE;
         uint32_t                       max_sec_until_expiration  = BTS_DEFAULT_MAX_TIME_UNTIL_EXPIRATION;
         delegate_feeds_id_type         feeds;
         // updated once per round
         secret_hash_type               next_secret;
         secret_hash_type               last_secret;
         share_type                     accumulated_income;
         // updated every transfer
         delegate_vote_id_type          vote;
   };

   /**
    *  Delegate votes are updated all the time which means that an "undo" state
    *  must be generated for every delegate.  Undo states require serializing
    *  and saving a copy of a complete object.  delegate_object is easily 250 bytes
    *  and there are potentially 220 delegates that require updating with
    *  every transaction.  This would generate 53KB of serialized backup state
    *  per processed transaction.
    *
    *  To get around this delegate votes are moved into their own object type that
    *  has less than 32 bytes of backup state or at most 7 KB.  The penalty is
    *  a double-lookup of the object on every vote...
    */
   class delegate_vote_object : public abstract_object<delegate_vote_object>
   {
      public:
         static const uint8_t space_id = implementation_ids;
         static const uint8_t  type_id = impl_delegate_vote_object_type;

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

FC_REFLECT_DERIVED( bts::chain::delegate_object, (bts::chain::object),
                    (delegate_account)
                    (signing_key)
                    (next_secret)
                    (last_secret)
                    (fee_schedule)
                    (pay_rate)
                    (block_interval_sec)
                    (max_block_size)
                    (max_transaction_size)
                    (max_sec_until_expiration)
                    (feeds)
                    (vote) )

FC_REFLECT_DERIVED( bts::chain::delegate_vote_object, (bts::chain::object), (total_votes) )

FC_REFLECT_DERIVED( bts::chain::delegate_feeds_object, (bts::chain::object), (feeds) )
