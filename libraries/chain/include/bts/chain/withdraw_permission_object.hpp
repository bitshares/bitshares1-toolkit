#pragma once
#include <bts/chain/authority.hpp>
#include <bts/chain/asset.hpp>
#include <bts/db/generic_index.hpp>

namespace bts { namespace chain {

  /**
   *  @class withdraw_permission_object
   *  @brief grants another account authority to withdraw a limited amount of funds per period
   *
   *  The primary purpose of this object is to enable recurring payments on the blockchain.
   */
  class withdraw_permission_object : public bts::db::abstract_object<withdraw_permission_object>
  {
     public:
        static const uint8_t space_id = protocol_ids;
        static const uint8_t type_id  = withdraw_permission_object_type;

        account_id_type    withdraw_from_account;
        account_id_type    authorized_account;
        asset              withdraw_limit;
        uint32_t           period_sec;
        /**
         *  The maximum number of withdraws authorized
         */
        uint32_t           recurring;
        /**
         *  Tracks the start of the first withdraw period
         */
        time_point_sec     starting_time;

        /**
         *  Tracks the last time funds were withdrawn
         */
        time_point_sec     last_withdraw_time;
  };

   struct by_from{};
   struct by_authorized{};

   /// TODO: implement boost::hash for account_id_type and switch ot hashed_non_unique 
   typedef multi_index_container<
      withdraw_permission_object,
      indexed_by<
         hashed_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
         ordered_non_unique< tag<by_from>, member<withdraw_permission_object, account_id_type, &withdraw_permission_object::withdraw_from_account> >,
         ordered_non_unique< tag<by_authorized>, member<withdraw_permission_object, account_id_type, &withdraw_permission_object::authorized_account> >
      >
   > withdraw_permission_object_multi_index_type;

   typedef generic_index<withdraw_permission_object, withdraw_permission_object_multi_index_type> withdraw_permission_index;


} } // bts::chain

FC_REFLECT_DERIVED( bts::chain::withdraw_permission_object, (bts::db::object), 
                   (withdraw_from_account)
                   (authorized_account)
                   (withdraw_limit)
                   (period_sec)
                   (recurring)
                   (starting_time)
                   (last_withdraw_time) )
