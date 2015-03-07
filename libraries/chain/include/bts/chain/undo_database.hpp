#pragma once
#include <bts/chain/object.hpp>

namespace bts { namespace chain {

   class database;

   class undo_database
   {
      public:
         undo_database( database& db ):_db(db){}

         class session
         {
            public:
               ~session()    { if( _apply_undo ) _db.undo(); }
               void commit() { _apply_undo = false;   }
               void undo()   { if( _apply_undo ) _db.undo(); commit();  }
               void merge()  { if( _apply_undo ) _db.merge(); commit(); }

            private:
               friend undo_database;
               session(undo_database& db): _db(db) {}
               undo_database& _db;
               bool _apply_undo = !_db._disabled;
         };

         void    disable();
         void    enable();

         session start_undo_session();
         void on_create( const object& obj );
         void on_modify( const object& obj );
         void on_remove( const object& obj );

      private:
         void undo();
         void merge();

         struct undo_state
         {
            unordered_map<object_id_type, unique_ptr<object> > old_values;
            unordered_map<object_id_type, uint64_t>            old_index_next_ids;
            set<object_id_type>                                new_ids;
            unordered_map<object_id_type, unique_ptr<object> > removed;
         };


         bool                   _disabled = false;
         std::deque<undo_state> _stack;
         database&              _db;
   };

} }
