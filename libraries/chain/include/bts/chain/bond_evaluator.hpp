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

class bond_cancel_offer_evaluator : public evaluator<bond_cancel_offer_evaluator>
{
    public:
        typedef bond_cancel_offer_operation operation_type;

        object_id_type do_evaluate( const bond_cancel_offer_operation& op );
        object_id_type do_apply( const bond_cancel_offer_operation& op );

        const bond_offer_object* _offer = nullptr;
};

class bond_accept_offer_evaluator : public evaluator<bond_accept_offer_evaluator>
{
    public:
        typedef bond_accept_offer_operation operation_type;

        object_id_type do_evaluate( const bond_accept_offer_operation& op );
        object_id_type do_apply( const bond_accept_offer_operation& op );

        const bond_offer_object* _offer = nullptr;
        asset                    _fill_amount;
};

class bond_claim_collateral_evaluator : public evaluator<bond_claim_collateral_evaluator>
{
    public:
        typedef bond_claim_collateral_operation operation_type;

        object_id_type do_evaluate( const bond_claim_collateral_operation& op );
        object_id_type do_apply( const bond_claim_collateral_operation& op );

        const bond_object* _bond = nullptr;
        asset              _interest_due;
};

} } // bts::chain
