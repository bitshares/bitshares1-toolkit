#pragma once

#include <bts/chain/evaluator.hpp>

namespace bts { namespace chain {

class create_bond_offer_evaluator : public evaluator<create_bond_offer_evaluator>
{
    public:
        typedef create_bond_offer_operation operation_type;

        object_id_type do_evaluate( const create_bond_offer_operation& op );
        object_id_type do_apply( const create_bond_offer_operation& op );
};

} } // bts::chain
