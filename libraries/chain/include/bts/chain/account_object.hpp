#pragma once
#include <bts/chain/authority.hpp>
#include <bts/chain/asset.hpp>
#include <bts/db/generic_index.hpp>
#include <boost/multi_index/composite_key.hpp>

namespace bts { namespace chain {

   /**
    * @class account_statistics_object
    * @ingroup implementation
    *
    * This object contains regularly updated statistical data about an account. It is provided for the purpose of
    * separating the account data that changes frequently from the account data that is mostly static, which will
    * minimize the amount of data that must be backed up as part of the undo history everytime a transfer is made.
    */
   class account_statistics_object : public bts::db::abstract_object<account_statistics_object>
   {
      public:
         static const uint8_t space_id = implementation_ids;
         static const uint8_t type_id  = impl_account_statistics_object_type;

         /**
          *  When funds are added to the cashback fund they must adjust the average maturity
          *  weighted by the amount of funds and the maturity of those new funds.
          */
         void adjust_cashback( share_type amount, time_point_sec maturity, time_point_sec current_time );

         /**
          * Keep the most recent operation as a root pointer to
          * a linked list of the transaction history. This field is
          * not required by core validation and could in theory be
          * made an annotation on the account object, but because
          * transaction history is so common and this object is already
          * cached in the undo buffer (because it likely affected the
          * balances of this account) it is convienent to simply
          * track this data here.  Account balance objects don't currenty
          * inherit from annotated object.
          */
         account_transaction_history_id_type most_recent_op;

         /**
          *  When calculating votes it is necessary to know how much is
          *  stored in orders (and thus unavailable for transfers).  Rather
          *  than maintaining an index of  [asset,owner,order_id] we will
          *  simply maintain the running total here and update it every
          *  time an order is created or modified.
          */
         share_type            total_core_in_orders;

         /**
          *  Tracks the total fees paid by this account for the purpose
          *  of calculating bulk discounts.
          */
         share_type            lifetime_fees_paid;

         /**
          *  Tracks the total cash back accrued from bulk discounts and
          *  referrals.
          */
         share_type            cashback_rewards;

         /**
          *  Cashback rewards from prime membership upgrades mature
          *  over X months to prevent abuse of the prime membership
          *  program.
          */
         time_point_sec        cashback_maturity;
   };

   class account_balance_object : public abstract_object<account_balance_object>
   {
      public:
         static const uint8_t space_id = implementation_ids;
         static const uint8_t type_id  = impl_account_balance_object_type;

         //Hashed-non-unique index on owner, on asset_type, and hashed-unique on <owner,asset_type>
         account_id_type   owner;
         asset_id_type     asset_type;
         share_type        balance;

         asset get_balance()const { return asset(balance, asset_type); }
         void  adjust_balance(const asset& delta);
   };

   /**
    * @brief This class represents an account on the object graph
    * @ingroup protocol
    *
    * Accounts are the primary unit of authority on the BitShares system. Users must have an account in order to use
    * assets, trade in the markets, vote for delegates, etc.
    */
   class account_object : public bts::db::annotated_object<account_object>
   {
      public:
         static const uint8_t space_id = protocol_ids;
         static const uint8_t type_id  = account_object_type;
         /**
          * The account that paid the fee to register this account, this account is
          * known as the primary referrer and is entitled to a percent of transaction
          * fees.
          */
         account_id_type       registrar;

         /**
          * The registrar may be a faucet with its own revenue sharing model that allows
          * users to refer each other.
          */
         account_id_type       referrer;

         /**
          * Any referral fees not paid to referrer are paid to registrar
          */
         uint8_t               referrer_percent = 0;


         /// The account's name. This name must be unique among all account names on the graph. The name may be empty.
         string                name;

         /**
          * The owner authority represents absolute control over the account. Usually the keys in this authority will
          * be kept in cold storage, as they should not be needed very often and compromise of these keys constitutes
          * complete and irrevocable loss of the account. Generally the only time the owner authority is required is to
          * update the active authority.
          */
         authority             owner;

         /// The owner authority contains the hot keys of the account. This authority has control over nearly all
         /// operations the account may perform.
         authority             active;

         /// The memo key is the key this account will typically use to encrypt/sign transaction memos and other non-
         /// validated account activities. This field is here to prevent confusion if the active authority has zero or
         /// multiple keys in it.
         key_id_type           memo_key;

         /// The voting key may be used to update the account's votes.
         account_id_type       voting_account;


         uint16_t              num_witness = BTS_DEFUALT_NUM_WITNESSES;
         uint16_t              num_committee = BTS_DEFUALT_NUM_COMMITTEE;

         /// This is the list of vote tallies this account votes for. The weight of these votes is determined by this
         /// account's balance of core asset.
         flat_set<vote_tally_id_type> votes;

         /// The reference implementation records the account's statistics in a separate object. This field contains the
         /// ID of that object.
         account_statistics_id_type statistics;

         /**
          * This is a set of all accounts which have 'whitelisted' this account. Whitelisting is only used in core
          * validation for the purpose of authorizing accounts to hold and transact in whitelisted assets. This
          * account cannot update this set, except by transferring ownership of the account, which will clear it. Other
          * accounts may add or remove their IDs from this set.
          */
         flat_set<account_id_type>        whitelisting_accounts;

         /**
          * This is a set of all accounts which have 'blacklisted' this account. Blacklisting is only used in core
          * validation for the purpose of forbidding accounts from holding and transacting in whitelisted assets. This
          * account cannot update this set, and it will be preserved even if the account is transferred. Other accounts
          * may add or remove their IDs from this set.
          */
         flat_set<account_id_type>        blacklisting_accounts;

         /**
          * @return true if this is a prime account, false otherwise.
          */
         bool is_prime()const
         {
            return get_id() == referrer;
         }

         /**
          * @return true if this account is whitelisted and not blacklisted to transact in the provided asset; false
          * otherwise.
          */
         bool is_authorized_asset(const asset_object& asset_obj)const;

         account_id_type get_id()const { return id; }
   };

   /**
    *  This object is attached as the meta annotation on the account object, this information is not relevant to
    *  validation.
    */
   class meta_account_object : public bts::db::abstract_object<meta_account_object>
   {
      public:
         static const uint8_t space_id = implementation_ids;
         static const uint8_t type_id  = meta_account_object_type;

         key_id_type         memo_key;
         delegate_id_type    delegate_id; // optional
   };

   struct by_asset;
   struct by_account;
   struct by_balance;
   typedef multi_index_container<
      account_balance_object,
      indexed_by<
         hashed_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
         //TODO: make these ordered_... indices hashed instead
         ordered_unique< tag<by_balance>, composite_key<
            account_balance_object,
            member<account_balance_object, account_id_type, &account_balance_object::owner>,
            member<account_balance_object, asset_id_type, &account_balance_object::asset_type> >
         >,
         ordered_non_unique< tag<by_account>, member<account_balance_object, account_id_type, &account_balance_object::owner> >,
         ordered_non_unique< tag<by_asset>, member<account_balance_object, asset_id_type, &account_balance_object::asset_type> >
      >
   > account_balance_object_multi_index_type;

   typedef generic_index<account_balance_object, account_balance_object_multi_index_type> account_balance_index;

   struct by_name{};
   typedef multi_index_container<
      account_object,
      indexed_by<
         hashed_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
         hashed_non_unique< tag<by_name>, member<account_object, string, &account_object::name> >
      >
   > account_object_multi_index_type;

   typedef generic_index<account_object, account_object_multi_index_type> account_index;

}}
FC_REFLECT_DERIVED( bts::chain::account_object,
                    (bts::db::annotated_object<bts::chain::account_object>),
                    (registrar)(referrer)(referrer_percent)(name)(owner)(active)(memo_key)(voting_account)(num_witness)(num_committee)(votes)
                    (statistics)(whitelisting_accounts)(blacklisting_accounts) )

FC_REFLECT_DERIVED( bts::chain::account_balance_object,
                    (bts::db::object),
                    (owner)(asset_type)(balance) )

FC_REFLECT_DERIVED( bts::chain::meta_account_object,
                    (bts::db::object),
                    (memo_key)(delegate_id) )

FC_REFLECT_DERIVED( bts::chain::account_statistics_object, (bts::chain::object),
                    (most_recent_op)
                    (total_core_in_orders)
                    (lifetime_fees_paid)
                    (cashback_rewards)
                    (cashback_maturity)
                  )

