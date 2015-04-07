#pragma once
#include <bts/chain/object.hpp>

namespace bts { namespace chain {

   /**
    *  All accounts may optionally upgrade to a referrer account which
    *  means they will get a percentage of all transaction fees generated 
    *  by all future accounts they "create" aka "sign up".   When a account
    *  then creates additional accounts the referrer property is inherited 
    *  which means that signing up one person who then creates N accounts gets
    *  your account credit for all N accounts.  
    *
    *  There is a fee required to become your own referrer that is equal to
    *  the present value of the expected future fees generated from your
    *  account.   Anyone who is an "above average" user should sign up to
    *  be a referrer so they can get a "life time discount" on all their
    *  future activity as well as activity of anyone they refer.
    *
    *  When a new account is created it must have the same referral object
    *  as the account that paid the registration fee *unless* the account
    *  that paid the fee is the owner of his own referral fee.
    */
   class referrer_object : public abstract_object<referrer_object>
   {
      public:
         account_id_type owner;
         share_type      accumulated_balance;
   };

} }

FC_REFLECT_DERIVED( bts::chain::referrer_object, (bts::chain::object), (owner)(accumulated_balance) )
