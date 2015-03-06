#pragma once
#include <bts/chain/index.hpp>
#include <bts/chain/account_object.hpp>

namespace bts { namespace chain {

   /**
    *  @class account_index
    *  @brief enable account lookup by NAME or INSTANCE and enforce uniqueness
    */
   class account_index : public index
   {
      public:
         typedef account_object object_type;

         ~account_index(){}

         virtual object_id_type get_next_available_id()const override;
         virtual packed_object  get_meta_object()const override;
         virtual void           set_meta_object( const packed_object& obj ) override;

         /**
          * Builds a new object and assigns it the next available ID and then
          * initializes it with constructor and lastly inserts it into the index.
          */
         virtual const object*  create( const std::function<void(object*)>& constructor, 
                                        object_id_type requested_id = object_id_type() );

         virtual int64_t size()const;

         virtual void modify( const object* obj, const std::function<void(object*)>& m )override;
         virtual void replace( unique_ptr<object> o )override;
         virtual void add( unique_ptr<object> o )override;
         virtual void remove( object_id_type id )override;
         virtual const object* get( object_id_type id )const override;

         const account_object* get( const string& name )const;

         virtual void inspect_all_objects(std::function<void (const object*)> inspector);

         typedef vector<unique_ptr<account_object>>::const_iterator const_iterator;
         const_iterator begin()const { return accounts.begin(); }
         const_iterator end()const   { return accounts.end();   }

      private:
         vector<unique_ptr<account_object> >   accounts;
         unordered_map<string,uint64_t>        name_to_instance;
   };

} }
