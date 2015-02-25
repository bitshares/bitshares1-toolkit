#include <bts/chain/database.hpp>
#include <fc/io/raw.hpp>
#include <fc/container/flat.hpp>

#include <bts/chain/key_object.hpp>
#include <bts/chain/account_object.hpp>
#include <bts/chain/asset_object.hpp>
#include <bts/chain/delegate_object.hpp>
#include <bts/chain/operation_factory.hpp>
#include <bts/chain/simple_index.hpp>
#include <bts/chain/account_index.hpp>

namespace bts { namespace chain {

database::database()
{
   _index.resize(255);
   _index[protocal_ids].resize( 10 );
   _index[implementation_ids].resize( 10 );
   _index[meta_info_ids].resize( 10 );

   add_index<primary_index<simple_index<key_object>> >();
   add_index<primary_index<account_index> >();
   //add_index<primary_index<asset_index> >();
}

database::~database(){}

void database::close()
{
   flush();

   _block_num_to_block.close();
   _block_id_to_num.close();
   _object_id_to_object.close();
}

const object* database::get_object( object_id_type id )const
{
   return get_index(id.space(),id.type()).get( id );
}

const index& database::get_index(uint8_t space_id, uint8_t type_id)const
{
   FC_ASSERT( _index.size() > space_id );
   FC_ASSERT( _index[space_id].size() > type_id );
   const auto& tmp = _index[space_id][type_id];
   FC_ASSERT( tmp );
   return *tmp;
}
index& database::get_index(uint8_t space_id, uint8_t type_id)
{
   FC_ASSERT( _index.size() > space_id );
   FC_ASSERT( _index[space_id].size() > type_id );
   const auto& tmp = _index[space_id][type_id];
   FC_ASSERT( tmp );
   return *tmp;
}

const account_index& database::get_account_index()const
{
   return dynamic_cast<const account_index&>( get_index<account_object>() ); 
}
account_index& database::get_account_index()
{
   return dynamic_cast<account_index&>( get_index<account_object>() ); 
}

const asset_index&   database::get_asset_index()const
{
}
asset_index&   database::get_asset_index()
{
}


void database::flush()
{

}

void database::open( const fc::path& data_dir )
{ try {
   init_genesis();
   _block_num_to_block.open( data_dir / "database" / "block_num_to_block" );
   _block_id_to_num.open( data_dir / "database" / "block_id_to_num" );
   _object_id_to_object.open( data_dir / "database" / "objects" );

   for( auto& space : _index )
   {
      for( auto& type_index : space )
      {
         if( type_index )
         {
            type_index->open( _object_id_to_object );
         }
      }
   }
} FC_CAPTURE_AND_RETHROW( (data_dir) ) }

void database::init_genesis()
{
}


asset database::current_delegate_registration_fee()const
{
   return asset();
}

void database::save_undo( const object* obj )
{
   FC_ASSERT( obj );
   auto id = obj->id;
   auto current_undo = _undo_state.back().old_values.find(id);
   if( current_undo == _undo_state.back().old_values.end() )
   {
      _undo_state.back().old_values[id] = get_index(obj->space(),obj->type()).pack( obj ); 
   }
}


void database::pop_undo_state()
{
   _undo_state.pop_back();
}
void database::push_undo_state()
{
   _undo_state.push_back( undo_state() );
}
void database::undo()
{
   for( auto item : _undo_state.back().old_values )
   {
      if( item.second.is_null() )
      {
         //get_object_ptr(item.first).reset();
      }
      else
      {
         /*
         auto obj = get_object_ptr(item.first).get();
         FC_ASSERT( obj );
         // IF NO INDEXES THEN WE DO THIS... ELSE WE CANNOT MODIFY OBJECT...
         get_object_builder(obj->space(),obj->type())->unpack( obj, item.second );
         */
      }
   }
   //for( const auto& old_nxt_id : _undo_state.back().old_next_object_ids )
   //   _next_object_ids[old_nxt_id.first.first][old_nxt_id.first.second] = old_nxt_id.second;
   _undo_state.pop_back();
}


void database::push_block( const block& new_block )
{ try {
   pop_pending_block();
   { // logically connect pop/push of pending block
      push_undo_state();
      optional<fc::exception> except;
      try {
         for( const auto& trx : new_block.transactions )
         {
            /* We do not need to push the undo state for each transaction
             * because they either all apply and are valid or the
             * entire block fails to apply.  We only need an "undo" state
             * for transactions when validating broadcast transactions or
             * when building a block.
             */
            apply_transaction( trx ); 
         }
      } 
      catch ( const fc::exception& e ) { except = e; }
      if( except ) 
      {
         undo();
         throw *except;
      }
   }
   push_pending_block();
} FC_CAPTURE_AND_RETHROW( (new_block) ) }

/**
 *  Attempts to push the transaction into the pending queue
 */
bool database::push_transaction( const signed_transaction& trx )
{
   push_undo_state(); // trx undo 
   optional<fc::exception> except;
   try {
      apply_transaction( trx );
      _pending_block.transactions.push_back(trx);
      pop_undo_state(); // everything was OK.
      return true;
   } catch ( const fc::exception& e ) { except = e; }
   if( except )
   {
      undo();
   }
   return false;
}
void database::push_pending_block()
{
   block old_pending = std::move( _pending_block );
   _pending_block.transactions.clear();
   push_undo_state();
   for( auto trx : old_pending.transactions )
   {
      push_transaction( trx );
   }
}
void database::pop_pending_block()
{
   undo();
}


processed_transaction database::apply_transaction( const signed_transaction& trx )
{ try {
   transaction_evaluation_state eval_state(this);
   processed_transaction ptrx(trx);
   for( auto op : ptrx.operations )
   {
      auto r = operation_factory::instance().evaluate( eval_state, op );
      ptrx.operation_results.push_back(r);
   }
   return ptrx;
} FC_CAPTURE_AND_RETHROW( (trx) ) }


} } // namespace bts::chain
