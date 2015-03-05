#pragma once
#include <bts/chain/index.hpp>

namespace bts { namespace chain {

   template<typename T>
   class flat_index : public index
   {
      public:
         typedef T object_type;

         virtual object_id_type get_next_available_id()const override
         {
            return object_id_type( T::space_id, T::type_id, size());
         }

         virtual packed_object  get_meta_object()const override
         {
            return packed_object( index_meta_object( get_next_available_id() ) );
         }
         virtual void           set_meta_object( const packed_object& obj ) override
         { try {
            index_meta_object meta;
            obj.unpack(meta);
            wdump( (meta.next_object_instance) );
            _objects.resize( meta.next_object_instance );
         } FC_CAPTURE_AND_RETHROW( (obj) ) }

         virtual const object*  create( const std::function<void(object*)>& constructor,
                                        object_id_type /*requested_id*/ ) override
         {
             auto next_id = get_next_available_id();
             auto instance = next_id.instance();
             if( instance >= _objects.size() ) _objects.resize( instance + 1 );
             auto result = &_objects[instance];
             result->id = next_id;
             constructor( result );
             return result;
         }

         virtual int64_t size()const { return _objects.size(); }

         virtual void modify( const object* obj, const std::function<void(object*)>& modify_callback ) override
         {
            assert( obj != nullptr );
            assert( obj->id.instance() < _objects.size() );
            modify_callback( &_objects[obj->id.instance()]);
         }

         virtual void add( unique_ptr<object> o )override
         {
             assert( o );
             assert( o->id.space() == T::space_id );
             assert( o->id.type() == T::type_id );
             _objects.push_back( std::move(*static_cast<T*>(o.get())) );
         }

         virtual void remove( object_id_type id ) override
         {
            assert( id.space() == T::space_id );
            assert( id.type() == T::type_id );
            const auto instance = id.instance();

            if( instance == _objects.size() - 1 )
               _objects.pop_back();
            //else if( instance < _objects.size() )
            //   _objects[instance].reset();
         }

         virtual const object* get( object_id_type id )const override
         {
            assert( id.space() == T::space_id );
            assert( id.type() == T::type_id );

            const auto instance = id.instance();
            if( instance >= _objects.size() ) return nullptr;
            return &_objects[instance];
         }

         virtual void inspect_all_objects(std::function<void (const object*)> inspector)
         {
            try {
               for( const auto& object : _objects )
                  inspector(&object);
            } FC_CAPTURE_AND_RETHROW()
         }

         class const_iterator
         {
            public:
               const_iterator(){}
               const_iterator( const typename vector<T>::const_iterator& a ):_itr(a){}
               friend bool operator==( const const_iterator& a, const const_iterator& b ) { return a._itr == b._itr; }
               friend bool operator!=( const const_iterator& a, const const_iterator& b ) { return a._itr != b._itr; }
               const T& operator*()const { return *_itr; }
               const_iterator& operator++(int){ ++_itr; return *this; }
               const_iterator& operator++()   { ++_itr; return *this; }
            private:
               typename vector<T>::const_iterator _itr;
         };
         const_iterator begin()const { return const_iterator(_objects.begin()); }
         const_iterator end()const   { return const_iterator(_objects.end());   }

      private:
         vector< T > _objects;
   };

} } // bts::chain
