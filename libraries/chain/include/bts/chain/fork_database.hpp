#pragma once
#include <bts/chain/block.hpp>
#include <bts/chain/types.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>

namespace bts { namespace chain {
   using boost::multi_index_container;
   using namespace boost::multi_index;

   struct fork_item
   {
      fork_item( signed_block d )
      :num(d.block_num()),id(d.id()),data( std::move(d) ){}

      weak_ptr< fork_item > prev;
      uint32_t              num;
      /**
       * Used to flag a block as invalid and prevent other blocks from
       * building on top of it.
       */
      bool                  invalid = false;
      block_id_type         id;
      signed_block          data;
   };
   typedef shared_ptr<fork_item> item_ptr;

   /**
    *  As long as blocks are pushed in order the fork
    *  database will maintain a linked tree of all blocks
    *  that branch from the start_block.  The tree will
    *  have a maximum depth of 1024 blocks after which
    *  the database will start lopping off forks.
    *
    *  Every time a block is pushed into the fork DB the
    *  block with the highest block_num will be returned.
    */
   class fork_database
   {
      public:
         typedef vector<item_ptr>      branch_type;

         fork_database();
         void reset();

         void                             start_block( signed_block b );
         void                             remove( block_id_type b );
         void                             set_head( shared_ptr<fork_item> h );
         shared_ptr<fork_item>            fetch_block( const block_id_type& id )const;
         vector<item_ptr>                 fetch_block_by_number( uint32_t n )const;
         shared_ptr<fork_item>            push_block( signed_block b );
         shared_ptr<fork_item>            head()const { return _head; }
         void                             pop_block();


         /**
          *  Given two head blocks, return two branches of the fork graph that
          *  end with a common ancestor (same prior block)
          */
         pair< branch_type, branch_type >  fetch_branch_from( block_id_type first,
                                                              block_id_type second )const;

         struct block_id{};
         struct block_num{};
         typedef multi_index_container<
            item_ptr,
            indexed_by<
               hashed_unique< tag<block_id>, member< fork_item, block_id_type, &fork_item::id> >,
               ordered_non_unique< tag<block_num>, member<fork_item,uint32_t,&fork_item::num> >
            >
         > fork_multi_index_type;

      private:
         fork_multi_index_type    _index;
         shared_ptr<fork_item>    _head;
   };
} } // bts::chain
