#pragma once

#include <bts/chain/evaluator.hpp>

namespace bts { namespace chain {

class withdraw_permission_create_evaluator : public evaluator<withdraw_permission_create_evaluator>
{
public:
   typedef withdraw_permission_create_operation operation_type;

   object_id_type do_evaluate( const operation_type& op );
   object_id_type do_apply( const operation_type& op );
};

class withdraw_permission_claim_evaluator : public evaluator<withdraw_permission_claim_evaluator>
{
public:
   typedef withdraw_permission_claim_operation operation_type;

   void_result do_evaluate( const operation_type& op );
   void_result do_apply( const operation_type& op );
};

class withdraw_permission_update_evaluator : public evaluator<withdraw_permission_update_evaluator>
{
public:
   typedef withdraw_permission_update_operation operation_type;

   void_result do_evaluate( const operation_type& op );
   void_result do_apply( const operation_type& op );
};

class withdraw_permission_delete_evaluator : public evaluator<withdraw_permission_delete_evaluator>
{
public:
   typedef withdraw_permission_delete_operation operation_type;

   void_result do_evaluate( const operation_type& op );
   void_result do_apply( const operation_type& op );
};

} } // bts::chain
