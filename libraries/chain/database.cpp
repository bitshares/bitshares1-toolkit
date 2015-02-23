#include <bts/chain/database.hpp>
#include <fc/io/raw.hpp>
#include <fc/container/flat.hpp>

#include <bts/chain/key_object.hpp>
#include <bts/chain/account_object.hpp>
#include <bts/chain/asset_object.hpp>
#include <bts/chain/delegate_object.hpp>
#include <bts/chain/operation_factory.hpp>

namespace bts { namespace chain {

database::database()
:_object_factory(3),_next_object_ids(3,0)
{
  register_object<key_object>();
  register_object<account_object>();
  register_object<asset_object>();
  register_object<delegate_object>();
}

database::~database(){}

void database::close()
{
   flush();

   _block_num_to_block.close();
   _block_id_to_num.close();
   _object_id_to_object.close();
}
void database::flush()
{
   for( const auto& space : _loaded_objects )
   {
      for( const auto& item : space )
      {
         if( item && item->is_dirty() )
            _object_id_to_object.store( item->object_id(), get_object_builder(item->type)->pack( item.get() ) );
      }
   }
}

void database::open( const fc::path& data_dir )
{ try {
   init_genesis();
   _block_num_to_block.open( data_dir / "database" / "block_num_to_block" );
   _block_id_to_num.open( data_dir / "database" / "block_id_to_num" );
   _object_id_to_object.open( data_dir / "database" / "objects" );

   auto itr = _object_id_to_object.begin();
   while( itr.valid() )
   {
      const auto& obj = itr.value();
      auto obj_ptr    = load_object( itr.value() );
      auto ptr = obj_ptr.get();
      auto id = obj_ptr->object_id();
      /* TODO: properly index this
      if( id >= _loaded_objects.size() )
         _loaded_objects.resize( id + 1 );
      _loaded_objects[id] = std::move(obj_ptr);
      */

      switch( ptr->type.value )
      {
         case account_object_type:
         {
            auto account_ptr = dynamic_cast<const account_object*>(ptr);
            FC_ASSERT( account_ptr );
            _account_index[account_ptr->name] = account_ptr->object_id();
            break;
         }
         case asset_object_type:
         {
            auto asset_ptr = dynamic_cast<const asset_object*>(ptr);
            FC_ASSERT( asset_ptr );
            _symbol_index[asset_ptr->symbol] = asset_ptr->object_id();
            break;
         }
         case delegate_object_type:
         {
            _delegates.push_back( ptr->object_id() );
            break;
         }
         default: // do nothing
            break;
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


object_builder* database::get_object_builder( uint16_t type )const
{
   auto id_space = type >> 8;
   auto type_in_space = type & 0x00ff;
   return _object_factory[id_space][type_in_space].get();
}
void database::save_undo( object* obj )
{
   FC_ASSERT( obj );
   auto id = obj->id;
   auto current_undo = _undo_state.back().old_values.find(id);
   if( current_undo == _undo_state.back().old_values.end() )
   {
      obj->mark_dirty();
      _undo_state.back().old_values[id] = get_object_builder(obj->type)->pack( obj ); //obj->pack();
   }
}
void database::pop_undo_state()
{
   _undo_state.pop_back();
}
void database::push_undo_state()
{
   _undo_state.push_back( undo_state() );
   _undo_state.back().old_next_object_ids = _next_object_ids;
}
void database::undo()
{
   for( auto item : _undo_state.back().old_values )
   {
      if( item.second.type == null_object_type )
      {
         get_object_ptr(item.first).reset();
      }
      else
      {
         auto obj = get_object_ptr(item.first).get();
         FC_ASSERT( obj );
         get_object_builder(obj->type)->unpack( obj, item.second );
         // obj->unpack( item.second );
      }
   }
   for( auto item : _undo_state.back().old_account_index )
   {
      if( item.second != 0 ) 
      { 
         _account_index[item.first] = item.second; 
      }
      else
      {
         _account_index.erase( item.first );
      }
   }
   for( auto item : _undo_state.back().old_symbol_index )
   {
      if( item.second != 0 ) 
      { 
         _symbol_index[item.first] = item.second; 
      }
      else
      {
         _symbol_index.erase( item.first );
      }
   }
   _next_object_ids = _undo_state.back().old_next_object_ids;
   _undo_state.pop_back();
}

void database::index_account( account_object* a )
{
   auto name    = a->name;
   auto cur_itr = _account_index.find(name);
   if( cur_itr != _account_index.end() )
   {
      _undo_state.back().old_account_index[name] = cur_itr->second;
   }
   else
   {
      _undo_state.back().old_account_index[name] = 0;
   }
   _account_index[name] = a->object_id();
}
void database::index_symbol( asset_object* a )
{
   auto name    = a->symbol;
   auto cur_itr = _symbol_index.find(name);
   if( cur_itr != _symbol_index.end() )
   {
      _undo_state.back().old_symbol_index[name] = cur_itr->second;
   }
   else
   {
      _undo_state.back().old_symbol_index[name] = 0;
   }
   _symbol_index[name] = a->object_id();
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


const account_object* database::lookup_account( const string& name )const
{
   auto itr = _account_index.find( name );
   if( itr == _account_index.end() )
      return nullptr;
   return get<account_object>(itr->second);
}

const asset_object* database::lookup_symbol( const string& name )const
{
   auto itr = _symbol_index.find( name );
   if( itr == _symbol_index.end() )
      return nullptr;
   return get<asset_object>(itr->second);
}

unique_ptr<object>  database::load_object( const packed_object& obj )
{ try {
   const auto& builder = get_object_builder( obj.type );
   FC_ASSERT( builder );
   auto built_obj = builder->create();
   builder->unpack( built_obj.get(), obj );
   return built_obj;
} FC_CAPTURE_AND_RETHROW( (obj) ) }

} } // namespace bts::chain
