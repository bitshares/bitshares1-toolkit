#pragma once
#include <bts/chain/types.hpp>
#include <fc/io/raw.hpp>

namespace bts { namespace chain {

   /**
    *  @brief base for all database objects
    *
    *  The object is the fundamental building block of the database and
    *  is the level upon which undo/redo operations are performed.  Objects
    *  are used to track data and their relationships and provide an effecient
    *  means to find and update information.   
    *
    *  Objects are assigned a unique and sequential object ID by the database within
    *  the id_space defined in the object.  Derived objects from plugins should specify
    *  a unique id_space if they want their objects to be universially numbered across
    *  all interpretations of the transaction history.
    *
    *  All objects must be serializable via FC_REFLECT() and their content must be
    *  faithfully restored.   Additionally all objects must be copy-constructable and
    *  assignable in a relatively efficient manner.  In general this means that objects 
    *  should only refer to other objects by ID and avoid expensive operations when
    *  they are copied, especially if they are modified frequently. 
    *
    *  Additionally all objects may be annotated by plugins which wish to maintain
    *  additional information to an object.  There can be at most one annotation 
    *  per id_space for each object.   An example of an annotation would be tracking
    *  extra data not required by validation such as the name and description of
    *  a user asset.  By carefully organizing how information is organized and
    *  tracked systems can minimize the workload to only that which is necessary
    *  to perform their function.  
    *
    *  @note Do not use multiple inheritance with object because the code assumes
    *  a static_cast will work between object and derived types.  
    */
   class object 
   {
      public:
         object(){}
         virtual ~object(){};

         static const uint8_t space_id = protocol_ids;
         static const uint8_t type_id  = base_object_type;

         /** return object_id_type() if no anotation is found for id_space */
         object_id_type          get_annotation( id_space_type annotation_id_space )const;
         void                    set_annotation( object_id_type id );

         // serialized
         object_id_type          id;

         /// these methods are implemented for derived classes by inheriting abstract_object<DerivedClass>
         virtual unique_ptr<object> clone()const = 0;
         virtual void               move_from( object& obj ) = 0;
         virtual variant            to_variant()const  = 0;
         virtual vector<char>       pack()const = 0;
   };

   template<typename DerivedClass>
   class abstract_object : public object
   {
      public:
         virtual unique_ptr<object> clone()const 
         { 
            return unique_ptr<object>(new DerivedClass( *static_cast<const DerivedClass*>(this) )); 
         }

         virtual void    move_from( object& obj ) 
         {
            static_cast<DerivedClass&>(*this) = std::move( static_cast<DerivedClass&>(obj) );
         }
         virtual variant to_variant()const { return variant( static_cast<const DerivedClass&>(*this) ); }
         virtual vector<char> pack()const  { return fc::raw::pack( static_cast<const DerivedClass&>(*this) ); }
   };

   template<typename DerivedClass>
   class annotated_object : public abstract_object<DerivedClass> 
   {
      public:
         /**
          *  Annotations should be accessed via get_annotation and set_annotation so
          *  that they can be maintained in sorted order.
          */
         flat_map<uint8_t,object_id_type> annotations;
   };

} }

FC_REFLECT( bts::chain::object, (id) )
FC_REFLECT_DERIVED_TEMPLATE( (typename Derived), bts::chain::annotated_object<Derived>, (bts::chain::object), (annotations) )


