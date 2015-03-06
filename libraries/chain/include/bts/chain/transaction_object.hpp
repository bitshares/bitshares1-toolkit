#pragma once
#include <bts/chain/index.hpp>

#include <fc/uint128.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/mem_fun.hpp>

namespace bts { namespace chain {
   using boost::multi_index_container;
   using namespace boost::multi_index;
   /**
    *  The purpose of this object is to enable the detection
    *  of duplicate transactions.  When a transaction is
    *  included in a block a transaction_object is
    *  added.  At the end of block processing all
    *  transaction_objects that have expired can
    *  be removed from the index.
    */
   class transaction_object : public object
   {
      public:
         /**
          * The odds of a collision are sufficiently low
          * that it doesn't make sense to consume extra
          * ram to have more bits.  If there is a collision
          * then simply changing the transaction by a single
          * bit (retry) would resolve it.
          */
         fc::uint128             transaction_id;
         time_point_sec          expiration;

         uint64_t instance()const { return id.instance(); }
   };

   /**
    *  Unlike most other indexes, there is no need to keep all transaction data
    *  for ever.
    */
   class transaction_index : public index
   {
      public:
         struct trx_id{};
         struct instance{};
         struct expiration{};
         typedef multi_index_container<
            transaction_object,
            indexed_by<
               hashed_unique< tag<instance>, const_mem_fun< transaction_object, uint64_t, &transaction_object::instance >>,
               hashed_unique< tag<trx_id>, BOOST_MULTI_INDEX_MEMBER( transaction_object, fc::uint128, transaction_id )>,
               ordered_non_unique< tag<expiration>, BOOST_MULTI_INDEX_MEMBER( transaction_object, time_point_sec, expiration)>
            >
         > transaction_multi_index_type;

         virtual object_id_type get_next_available_id()const override { return transaction_obj_id_type(_index.size()); }
         virtual packed_object  get_meta_object()const override;
         virtual void           set_meta_object( const packed_object& obj ) override;

         virtual const object*  create(const std::function<void(object*)>& constructor,
                                        object_id_type = object_id_type());

         virtual int64_t size()const { return _index.size(); }

         virtual void modify( const object* obj, const std::function<void(object*)>& m )override;
         virtual void add( unique_ptr<object> o )override;
         virtual void remove( object_id_type id )override;
         virtual const object* get( object_id_type id )const override;

      private:
         object_id_type               _next_obj_id;
         transaction_multi_index_type _index;

   };

} }

FC_REFLECT_DERIVED( bts::chain::transaction_object, (bts::chain::object), (transaction_id)(expiration) )
