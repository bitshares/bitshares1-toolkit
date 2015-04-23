#pragma once
#include <bts/chain/evaluator.hpp>

namespace bts { namespace chain {

   class key_create_evaluator : public evaluator<key_create_evaluator>
   {
      public:
         typedef key_create_operation operation_type;

         object_id_type do_evaluate( const key_create_operation& op )
         {
            return object_id_type();
         }

         object_id_type do_apply( const key_create_operation& op )
         {
            new_key_object = &db().create<key_object>( [&]( key_object& obj ){
                obj.key_data = op.key_data;
            });

            return new_key_object->id;
         }

         const key_object*                new_key_object = nullptr;
   };

} } // bts::chain
