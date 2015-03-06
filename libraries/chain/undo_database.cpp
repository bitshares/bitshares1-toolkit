#include <bts/chain/database.hpp>
#include <bts/chain/undo_database.hpp>

namespace bts { namespace chain {

undo_database::session undo_database::start_undo_session()
{
   if( _disabled ) return session(*this);
   _stack.emplace_back( undo_state() );
   return session(*this);
}
void undo_database::create( const object* obj )
{
   if( _disabled ) return;
   auto& state = _stack.back();
   state.new_ids.insert(obj->id);
}

void undo_database::undo()
{
   FC_ASSERT( !_disabled );
   disable();

   auto& state = _stack.back();
   for( auto& item : state.old_values )
   {
      index& idx = _db.get_index( item.second->id.space(), item.second->id.type() );
      idx.replace( std::move( item.second ) );
   }
   auto ritr = state.new_ids.rbegin();
   while( ritr != state.new_ids.rend() )
   {
      index& idx = _db.get_index( ritr->space(), ritr->type() );
      idx.remove( *ritr );
   }
   for( auto& item : state.removed )
   {
      index& idx = _db.get_index( item.second->id.space(), item.second->id.type() );
      idx.replace( std::move( item.second ) );
   }
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
