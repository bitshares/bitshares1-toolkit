
#include <bts/chain/key_object.hpp>
#include <bts/chain/account_object.hpp>
#include <bts/chain/asset_object.hpp>
#include <bts/chain/delegate_object.hpp>
#include <bts/chain/witness_object.hpp>
#include <bts/chain/limit_order_object.hpp>
#include <bts/chain/short_order_object.hpp>
#include <bts/chain/proposal_object.hpp>
#include <bts/chain/operation_history_object.hpp>
#include <bts/chain/withdraw_permission_object.hpp>
#include <bts/chain/bond_object.hpp>

using namespace fc;
using namespace bts::chain;

namespace bts { namespace wallet {

template< typename ObjectType >
object* create_object_of_type( const variant& v )
{
   return new ObjectType( v.as<ObjectType>() );
}

object* create_object( const variant& v )
{
   const variant_object& obj = v.get_object();
   object_id_type obj_id = obj["id"].as< object_id_type >();

   FC_ASSERT( obj_id.type() == protocol_ids );

   //
   // Sufficiently clever template metaprogramming might
   // be able to convince the compiler to emit this switch
   // instead of creating it explicitly.
   //
   switch( obj_id.space() )
   {
      /*
      case null_object_type:
         return nullptr;
      case base_object_type:
         return create_object_of_type< base_object >( v );
      */
      case key_object_type:
         return create_object_of_type< key_object >( v );
      case account_object_type:
         return create_object_of_type< account_object >( v );
      case asset_object_type:
         return create_object_of_type< asset_object >( v );
      case force_settlement_object_type:
         return create_object_of_type< force_settlement_object >( v );
      case delegate_object_type:
         return create_object_of_type< delegate_object >( v );
      case witness_object_type:
         return create_object_of_type< witness_object >( v );
      case limit_order_object_type:
         return create_object_of_type< limit_order_object >( v );
      case short_order_object_type:
         return create_object_of_type< short_order_object >( v );
      case call_order_object_type:
         return create_object_of_type< call_order_object >( v );
      /*
      case custom_object_type:
         return create_object_of_type< custom_object >( v );
      */
      case proposal_object_type:
         return create_object_of_type< proposal_object >( v );
      case operation_history_object_type:
         return create_object_of_type< operation_history_object >( v );
      case withdraw_permission_object_type:
         return create_object_of_type< withdraw_permission_object >( v );
      case bond_offer_object_type:
         return create_object_of_type< bond_offer_object >( v );
      case bond_object_type:
         return create_object_of_type< bond_object >( v );
      default:
         ;
   }
   FC_ASSERT( false, "unknown type_id" );
}

} }
