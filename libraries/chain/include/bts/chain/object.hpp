#pragma once
#include <bts/chain/types.hpp>
#include <fc/io/raw.hpp>

namespace bts { namespace chain {

   /**
    *  Objects are divided into namespaces each with 
    *  their own unique sequence numbers for both
    *  object IDs and types.  These namespaces
    *  are useful for building plugins that wish
    *  to leverage the same ID infrastructure.
    */
   enum id_space_names
   {
      /** objects that may be directly referred to by the protocol operations */
      protocoal_ids = 0,
      /** objects created for implementation specific reasons such as maximizing performance */
      implementation_ids = 1,
      /** objects created for the purpose of tracking meta info not used by validation, 
       * such as names and descriptions of assets or the value of data objects. */
      meta_info_ids = 2
   };

   /**
    *  List all object types from all namespaces here so they can
    *  be easily reflected and displayed in debug output.  If a 3rd party
    *  wants to extend the core code then they will have to change the
    *  packed_object::type field from enum_type to uint16 to avoid
    *  warnings when converting packed_objects to/from json.
    */
   enum object_type
   {
      null_object_type                  = protocoal_ids,
      base_object_type                   ,
      key_object_type                    ,
      account_object_type                ,
      asset_object_type                  ,
      delegate_object_type               ,
      impl_account_balance_object_type  = implementation_ids<<8,
      impl_delegate_vote_object_type     ,
      meta_info_asset_object_type       = meta_info_ids<<8
   };


   struct packed_object
   {
      enum_type<uint16_t,object_type>  type = null_object_type;
      vector<char>                     data;
      
      packed_object(){}
      packed_object( packed_object&& ) = default;
      packed_object( const packed_object& ) = default;
      packed_object& operator=(const packed_object&) = default;
      packed_object& operator=(packed_object&&) = default;

      template<typename T>
      packed_object( const T& o )
      {
         type = o.type;
         data = fc::raw::pack( o );
      }

      template<typename T>
      void unpack( T& o )const
      { 
         FC_ASSERT( o.type == type ); 
         fc::raw::unpack( data, o ); 
      }
   };

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
    */
   class object 
   {
      public:
         object( object_type t = base_object_type ):type(t){};
         virtual ~object(){};

         static const uint16_t id_space = 0;

         uint64_t object_id()const       { return id & 0x7fffffffffffffff; }
         /** local object ID is the ID within the individual sequence numbers
          * of the id_space. 
          */
         uint64_t local_object_id()const { return id & 0x0000ffffffffffff; }
         uint16_t get_id_space()const    { return (id >> 48) & 0x7fff; }
         bool     is_dirty()const        { return id & 0x8000000000000000; }
         void     mark_dirty()           { id |= 0x8000000000000000;       }

         object_id_type                  get_annotation( uint16_t annotation_id_space )const;
         void                            set_annotation( object_id_type id );

         const enum_type<uint16_t,object_type> type;
         object_id_type                  id = 0;

         /**
          *  Annotations should be accessed via get_annotation and set_annotation so
          *  that they can be maintained in sorted order.
          */
         vector<object_id_type>          annotations;
   };

} }

FC_REFLECT_ENUM( bts::chain::object_type,
                 (null_object_type)
                 (base_object_type)
                 (key_object_type)
                 (account_object_type) 
                 (asset_object_type)
                 (delegate_object_type)
                 (impl_account_balance_object_type) 
                 (impl_delegate_vote_object_type)
                 (meta_info_asset_object_type)
               )

FC_REFLECT( bts::chain::object, (type)(id) )
