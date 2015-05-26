#include <bts/chain/transfer_evaluator.hpp>
#include <bts/chain/account_object.hpp>

namespace bts { namespace chain {
object_id_type transfer_evaluator::do_evaluate( const transfer_operation& op )
{ try {
   database& d = db();

   const account_object& from_account    = op.from(d);
   const account_object& to_account      = op.to(d);
   const asset_object&   asset_type      = op.amount.asset_id(d);
   const asset_object&   fee_asset_type  = op.fee.asset_id(d);

   if( asset_type.options.flags & white_list )
   {
      FC_ASSERT( to_account.is_authorized_asset( asset_type ) );
      FC_ASSERT( from_account.is_authorized_asset( asset_type ) );
   }

   if( fee_asset_type.options.flags & white_list )
      FC_ASSERT( from_account.is_authorized_asset( asset_type ) );

   if( asset_type.is_transfer_restricted() )
      FC_ASSERT( from_account.id == asset_type.issuer || to_account.id == asset_type.issuer );

   FC_ASSERT( d.get_balance( &from_account, &asset_type ).amount >= op.amount.amount,
              "", ("total_transfer",op.amount)("balance",d.get_balance(&from_account, &asset_type).amount) );

   return object_id_type();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type transfer_evaluator::do_apply( const transfer_operation& o )
{ try {
   db().adjust_balance( o.from, -o.amount );
   db().adjust_balance( o.to, o.amount );
   return object_id_type();
} FC_CAPTURE_AND_RETHROW( (o) )}
} } // bts::chain
