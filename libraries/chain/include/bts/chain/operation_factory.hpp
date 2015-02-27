#pragma once

#include <bts/chain/operations.hpp>
#include <bts/chain/transaction_evaluation_state.hpp>
#include <bts/chain/exceptions.hpp>

namespace bts { namespace chain {

   /**
    * @class operation_factory
    *
    *  Enables polymorphic creation and serialization of operation objects in
    *  an manner that can be extended by derived chains.
    */
   class operation_factory
   {
       public:
          static operation_factory& instance();
          class operation_converter_base
          {
             public:
                  virtual ~operation_converter_base(){};
                  virtual void to_variant( const bts::chain::operation& in, fc::variant& out )const = 0;
                  virtual void from_variant( const fc::variant& in, bts::chain::operation& out )const = 0;
          };

          template<typename OperationType>
          class operation_converter : public operation_converter_base
          {
             public:
                  virtual void to_variant( const bts::chain::operation& in, fc::variant& output )const override
                  { try {
                     FC_ASSERT( in.type == OperationType::type );
                     fc::mutable_variant_object obj( "type", in.type );

                     obj[ "data" ] = fc::raw::unpack<OperationType>(in.data);

                     output = std::move(obj);
                  } FC_RETHROW_EXCEPTIONS( warn, "" ) }

                  virtual void from_variant( const fc::variant& in, bts::chain::operation& output )const override
                  { try {
                     auto obj = in.get_object();

                     FC_ASSERT( output.type == OperationType::type );
                     output.data = fc::raw::pack( obj["data"].as<OperationType>() );
                  } FC_RETHROW_EXCEPTIONS( warn, "type: ${type}", ("type",fc::get_typename<OperationType>::name()) ) }
          };

          template<typename OperationType>
          void   register_operation()
          {
             FC_ASSERT( !_converters[ OperationType::type ], 
                        "Operation ID already Registered ${id}", ("id",OperationType::type) );
            _converters[OperationType::type] = std::make_shared< operation_converter<OperationType> >(); 
          }

          /// defined in operations.cpp
          void to_variant( const bts::chain::operation& in, fc::variant& output );
          /// defined in operations.cpp
          void from_variant( const fc::variant& in, bts::chain::operation& output );

          operation_factory():_converters( 255 ){};
       private:
          vector< std::shared_ptr<operation_converter_base> > _converters;
   };

} } // bts::chain 
