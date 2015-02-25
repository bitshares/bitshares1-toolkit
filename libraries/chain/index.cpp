#include <bts/chain/index.hpp>
#include <bts/chain/database.hpp>

namespace bts { namespace chain {
   void base_primary_index::save_undo( const object* obj )
   { _db->save_undo( obj ); }

   void base_primary_index::on_add( const object* obj )
   {for( auto ob : _observers ) ob->on_add( obj ); }

   void base_primary_index::on_remove( object_id_type id )
   {for( auto ob : _observers ) ob->on_remove( id ); }

   void base_primary_index::on_modify( object_id_type id, const object* obj )
   {for( auto ob : _observers ) ob->on_modify( obj->id, obj ); }
} } // bts::chain
