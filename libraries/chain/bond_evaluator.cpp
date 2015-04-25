#include <bts/chain/account_object.hpp>
#include <bts/chain/bond_evaluator.hpp>
#include <bts/chain/bond_object.hpp>
#include <bts/chain/database.hpp>

namespace bts { namespace chain {

object_id_type create_bond_offer_evaluator::do_evaluate( const create_bond_offer_operation& op )
{
    const auto& d = db();

    const auto& creator_account = op.creator( d );
    const auto& base_asset = op.collateral_rate.base.asset_id( d );
    const auto& quote_asset = op.collateral_rate.quote.asset_id( d );

    // TODO: Check asset authorizations and withdrawals

    const auto& amount_asset = (op.amount.asset_id == op.collateral_rate.base.asset_id) ? base_asset : quote_asset;

    FC_ASSERT( get_balance( creator_account, amount_asset ) >= op.amount );

    return object_id_type();
}

object_id_type create_bond_offer_evaluator::do_apply( const create_bond_offer_operation& op )
{
    adjust_balance( op.creator, -op.amount );

    const auto& offer = db().create<bond_offer_object>( [&]( bond_offer_object& obj )
    {
        obj.offered_by_account = op.creator;
        obj.offer_to_borrow = op.offer_to_borrow;
        obj.amount = op.amount;
        obj.collateral_rate = op.collateral_rate;
        obj.min_loan_period_sec = op.min_loan_period_sec;
        obj.loan_period_sec = op.loan_period_sec;
        obj.interest_apr = op.interest_apr;
    } );

    return offer.id;
}

} } // bts::chain
