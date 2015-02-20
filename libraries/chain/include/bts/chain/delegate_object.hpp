#pragma once
#include <bts/chain/object.hpp>

namespace bts { namespace chain { 
   class account_object;

   class delegate_object : public object
   {
      public:
         static const object_type type = delegate_object_type;

         delegate_object():object(delegate_object_type){}

         account_id_type                delegate_account;
         public_key_type                signing_key;
         secret_hash_type               previous_secret;
         vector<asset>                  fee_schedule;
         uint16_t                       pay_rate = 0;
         object_id_type                 vote;
   };

   /**
    *  Delegate votes are updated all the time which means that an "undo" state
    *  must be generated for every delegate.  Undo states require serializing 
    *  and saving a copy of a complete object.  delegate_object is easily 250 bytes
    *  and there are potentially 220 delegates that require updating with
    *  every transaction.  This would generate 53KB of serialized backup state
    *  per processed transaction.  
    *
    *  To get around this delegate votes are moved into their own object type that
    *  has less than 32 bytes of backup state or at most 7 KB.  The penalty is
    *  a double-lookup of the object on every vote...
    */
   class delegate_vote_object : public object 
   {
      public:
         static const object_type type = impl_delegate_vote_object_type;

         delegate_vote_object():object(type){}

         share_type                     total_votes;
   };
} } // bts::chain

FC_REFLECT_DERIVED( bts::chain::delegate_object, (bts::chain::object), (delegate_account)(signing_key)(previous_secret)(fee_schedule)(vote) )
FC_REFLECT_DERIVED( bts::chain::delegate_vote_object, (bts::chain::object), (total_votes) )
