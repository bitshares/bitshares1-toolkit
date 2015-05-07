#pragma once

#include <bts/chain/evaluator.hpp>

namespace bts { namespace chain {

class bond_create_offer_evaluator : public evaluator<bond_create_offer_evaluator>
{
    public:
        typedef bond_create_offer_operation operation_type;

        object_id_type do_evaluate( const bond_create_offer_operation& op );
        object_id_type do_apply( const bond_create_offer_operation& op );
};

} } // bts::chain
