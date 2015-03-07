#include <bts/chain/database.hpp>
#include <bts/chain/undo_database.hpp>

namespace bts { namespace chain {

undo_database::session undo_database::start_undo_session()
{
   if( _disabled ) return session(*this);
   _stack.emplace_back( undo_state() );
   return session(*this);
}
void undo_database::on_create( const object& obj )
{
   if( _disabled ) return;
   auto& state = _stack.back();
   state.new_ids.insert(obj.id);
}
void undo_database::on_modify( const object& obj )
{
   if( _disabled ) return;
   auto& state = _stack.back();
   auto itr =  state.old_values.find(obj.id);
   if( itr != state.old_values.end() ) return;
   state.old_values[obj.id] = obj.clone();
}
void undo_database::on_remove( const object& obj )
{
   if( _disabled ) return;
   auto& state = _stack.back();
   auto itr =  state.removed.find(obj.id);
   if( itr != state.removed.end() ) return;
   state.removed[obj.id] = obj.clone();
}

void undo_database::undo()
{
   FC_ASSERT( !_disabled );
   disable();

   auto& state = _stack.back();
   for( auto& item : state.old_values )
      _db.modify( _db.get_object( item.second->id ), [&]( object& obj ){ obj.move_from( *item.second ); } );
   
   for( auto ritr = state.new_ids.rbegin(); ritr != state.new_ids.rend(); ++ritr  )
      _db.remove( _db.get_object(*ritr) );

   for( auto& item : state.removed )
      _db.insert( std::move(*item.second) );

   enable();
}

void undo_database::merge()
{
   FC_ASSERT( _stack.size() >=2 );
   auto& state = _stack.back();
   auto& prev_state = _stack[_stack.size()-2];
   for( auto& obj : state.old_values )
      prev_state.old_values[obj.second->id] = std::move(obj.second);
   for( auto id : state.new_ids )
      prev_state.new_ids.insert(id);
   for( auto& obj : state.removed )
      prev_state.removed[obj.second->id] = std::move(obj.second);
}

} } // bts::chain
