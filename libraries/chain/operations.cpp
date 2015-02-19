#include <bts/chain/database.hpp>
#include <bts/chain/asset_operations.hpp>
#include <bts/chain/account_object.hpp>
#include <bts/chain/asset_object.hpp>
#include <bts/chain/balance_object.hpp>
#include <bts/chain/delegate_object.hpp>
#include <bts/chain/exceptions.hpp>
#include <fc/utf8.hpp>

#include <algorithm>

namespace bts { namespace chain {
bool asset_object::is_valid_symbol( const string& symbol )
{
   if( symbol.size() > 17 ) return false;
   if( symbol.size() < 3  ) return false;
   int dot_count = 0;
   for( auto c : symbol )
   {
      if( c == '.' ) ++dot_count;
      else if( c < 'A' || c > 'Z' ) return false;
   }
   if( symbol[0] == '.' || symbol[symbol.size()-1] == '.' ) 
      return false;
   return dot_count <= 1;
}

void  account_object::authorize_asset( asset_id_type asset_id, bool state )
{
   auto& vec = authorized_assets;

   auto lb = std::lower_bound( vec.begin(), vec.end(), asset_id );
   if( !state ) FC_ASSERT( lb != vec.end() && *lb == asset_id );
   else FC_ASSERT( lb == vec.end() );

   if( state ) vec.insert( lb, asset_id );
   else vec.erase( lb );
}

bool  account_object::is_authorized_asset( asset_id_type asset_id )const
{
   return std::binary_search( authorized_assets.begin(), authorized_assets.end(), asset_id );
}

void account_balance_object::add_balance( const asset& a )
{
   FC_ASSERT( a.amount > 0 );
   auto& vec = balances;
   auto lb = std::lower_bound( vec.begin(), vec.end(), std::make_pair(a.asset_id,share_type(0)) );
   if( lb != vec.end() && lb->first == a.asset_id ) lb->second += a.amount;
   else
   {
      vec.insert( lb, std::make_pair( a.asset_id, a.amount ) );
   }
}
asset account_balance_object::get_balance( asset_id_type what )const
{
   auto& vec = balances;
   auto lb = std::lower_bound( vec.begin(), vec.end(), std::make_pair(what,share_type(0)) );
   if( lb != vec.end() && lb->first == what ) return asset{lb->second,what};
   return asset{0,what};
}

void account_balance_object::sub_balance( const asset& a )
{
   FC_ASSERT( a.amount > 0 );
   auto& vec = balances;
   auto lb = std::lower_bound( vec.begin(), vec.end(), std::make_pair(a.asset_id,share_type(0)) );
   if( lb != vec.end() && lb->first == a.asset_id ) lb->second -= a.amount;
   else
   {
      FC_ASSERT( false, "No current Balance for Asset" );
   }
}

object_id_type create_account_operation::evaluate( transaction_evaluation_state& eval_state )
{ try {
    database& db = eval_state.db();

    shared_ptr<const account_object> current_account = db.lookup_account( this->name );
    FC_ASSERT( !current_account );

    shared_ptr<account_object> new_account = db.create<account_object>();
    new_account->name = this->name;
    new_account->owner = this->owner;
    new_account->active = this->active;
    new_account->voting = this->voting;

    shared_ptr<account_balance_object> balance_obj = db.create<account_balance_object>();
    new_account->balances = balance_obj->object_id();

    db.index_account( new_account );

    return new_account->object_id();
} FC_CAPTURE_AND_RETHROW( (*this) ) }


object_id_type create_asset_operation::evaluate( transaction_evaluation_state& eval_state )
{ try {
   database& db = eval_state.db();

   FC_ASSERT( asset_object::is_valid_symbol( this->symbol ) );
   FC_ASSERT( fc::is_utf8( this->description ) );
   FC_ASSERT( fc::is_utf8( this->name ) );
   FC_ASSERT( this->name.size() < BTS_MAX_ASSET_NAME_LENGTH );


   shared_ptr<const asset_object> current_asset = db.lookup_symbol( this->symbol );
   FC_ASSERT( !current_asset );

   shared_ptr<asset_object> new_asset = db.create<asset_object>();
   new_asset->symbol        = this->symbol;
   new_asset->name          = this->name;
   new_asset->description   = this->description;
   new_asset->max_supply    = this->max_supply;

   new_asset->issuer        = this->issuer;
   
   shared_ptr<const account_object> issuer_account = db.get<account_object>( this->issuer ); 
   FC_ASSERT( issuer_account );

   if( !eval_state.check_authority( issuer_account->active )  )
      FC_CAPTURE_AND_THROW( missing_signature, (issuer_account->active) );

   db.index_symbol( new_asset );

   return new_asset->object_id();
} FC_CAPTURE_AND_RETHROW( (*this) ) }

object_id_type register_delegate_operation::evaluate( transaction_evaluation_state& eval_state )
{ try {
   database& db = eval_state.db();

   FC_ASSERT( this->pay_rate <= BTS_MAX_PAY_RATE );
   FC_ASSERT( this->delegate_registration_fee == db.current_delegate_registration_fee() );
   eval_state.withdraw_from_account( this->delegate_account, this->delegate_registration_fee );

   shared_ptr<account_object> del_account = db.get_mutable<account_object>(this->delegate_account);
   FC_ASSERT( del_account );
   FC_ASSERT( del_account->delegate_id == 0 );


   shared_ptr<delegate_object> del_obj = db.create<delegate_object>();
   del_obj->delegate_account  = del_account->object_id();
   del_obj->signing_key       = this->block_signing_key;
   del_obj->pay_rate          = this->pay_rate;
   del_obj->previous_secret   = this->first_secret;
   del_obj->fee_schedule      = this->fee_schedule;
   del_obj->vote              = db.create<delegate_vote_object>()->object_id();

   del_account->delegate_id = del_obj->object_id();

   return del_account->delegate_id;

} FC_CAPTURE_AND_RETHROW( (*this) ) }


object_id_type update_asset_operation::evaluate( transaction_evaluation_state& eval_state )
{
   return 0;
}

object_id_type update_asset_white_list_operation::evaluate( transaction_evaluation_state& eval_state )
{ try {
   database& db = eval_state.db();
   auto asset_obj      = db.get<asset_object>( this->asset_id );
   FC_ASSERT( asset_obj );
   auto issuer_account = db.get<account_object>( asset_obj->issuer );
   FC_ASSERT( issuer_account );

   auto user_account = db.get_mutable<account_object>( this->account_id );
   FC_ASSERT( user_account );

   if( !eval_state.check_authority( issuer_account->active )  )
      FC_CAPTURE_AND_THROW( missing_signature, (issuer_account->active) );

   user_account->authorize_asset( this->asset_id, this->authorize );

   return 0;
} FC_CAPTURE_AND_RETHROW( (*this) ) }

object_id_type issue_asset_operation::evaluate( transaction_evaluation_state& eval_state )
{ try {
   database& db = eval_state.db();
   FC_ASSERT( this->amount_to_issue.amount > 0 );

   auto asset_obj      = db.get_mutable<asset_object>( this->amount_to_issue.asset_id );
   FC_ASSERT( asset_obj );

   auto issuer_account = db.get<account_object>( asset_obj->issuer );
   FC_ASSERT( issuer_account );

   auto to_account_obj = db.get<account_object>( this->to_account );
   FC_ASSERT( to_account_obj );

   if( !eval_state.check_authority( issuer_account->active )  )
      FC_CAPTURE_AND_THROW( missing_signature, (issuer_account->active) );

   if( asset_obj->enforce_white_list() )
      FC_ASSERT( to_account_obj->is_authorized_asset( asset_obj->object_id() ) );

   asset_obj->issue( this->amount_to_issue.amount );

   auto balances = db.get_mutable<account_balance_object>(to_account_obj->balances);
   FC_ASSERT( balances );
   balances->add_balance( this->amount_to_issue );
   
   return 0;
} FC_CAPTURE_AND_RETHROW( (*this) ) }


object_id_type transfer_asset_operation::evaluate( transaction_evaluation_state& eval_state )
{ try {
   object_id_type result = 0;

   database& db = eval_state.db();
   FC_ASSERT( amount.amount > 0 );

   auto asset_obj = db.get<asset_object>( this->amount.asset_id );
   auto amount_with_fee = this->amount;
   amount_with_fee.amount += this->transfer_fee;
   FC_ASSERT( this->transfer_fee == asset_obj->transfer_fee );

   auto from_account = db.get<account_object>( this->from );
   if( from_account )
   {
      eval_state.withdraw_from_account( this->from, amount_with_fee );
   }
   else 
   {
      auto from_balance = db.get_mutable<balance_object>( this->from );
      FC_ASSERT( from_balance );
      if( !eval_state.check_authority( from_balance->owner )  )
         FC_CAPTURE_AND_THROW( missing_signature, (from_balance->owner) );
      FC_ASSERT( from_balance->balance >= amount_with_fee );
      from_balance->balance -= amount_with_fee;
   }

   if( this->to_authority ) 
   {
      FC_ASSERT( this->to == 0 );
      auto balance_obj = db.create<balance_object>();
      balance_obj->owner = *to_authority;
      balance_obj->balance = this->amount;
      result = balance_obj->object_id();
   }
   else
   {
      auto to_account = db.get<account_object>( this->to ); FC_ASSERT( to_account );
      if( to_account )
         eval_state.deposit_to_account( this->to, this->amount );
      else
      {
         auto  to_balance = db.get_mutable<balance_object>( this->to );
         FC_ASSERT(to_balance);
         FC_ASSERT(to_balance->balance.asset_id == this->amount.asset_id);
         to_balance->balance += this->amount;
      }
   }


   return result;
} FC_CAPTURE_AND_RETHROW( (*this) ) }

} } // namespace bts::chain
