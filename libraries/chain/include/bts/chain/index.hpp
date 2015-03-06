#pragma once
#include <bts/chain/types.hpp>
#include <bts/chain/object.hpp>
#include <bts/db/level_map.hpp>


namespace bts { namespace chain {
   class database;

   class index_observer
   {
      public:
         virtual ~index_observer(){}
         virtual void on_modify( object_id_type id, const object* obj ){}
         virtual void on_add( const object* obj ){}
         virtual void on_remove( object_id_type id ){}
   };

   class index_meta_object : public object
   {
      public:
       static const uint8_t space_id = implementation_ids;
       static const uint8_t type_id  = impl_index_meta_object_type;
       index_meta_object( uint64_t instance = 0 ):next_object_instance(instance)
       {
          id = object_id_type( space_id, type_id, 0 );
       }
       uint64_t next_object_instance = 0;
   };

   class index
   {
      public:
         virtual ~index(){}

         virtual uint8_t object_space_id()const = 0;
         virtual uint8_t object_type_id()const = 0;

         /**
          * This method must be deterministic based upon the set of
          * elements in the index.  Object IDs can be reused so long
          * as the order that they are added/removed does not change
          * the ID selection.
          *
          * @note - you can assume that when the UNDO state is
          * applied it will remove items from the highest ID first which
          * means that in a batch UNDO you can resize when the last
          * element is removed and reuse that ID
          */
         virtual object_id_type get_next_available_id()const = 0;
         virtual packed_object  get_meta_object()const = 0;
         virtual void           set_meta_object( const packed_object& obj ) = 0;

         /**
          * Builds a new object and assigns it the next available ID and then
          * initializes it with constructor and lastly inserts it into the index.
          */
         virtual const object*  create( const std::function<void(object*)>& constructor,
                                        object_id_type requested_id = object_id_type() ) = 0;

         virtual packed_object pack( const object* p )const  = 0;
         virtual void unpack( object* p, const packed_object& obj )const  = 0;
         virtual variant to_variant( const object* p )const  = 0;

         /**
          *  Opens the index loading objects from a level_db database
          */
         virtual void open( const shared_ptr<bts::db::level_map<object_id_type, packed_object>>& db ){}

         /**
          * Creates a new object that is free from the index and does not
          * claim the next_available_id.  This method is meant to be a helper for
          * dynamic deserialization of objects.
          */
         virtual unique_ptr<object> create_free_object()const = 0;
         virtual const object*      get( object_id_type id )const = 0;
         virtual int64_t            size()const = 0;
         virtual void               add( unique_ptr<object> o ) = 0;
         virtual void               modify( const object* obj, const std::function<void(object*)>& ) = 0;
         virtual void               replace( unique_ptr<object> o ) = 0;

         /**
          *   Lambda should have the signature:  void(Object*)
          */
         template<typename Object, typename Lambda>
         void modify( const Object* obj, const Lambda& l ) {
            modify( static_cast<const object*>(obj), std::function<void(object*)>( [&]( object* o ){ l( static_cast<Object*>(o) ); } ) );
         }

         virtual void               inspect_all_objects(std::function<void(const object*)> inspector) = 0;

         virtual void               remove( object_id_type id ) = 0;

         virtual void               add_observer( const shared_ptr<index_observer>& ) = 0;
   };

   class base_primary_index
   {
      public:
         base_primary_index( database* db ):_db(db){}
         void save_undo( const object* obj );

         void on_add( const object* obj );
         void on_remove( object_id_type id );
         void on_modify( object_id_type id, const object* obj );

      protected:
         vector< shared_ptr<index_observer> > _observers;

      private:
         database* _db;
   };

   template<typename DerivedIndex>
   class primary_index  : public DerivedIndex, public base_primary_index
   {
      public:
         typedef typename DerivedIndex::object_type ObjectType;

         primary_index( database* db )
         :base_primary_index(db){}

         virtual uint8_t object_space_id()const override
         { return ObjectType::space_id; }

         virtual uint8_t object_type_id()const override
         { return ObjectType::type_id; }

         virtual void open( const shared_ptr<bts::db::level_map<object_id_type, packed_object>>& db )
         {
            auto first = object_id_type( DerivedIndex::object_type::space_id, DerivedIndex::object_type::type_id, 0 );
            auto last = object_id_type( DerivedIndex::object_type::space_id, DerivedIndex::object_type::type_id+1, 0 );
            auto itr = db->lower_bound( first );
            while( itr.valid() && itr.key() < last )
            {
               unique_ptr<ObjectType> next_obj( new ObjectType() );
               unpack( next_obj.get(), itr.value() );
               add( std::move( next_obj ) );
               ++itr;
            }
         }

         virtual void  add( unique_ptr<object> o ) override
         {
            assert(o);
            object* obj = o.get();
            DerivedIndex::add(std::move(o));
            on_add(obj);
         }

         virtual void  remove( object_id_type id ) override
         {
            DerivedIndex::remove(id);
            on_remove(id);
         }

         virtual void modify( const object* obj, const std::function<void(object*)>& m )override
         {
            assert( obj != nullptr );
            save_undo( obj );
            DerivedIndex::modify( obj, m );
            on_modify( obj->id, obj );
         }

         virtual void replace( unique_ptr<object> obj ) override
         {
            DerivedIndex::replace( std::move(obj) );
         }

         virtual void add_observer( const shared_ptr<index_observer>& o ) override
         {
            _observers.emplace_back( o );
         }

         /** does not add it to the index */
         virtual unique_ptr<object> create_free_object()const override
         {
            return unique_ptr<ObjectType>( new ObjectType() );
         }

         virtual packed_object pack( const object* p )const override
         {
            const auto& cast = *((const ObjectType*)p);
            return packed_object(cast);
         }

         virtual void unpack( object* p, const packed_object& obj )const override
         {
            auto& cast = *((ObjectType*)p);
            obj.unpack(cast);
         }

         virtual variant to_variant( const object* p )const override
         {
            const auto& cast = *((const ObjectType*)p);
            return fc::variant(cast);
         }

   };


} }
FC_REFLECT_DERIVED( bts::chain::index_meta_object, (bts::chain::object), (next_object_instance) );

