#pragma once
#include <bts/chain/block.hpp>
#include <bts/chain/types.hpp>

namespace bts { namespace chain {

   struct fork_item 
   {
      weak_ptr< fork_item > prev;
      signed_block          data;
   };

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
         fork_database();
         void reset();

         void                             start_block( signed_block b );
         void                             remove( block_id_type b );
         void                             set_head( shared_ptr<fork_item> h );
         shared_ptr<fork_item>            push_block( signed_block b );
         shared_ptr<fork_item>            head()const { return _head; }
         void                             pop_block();

         typedef shared_ptr<fork_item> item_ptr;
         typedef vector<item_ptr>      branch_type;

         /**
          *  Given two head blocks, return two branches of the fork graph that
          *  end with a common ancestor (same prior block)
          */
         pair< branch_type, branch_type >  fetch_branch_from( block_id_type first, 
                                                              block_id_type second )const;
      private:
         unordered_map< block_id_type, shared_ptr<fork_item> > _by_id;
         vector< vector<shared_ptr<fork_item>> >               _by_num;
         shared_ptr<fork_item>                                 _head;
   };
} } // bts::chain 
