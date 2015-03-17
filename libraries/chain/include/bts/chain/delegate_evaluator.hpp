#pragma once
#include <bts/chain/evaluator.hpp>
#include <bts/chain/delegate_object.hpp>

namespace bts { namespace chain {

   class delegate_create_evaluator : public evaluator<delegate_create_evaluator>
   {
      public:
         typedef delegate_create_operation operation_type;

         object_id_type do_evaluate( const delegate_create_operation& o );
         object_id_type do_apply( const delegate_create_operation& o );
   };

   class delegate_update_evaluator : public evaluator<delegate_update_evaluator>
   {
      public:
         typedef delegate_update_operation operation_type;

         object_id_type do_evaluate( const delegate_update_operation& o );
         object_id_type do_apply( const delegate_update_operation& o );
   };

   class delegate_publish_feeds_evaluator : public evaluator<delegate_publish_feeds_evaluator>
   {
      public:
         typedef delegate_publish_feeds_operation operation_type;

         object_id_type do_evaluate( const delegate_publish_feeds_operation& o );
         object_id_type do_apply( const delegate_publish_feeds_operation& o );

         const delegate_feeds_object* feed_box = nullptr;
         std::map<std::pair<asset_id_type,asset_id_type>,std::vector<const price_feed*>> all_delegate_feeds;
         std::map<std::pair<asset_id_type,asset_id_type>,price_feed> median_feed_values;
   };

} } // bts::chain
