#pragma once

#pragma once
#include <bts/chain/authority.hpp>
#include <bts/chain/asset.hpp>
#include <bts/db/generic_index.hpp>

namespace bts { namespace chain {

  /**
   *  @ingroup object
   */
  class bond_object : public bts::db::abstract_object<bond_object>
  {
     public:
        static const uint8_t space_id = protocol_ids;
        static const uint8_t type_id  = bond_object_type;

        asset_id_type collateral_type()const { return collateral.asset_id; }

        account_id_type   borrower;
        account_id_type   lender;
        asset             borrowed;
        /** if collateral is the core asset, then voting rights belong to the borrower 
         * because the borrower is owner of the collateral until they default
         */
        asset             collateral;
        uint16_t          interest_apr = 0;
        time_point_sec    start_date;
        /** after this date the lender can collect the collateral at will or let it float */
        time_point_sec    due_date;
        /** the loan cannot be paid off before this date */
        time_point_sec    earliest_payoff_date;
  };

  /**
   *  @ingroup object
   */
  class bond_offer_object : public bts::db::abstract_object<bond_offer_object>
  {
     public:
        static const uint8_t space_id = protocol_ids;
        static const uint8_t type_id  = bond_offer_object_type;

        asset_id_type asset_type()const { return amount.asset_id; }

        account_id_type offered_by_account;
        bool            offer_to_borrow = false; // Offer to borrow if true, and offer to lend otherwise
        asset           amount;
        price           collateral_rate;
        uint32_t        min_loan_period_sec = 0;
        uint32_t        loan_period_sec = 0;
        uint16_t        interest_apr    = 0;
  };

  struct by_borrower;
  struct by_lender;
  struct by_offerer;
  struct by_collateral; /// needed for blackswan resolution
  struct by_asset; /// needed for blackswan resolution

  typedef multi_index_container<
     bond_object,
     indexed_by<
        hashed_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
        ordered_non_unique< tag<by_borrower>, member<bond_object, account_id_type, &bond_object::borrower> >,
        ordered_non_unique< tag<by_lender>, member<bond_object, account_id_type, &bond_object::lender> >,
        hashed_non_unique< tag<by_collateral>, const_mem_fun<bond_object, asset_id_type, &bond_object::collateral_type> >
     >
  > bond_object_multi_index_type;

  typedef generic_index<bond_object, bond_object_multi_index_type> bond_index;

  /**
   *  Todo: consider adding index of tuple<collateral_type,loan_asset_type,interest_rate>
   *  Todo: consider adding index of tuple<collateral_type,loan_asset_type,period>
   */
  typedef multi_index_container<
     bond_offer_object,
     indexed_by<
        hashed_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
        ordered_non_unique< tag<by_offerer>, member<bond_offer_object, account_id_type, &bond_offer_object::offered_by_account> >,
        hashed_non_unique< tag<by_asset>, const_mem_fun<bond_offer_object, asset_id_type, &bond_offer_object::asset_type> >
     >
  > bond_offer_object_multi_index_type;

  typedef generic_index<bond_offer_object, bond_offer_object_multi_index_type> bond_offer_index;

}} // bts::chain

FC_REFLECT_DERIVED( bts::chain::bond_object,       (bts::db::object), 
                    (borrower)(lender)(borrowed)(collateral)(interest_apr)(start_date)(due_date)(earliest_payoff_date) )
FC_REFLECT_DERIVED( bts::chain::bond_offer_object, (bts::db::object), (offered_by_account)(offer_to_borrow)(amount)(collateral_rate)(min_loan_period_sec)(loan_period_sec)(interest_apr) )
