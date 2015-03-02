#include <bts/chain/asset_evaluator.hpp>
#include <bts/chain/asset_object.hpp>
#include <bts/chain/asset_index.hpp>
#include <bts/chain/account_object.hpp>
#include <bts/chain/database.hpp>

namespace bts { namespace chain {
object_id_type asset_create_evaluator::evaluate( const operation& o )
{
   const auto& op = o.get<asset_create_operation>();
   database& d = db();

   FC_ASSERT( !d.get_asset_index().get( op.symbol ) );

   auto bts_fee_paid = pay_fee( op.issuer, op.fee );
   bts_fee_required = op.calculate_fee( d.current_fee_schedule() );
   FC_ASSERT( bts_fee_paid >= bts_fee_required );

   const asset_object* core_asset = asset_id_type()(d);
   fees_paid[core_asset].to_issuer -= bts_fee_required.value/2;
   assert( fees_paid[core_asset].to_issuer >= 0 );

   for( auto fp : op.feed_producers )
      FC_ASSERT( fp(d) != nullptr );

   return object_id_type();
}

object_id_type asset_create_evaluator::apply( const operation& o )
{
   const auto& op = o.get<asset_create_operation>();
   apply_delta_balances();
   apply_delta_fee_pools();

   const asset_dynamic_data_object* dyn_asset =
      db().create<asset_dynamic_data_object>( [&]( asset_dynamic_data_object* a ) {
         a->current_supply = 0;
         a->fee_pool = bts_fee_required.value / 2;
      });

   auto next_asset_id = db().get_asset_index().get_next_available_id();

   const asset_object* new_asset =
     db().create<asset_object>( [&]( asset_object* a ) {
         a->symbol = op.symbol;
         a->max_supply = op.max_supply;
         a->flags = op.flags;
         a->issuer_permissions = op.permissions;
         a->short_backing_asset = op.short_backing_asset;
         a->feed_producers = op.feed_producers;
         a->issuer = op.issuer;
         a->core_exchange_rate = op.core_exchange_rate;
         a->core_exchange_rate.base.asset_id = 0;
         a->core_exchange_rate.quote.amount = next_asset_id;
         a->core_exchange_rate.quote.asset_id = 0;
         a->dynamic_asset_data_id = dyn_asset->id;
      });
   assert( new_asset->id == next_asset_id );

   return next_asset_id;
}
} } // bts::chain
