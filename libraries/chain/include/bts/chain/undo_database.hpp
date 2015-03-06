#pragma once
#include <bts/chain/object.hpp>

namespace bts { namespace chain {

   class database;

   class undo_database
   {
      public:
         undo_database( database& db ):_db(db){};

         class session 
         {
            public:
               ~session()    { if( _apply_undo ) _db.undo(); }
               void commit() { _apply_undo = false;   }
               void undo()   { _db.undo(); commit();  }
               void merge()  { _db.merge(); commit(); }

            private:
               friend undo_database;
               session(undo_database& db);
               undo_database& _db;
               bool _apply_undo = true;
         };

         void    disable();
         void    enable();

         session start_undo_session();
         void create( const object* obj );

         template<typename T>
         void modify( const T* obj )
         {
            if( _disabled ) return;
            auto& state = _stack.back();
            auto itr =  state.old_values.find(obj->id);
            if( itr != state.old_values.end() ) return;
            state.old_values[obj->id].reset( new T(*obj) );
         }

         template<typename T>
         void remove( const T* obj )
         {
            if( _disabled ) return;
            auto& state = _stack.back();
            auto itr =  state.removed.find(obj->id);
            if( itr != state.removed.end() ) return;
            state.removed[obj->id].reset( new T(*obj) );
         }

      private:
         void undo(); 
         void merge();

         struct undo_state
         {
            unordered_map<object_id_type, unique_ptr<object> > old_values;
            set<object_id_type>                                new_ids;
            unordered_map<object_id_type, unique_ptr<object> > removed;
         };


         bool                   _disabled = false;
         std::deque<undo_state> _stack;
         database&              _db;
   };

} }
