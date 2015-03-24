#pragma once
#include <bts/db/object.hpp>
#include <bts/chain/types.hpp>

namespace bts { namespace chain {
   class database;

   template<typename T>
   class buffer
   {
      T*       data;
      uint64_t size;
   };


   struct script_op
   {
      uint16_t code;
      uint16_t arg1;
      uint16_t arg2;
      uint16_t arg3;
   };
   static_assert( sizeof(script_op) == 8, "verify ops are packed to 8 bytes" );

   struct script_object  : public bts::db::abstract_object<script_object>
   {
      vector<script_op> code;
   };

   struct data_object : public bts::db::abstract_object<data_object>
   {
      account_id_type          owner;
      fc::time_point_sec       last_access;
      vector<char>             data;
      flat_set<object_id_type> saved_permissions;
   };


   class virtual_machine 
   {
      public:
         data_object*                data;
         const script_object*        script;
         const account_object*       account;
         database*                   db;

         buffer<const char>          args;
         buffer<char>                result;
         flat_set<account_id_type>   active_permissions;

         uint16_t pc      = 0;
         uint16_t stackp  = 0;
         uint64_t gas     = 0; // measured in CORE 
         uint16_t depth   = 0;

         void run();
   };

} }

FC_REFLECT( bts::chain::script_op, (code)(arg1)(arg2)(arg3) );
FC_REFLECT_DERIVED( bts::chain::script_object, (bts::db::object), (code) );
FC_REFLECT_DERIVED( bts::chain::data_object, (bts::db::object), (owner)(last_access)(data)(saved_permissions) );

