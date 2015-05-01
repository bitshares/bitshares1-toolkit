#pragma once
#include <bts/chain/asset.hpp>
#include <bts/db/object.hpp>

namespace bts { namespace chain {
   using namespace bts::db;

   class account_object;

   class witness_object : public abstract_object<witness_object>
   {
      public:
         static const uint8_t space_id = protocol_ids;
         static const uint8_t type_id = witness_object_type;

         account_id_type                witness_account;
         key_id_type                    signing_key;
         secret_hash_type               next_secret;
         secret_hash_type               last_secret;
         share_type                     accumulated_income;
         vote_id_type                   vote_id;

         witness_object() : vote_id(vote_id_type::witness) {}
   };

} } // bts::chain

FC_REFLECT_DERIVED( bts::chain::witness_object, (bts::db::object),
                    (witness_account)
                    (signing_key)
                    (next_secret)
                    (last_secret)
                    (accumulated_income)
                    (vote_id) )

