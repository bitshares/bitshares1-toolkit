#pragma once
#include <bts/chain/asset.hpp>
#include <bts/db/object.hpp>

namespace bts { namespace chain {
   using namespace bts::db;

   class account_object;

   /**
    *  @brief tracks information about a delegate account.
    *  @ingroup object
    *
    *  A delegate is responsible for setting blockchain parameters and has
    *  dynamic multi-sig control over the genesis account.  The current set of
    *  active delegates has control.
    *
    *  Delegates were separated into a separate object to make iterating over
    *  the set of delegate easy. 
    */
   class delegate_object : public abstract_object<delegate_object>
   {
      public:
         static const uint8_t space_id = protocol_ids;
         static const uint8_t type_id  = delegate_object_type;

         account_id_type                delegate_account;
         vote_id_type                   vote_id;
   };

} } // bts::chain

FC_REFLECT_DERIVED( bts::chain::delegate_object, (bts::db::object),
                    (delegate_account)
                    (vote_id) )
