#pragma once
#include <bts/chain/types.hpp>

namespace bts { namespace chain { 
   enum operation_type
   {
      null_op_type               = 0,
      create_account_op_type     = 1,
      create_asset_op_type       = 2,
      transfer_asset_op_type     = 3,
      operations_count
   };

   struct operation 
   {
      enum_type<uint16_t,operation_type> type = null_op_type;
      vector<char>                       data;

      operation(){}

      operation( operation&& o )
      :type(o.type),data(std::move(o.data)){}

      operation( const operation& o )
      :type(o.type),data(o.data){}

      template<typename OperationType>
      operation( const OperationType& t )
      {
         type = OperationType::type;
         data = fc::raw::pack( t );
      }

      template<typename OperationType>
      OperationType as()const
      {
         FC_ASSERT( (operation_type)type == OperationType::type, 
                    "", ("type",type)("OperationType",OperationType::type) );

         return fc::raw::unpack<OperationType>(data);
      }

      operation& operator=( const operation& o )
      {
         if( this == &o ) return *this;
         type = o.type;
         data = o.data;
         return *this;
      }

      operation& operator=( operation&& o )
      {
         if( this == &o ) return *this;
         type = o.type;
         data = std::move(o.data);
         return *this;
      }
   };
} } // bts::chain

FC_REFLECT( bts::chain::operation, (type)(data) )
FC_REFLECT_ENUM( bts::chain::operation_type, 
                 (null_op_type) 
                 (create_account_op_type)
                 (create_asset_op_type)
                 (transfer_asset_op_type)
                 (operations_count)
               )
