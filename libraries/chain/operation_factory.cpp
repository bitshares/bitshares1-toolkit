#include <bts/chain/operation_factory.hpp>
#include <bts/chain/account_operations.hpp>
#include <bts/chain/asset_operations.hpp>
#include <fc/io/raw.hpp>
#include <fc/container/flat.hpp>

namespace bts { namespace chain { 
   const operation_type create_account_operation::type  = create_account_op_type;
   const operation_type create_asset_operation::type    = create_asset_op_type;

   static bool first_chain = []()->bool{
      bts::chain::operation_factory::instance().register_operation<create_account_operation>();
      bts::chain::operation_factory::instance().register_operation<create_asset_operation>();
      return true;
   }();

   operation_factory& operation_factory::instance()
   {
      static std::unique_ptr<operation_factory> inst( new operation_factory() );
      return *inst;
   }

   void operation_factory::to_variant( const bts::chain::operation& in, fc::variant& output )
   { try {
      FC_ASSERT( _converters[in.type.value] );
      _converters[in.type.value]->to_variant( in, output );
   } FC_RETHROW_EXCEPTIONS( warn, "" ) }

   void operation_factory::from_variant( const fc::variant& in, bts::chain::operation& output )
   { try {
      auto obj = in.get_object();
      output.type = obj["type"].as<operation_type>();

      FC_ASSERT( _converters[output.type] );
      _converters[output.type]->from_variant( in, output );
   } FC_RETHROW_EXCEPTIONS( warn, "", ("in",in) ) }

} }

namespace fc {
   void to_variant( const bts::chain::operation& var,  variant& vo )
   {
      bts::chain::operation_factory::instance().to_variant( var, vo );
   }

   void from_variant( const variant& var,  bts::chain::operation& vo )
   {
      bts::chain::operation_factory::instance().from_variant( var, vo );
   }
}
