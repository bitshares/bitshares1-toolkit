#pragma once
#include <bts/chain/operations.hpp>
#include <bts/chain/authority.hpp>

namespace bts { namespace chain { 
   class database;

   /**
    *  Place holder for state tracked while processing a
    *  transaction.  
    */
   class transaction_evaluation_state
   {
      public:
         bool check_authority( const authority& auth )const
         {
            if( _skip_signature_check ) return true;
            return check_authority( address_authority(auth) );
         }
         bool check_authority( const address_authority& auth )const;
         void adjust_votes( const vector<delegate_id_type>& delegates, share_type amount );

         void withdraw_from_account( account_id_type, const asset& what );
         void deposit_to_account( account_id_type, const asset& what );

         transaction_evaluation_state( database* db = nullptr, bool skip_sig_check = false )
         :_db(db),_skip_signature_check(skip_sig_check){};

         database& db()const { FC_ASSERT( _db ); return *_db; }

         set<address> signed_by;
         database*    _db = nullptr;
         bool         _skip_signature_check = false;
   };
} } // namespace bts::chain 
