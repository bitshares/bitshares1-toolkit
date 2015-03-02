#include <bts/chain/asset_evaluator.hpp>
#include <bts/chain/asset_object.hpp>
#include <bts/chain/asset_index.hpp>
#include <bts/chain/database.hpp>

namespace bts { namespace chain {
object_id_type asset_create_evaluator::evaluate( const operation& o )
{
   const auto& op = o.get<asset_create_operation>();
   database& d = db();

   FC_ASSERT( !d.get_asset_index().get( op.symbol ) );

   auto bts_fee_paid = pay_fee( op.issuer, op.fee );
   auto bts_fee_required = op.calculate_fee( d.current_fee_schedule() );
   FC_ASSERT( bts_fee_paid >= bts_fee_required );

   return object_id_type();
}

object_id_type asset_create_evaluator::apply( const operation& o )
{
   apply_delta_balances();
   apply_delta_fee_pools();

   return object_id_type();
}
} } // bts::chain
