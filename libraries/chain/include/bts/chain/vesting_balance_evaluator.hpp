#pragma once

#include <bts/chain/evaluator.hpp>

namespace bts { namespace chain {

class vesting_balance_create_evaluator;
class vesting_balance_withdraw_evaluator;

class vesting_balance_create_evaluator : public evaluator<vesting_balance_create_evaluator>
{
    public:
        typedef vesting_balance_create_operation operation_type;

        object_id_type do_evaluate( const vesting_balance_create_operation& op );
        object_id_type do_apply( const vesting_balance_create_operation& op );
};

class vesting_balance_withdraw_evaluator : public evaluator<vesting_balance_withdraw_evaluator>
{
    public:
        typedef vesting_balance_withdraw_operation operation_type;

        object_id_type do_evaluate( const vesting_balance_withdraw_operation& op );
        object_id_type do_apply( const vesting_balance_withdraw_operation& op );
};

} } // bts::chain
