#pragma once
#include <fc/io/raw.hpp>
#include <bts/db/index.hpp>
#include <bts/db/generic_index.hpp>
#include <fc/uint128.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/mem_fun.hpp>

namespace bts { namespace chain {
   using namespace bts::db;
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
   class transaction_object : public abstract_object<transaction_object>
   {
      public:
         static const uint8_t space_id = implementation_ids;
         static const uint8_t type_id  = impl_transaction_object_type;

         /**
          * The odds of a collision are sufficiently low
          * that it doesn't make sense to consume extra
          * ram to have more bits.  If there is a collision
          * then simply changing the transaction by a single
          * bit (retry) would resolve it.
          */
         transaction_id_type     transaction_id;
         time_point_sec          expiration;
   };


   struct by_expiration;
   struct by_id;
   struct by_trx_id;
   typedef multi_index_container<
      transaction_object,
      indexed_by<
         hashed_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
         hashed_unique< tag<by_trx_id>, BOOST_MULTI_INDEX_MEMBER( transaction_object, transaction_id_type, transaction_id )>,
         ordered_non_unique< tag<by_expiration>, BOOST_MULTI_INDEX_MEMBER( transaction_object, time_point_sec, expiration)>
      >
   > transaction_multi_index_type;

   typedef generic_index<transaction_object, transaction_multi_index_type> transaction_index;

} }

FC_REFLECT_DERIVED( bts::chain::transaction_object, (bts::db::object), (transaction_id)(expiration) )
