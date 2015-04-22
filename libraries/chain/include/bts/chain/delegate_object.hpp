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
         delegate_feeds_id_type         feeds;
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
                    (feeds)
                    (vote) )

FC_REFLECT_DERIVED( bts::chain::vote_tally_object, (bts::db::object), (total_votes) )
FC_REFLECT_DERIVED( bts::chain::delegate_feeds_object, (bts::db::object), (feeds) )
