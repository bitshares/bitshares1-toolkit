#include <bts/chain/types.hpp>
#include <bts/chain/time.hpp>
#include <bts/chain/database.hpp>

#include <bts/chain/key_object.hpp>
#include <bts/chain/account_object.hpp>
#include <bts/chain/asset_object.hpp>
#include <bts/chain/delegate_object.hpp>
#include <bts/chain/limit_order_object.hpp>
#include <bts/chain/short_order_object.hpp>
#include <bts/chain/block_summary_object.hpp>
#include <bts/chain/proposal_object.hpp>
#include <bts/db/simple_index.hpp>
#include <bts/db/flat_index.hpp>

#include <bts/chain/transaction_evaluation_state.hpp>
#include <bts/chain/key_evaluator.hpp>
#include <bts/chain/account_evaluator.hpp>
#include <bts/chain/delegate_evaluator.hpp>
#include <bts/chain/asset_evaluator.hpp>
#include <bts/chain/limit_order_evaluator.hpp>
#include <bts/chain/short_order_evaluator.hpp>
#include <bts/chain/asset_evaluator.hpp>
#include <bts/chain/transaction_object.hpp>
#include <bts/chain/transfer_evaluator.hpp>
#include <bts/chain/proposal_evaluator.hpp>
#include <bts/chain/operation_history_object.hpp>

#include <fc/io/raw.hpp>
#include <fc/crypto/digest.hpp>
#include <fc/container/flat.hpp>
#include <fc/uint128.hpp>

namespace bts { namespace chain {
   /*
template<typename T>
struct restore_on_scope_exit
{
   restore_on_scope_exit( T& v)
   :original_copy(v),value(v){}
   ~restore_on_scope_exit(){ value = original_copy; }
   T   original_copy;
   T&  value;
};

//template<typename T>
//restore_on_scope_exit<T> make_restore_on_exit( T& v ) { return restore_on_scope_exit<T>(v); }
*/

database::database()
{
   initialize_indexes();
   initialize_evaluators();
}

database::~database(){
   if( _pending_block_session )
      _pending_block_session->commit();
}

void database::close(uint32_t blocks_to_rewind)
{
   _pending_block_session.reset();

   for(int i = 0; i < blocks_to_rewind && head_block_num() > 0; ++i)
      pop_block();

   object_database::close();

   if( _block_id_to_block.is_open() )
      _block_id_to_block.close();

   _fork_db.reset();
}

const asset_object& database::get_core_asset() const
{
   return get(asset_id_type());
}

void database::wipe(bool include_blocks)
{
   close();
   object_database::wipe();
   if( include_blocks )
      fc::remove_all( get_data_dir() / "database" / "block_id_to_block" );
}

void database::open( const fc::path& data_dir, const genesis_allocation& initial_allocation )
{ try {
   ilog("Open database in ${d}", ("d", data_dir));
   object_database::open( data_dir );

   _block_id_to_block.open( data_dir / "database" / "block_num_to_block" );

   if( !find(global_property_id_type()) )
      init_genesis(initial_allocation);

   _pending_block.previous  = head_block_id();
   _pending_block.timestamp = head_block_time();

   auto last_block_itr = _block_id_to_block.last();
   if( last_block_itr.valid() )
      _fork_db.start_block( last_block_itr.value() );

} FC_CAPTURE_AND_RETHROW( (data_dir) ) }

void database::initialize_evaluators()
{
   _operation_evaluators.resize(255);
   register_evaluator<key_create_evaluator>();
   register_evaluator<account_create_evaluator>();
   register_evaluator<account_update_evaluator>();
   register_evaluator<account_whitelist_evaluator>();
   register_evaluator<delegate_create_evaluator>();
   register_evaluator<asset_create_evaluator>();
   register_evaluator<asset_issue_evaluator>();
   register_evaluator<asset_update_evaluator>();
   register_evaluator<limit_order_create_evaluator>();
   register_evaluator<limit_order_cancel_evaluator>();
   register_evaluator<short_order_create_evaluator>();
   register_evaluator<short_order_cancel_evaluator>();
   register_evaluator<call_order_update_evaluator>();
   register_evaluator<transfer_evaluator>();
   register_evaluator<asset_fund_fee_pool_evaluator>();
   register_evaluator<delegate_publish_feeds_evaluator>();
   register_evaluator<proposal_create_evaluator>();
   register_evaluator<proposal_update_evaluator>();
   register_evaluator<proposal_delete_evaluator>();
}

void database::initialize_indexes()
{
   reset_indexes();

   //Protocol object indexes
   add_index< primary_index< asset_index> >();
   add_index< primary_index< account_index> >();
   add_index< primary_index< simple_index<key_object>> >();
   add_index< primary_index< simple_index<delegate_object>> >();
   add_index< primary_index< simple_index<witness_object>> >();
   add_index< primary_index< limit_order_index > >();
   add_index< primary_index< short_order_index > >();
   add_index< primary_index< call_order_index > >();
   add_index< primary_index< proposal_index > >();

   //Implementation object indexes
   add_index< primary_index< transaction_index                             > >();
   add_index< primary_index< simple_index< global_property_object         >> >();
   add_index< primary_index< simple_index< dynamic_global_property_object >> >();
   add_index< primary_index< simple_index< account_balance_object         >> >();
   add_index< primary_index< simple_index< asset_dynamic_data_object      >> >();
   add_index< primary_index< flat_index<   vote_tally_object              >> >();
   add_index< primary_index< flat_index<   delegate_feeds_object          >> >();
   add_index< primary_index< flat_index<   block_summary_object           >> >();
}

void database::init_genesis(const genesis_allocation& initial_allocation)
{
   _undo_db.disable();

   fc::ecc::private_key genesis_private_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("genesis")));
   const key_object& genesis_key =
      create<key_object>( [&genesis_private_key](key_object& k) {
         k.key_data = public_key_type(genesis_private_key.get_public_key());
      });
   const account_balance_object& genesis_balance =
      create<account_balance_object>( [&](account_balance_object& b){
         b.add_balance(asset(BTS_INITIAL_SUPPLY));
      });
   const account_object& genesis_account =
      create<account_object>( [&](account_object& n) {
         n.owner.add_authority(genesis_key.get_id(), 1);
         n.owner.weight_threshold = 1;
         n.active = n.owner;
         n.voting_key = genesis_key.id;
         n.memo_key = genesis_key.id;
         n.balances = genesis_balance.id;
      });

   vector<delegate_id_type> init_delegates;
   vector<witness_id_type> init_witnesses;

   for( int i = 0; i < BTS_MIN_DELEGATE_COUNT; ++i )
   {
      const account_balance_object& balance_obj =
         create<account_balance_object>( [&](account_balance_object& b){
            (void)b;
         });
      const account_object& delegate_account =
         create<account_object>( [&](account_object& a) {
            a.active = a.owner = genesis_account.owner;
            a.name = string("init") + fc::to_string(i);
            a.balances = balance_obj.id;
         });
      const vote_tally_object& vote =
         create<vote_tally_object>( [&](vote_tally_object&) {
            // Nothing to do here...
         });
      const delegate_feeds_object& delegate_feeds = create<delegate_feeds_object>([](delegate_feeds_object&){});
      const delegate_object& init_delegate = create<delegate_object>( [&](delegate_object& d) {
         d.delegate_account = delegate_account.id;
         d.vote = vote.id;
         d.feeds = delegate_feeds.id;
      });
      init_delegates.push_back(init_delegate.id);

      const vote_tally_object& witness_vote =
         create<vote_tally_object>( [&](vote_tally_object&) {
            // Nothing to do here...
         });
      const witness_object& init_witness = create<witness_object>( [&](witness_object& d) {
            d.witness_account = delegate_account.id;
            d.vote = witness_vote.id;
            secret_hash_type::encoder enc;
            fc::raw::pack( enc, genesis_private_key );
            fc::raw::pack( enc, d.last_secret );
            d.next_secret = secret_hash_type::hash(enc.result());
      });
      init_witnesses.push_back(init_witness.id);

   }
   create<block_summary_object>( [&](block_summary_object& p) {
   });

   const global_property_object& properties =
      create<global_property_object>( [&](global_property_object& p) {
         p.active_delegates = init_delegates;
         p.active_witnesses = init_witnesses;
      });
   (void)properties;

   create<dynamic_global_property_object>( [&](dynamic_global_property_object& p) {
      });

   const asset_dynamic_data_object& dyn_asset =
      create<asset_dynamic_data_object>( [&]( asset_dynamic_data_object& a ) {
         a.current_supply = BTS_INITIAL_SUPPLY;
      });

   const asset_object& core_asset =
     create<asset_object>( [&]( asset_object& a ) {
         a.symbol = BTS_SYMBOL;
         a.max_supply = BTS_INITIAL_SUPPLY;
         a.flags = 0;
         a.issuer_permissions = 0;
         a.issuer = genesis_account.id;
         a.core_exchange_rate.base.amount = 1;
         a.core_exchange_rate.base.asset_id = 0;
         a.core_exchange_rate.quote.amount = 1;
         a.core_exchange_rate.quote.asset_id = 0;
         a.dynamic_asset_data_id = dyn_asset.id;
      });
   assert( asset_id_type(core_asset.id) == asset().asset_id );
   assert( genesis_balance.get_balance(core_asset.id) == asset(dyn_asset.current_supply) );
   (void)core_asset;

   if( !initial_allocation.empty() )
   {
      share_type total_allocation = 0;
      for( const auto& handout : initial_allocation )
         total_allocation += handout.second;

      fc::time_point start_time = fc::time_point::now();

      for( const auto& handout : initial_allocation )
      {
         asset amount(handout.second);
         amount.amount = ((fc::uint128(amount.amount.value) * BTS_INITIAL_SUPPLY)/total_allocation.value).to_uint64();
         if( amount.amount == 0 )
         {
            wlog("Skipping zero allocation to ${k}", ("k", handout.first));
            continue;
         }

         signed_transaction trx;
         trx.operations.emplace_back(key_create_operation({genesis_account.id, asset(), handout.first}));
         object_id_type key_id(relative_protocol_ids, 0, 0);
         authority account_authority;
         account_authority.add_authority(key_id_type(key_id), 1);
         account_create_operation cop;
         cop.fee_paying_account = account_id_type(1);
         cop.active = account_authority;
         cop.owner = account_authority;
         cop.voting_key = key_id;
         cop.memo_key = key_id;
         trx.operations.push_back(cop);
         trx.validate();
         auto ptrx = apply_transaction(trx, ~0);
         trx = signed_transaction();
         account_id_type account_id(ptrx.operation_results.back().get<object_id_type>());
         trx.operations.emplace_back(transfer_operation({
                                                           genesis_account.id,
                                                           account_id,
                                                           amount,
                                                           asset(),
                                                           vector<char>()
                                                        }));
         trx.validate();
         apply_transaction(trx, ~0);
      }

      if( genesis_balance.get_balance(asset_id_type()).amount > 0 )
      {
         asset leftovers = genesis_balance.get_balance(asset_id_type());
         modify(genesis_balance, [](account_balance_object& b) {
            b.balances.clear();
         });
         modify(core_asset.dynamic_asset_data_id(*this), [&leftovers](asset_dynamic_data_object& d) {
            d.accumulated_fees += leftovers.amount;
         });
      }

      fc::microseconds duration = fc::time_point::now() - start_time;
      ilog("Finished allocating to ${n} accounts in ${t} milliseconds.",
           ("n", initial_allocation.size())("t", duration.count() / 1000));
   }
   _undo_db.enable();
}

/**
 *  This method should be called after the genesis state has
 *  been initialized.
 */
void database::reindex(fc::path data_dir, genesis_allocation initial_allocation)
{
   wipe(false);
   initialize_indexes();
   open(data_dir, initial_allocation);

   auto start = fc::time_point::now();
   auto itr = _block_id_to_block.begin();
   while( itr.valid() )
   {
      apply_block( itr.value(), skip_delegate_signature |
                                skip_transaction_signatures |
                                skip_undo_block |
                                skip_undo_transaction |
                                skip_transaction_dupe_check |
                                skip_tapos_check );
      ++itr;
   }
   auto end = fc::time_point::now();
   wdump( ((end-start).count()/1000000.0) );
}

asset database::current_delegate_registration_fee()const
{
   return asset();
}

void database::apply_block( const signed_block& next_block, uint32_t skip )
{ try {
   const witness_object& signing_witness = validate_block_header(skip, next_block);
   const auto& global_props = get_global_properties();
   const auto& dynamic_global_props = get<dynamic_global_property_object>(dynamic_global_property_id_type());

   _current_block_num    = next_block.block_num();
   _current_trx_in_block = 0;

   for( const auto& trx : next_block.transactions )
   {
      /* We do not need to push the undo state for each transaction
       * because they either all apply and are valid or the
       * entire block fails to apply.  We only need an "undo" state
       * for transactions when validating broadcast transactions or
       * when building a block.
       */
      apply_transaction( trx, skip );
      ++_current_trx_in_block;
   }

   update_global_dynamic_data( next_block );
   update_signing_witness(signing_witness, next_block);

   auto current_block_interval = global_props.parameters.block_interval;

   // Are we at the maintenance interval?
   if( dynamic_global_props.next_maintenance_time <= next_block.timestamp )
      // This will update _pending_block.timestamp if the block interval has changed
      perform_chain_maintenance(next_block, global_props);
   // If we're at the end of a round, shuffle the active witnesses
   // We can skip this if they were just updated during chain maintenance
   else if( (next_block.block_num() % global_props.active_delegates.size()) == 0 )
      modify(global_props, [this](global_property_object& p) {
         shuffle_vector(p.active_witnesses);
      });

   create_block_summary(next_block);
   clear_expired_transactions();
   clear_expired_proposals();
   clear_expired_orders();

   // notify observers that the block has been applied
   applied_block( next_block ); //emit
   _applied_ops.clear();

   update_pending_block(next_block, current_block_interval);
} FC_CAPTURE_AND_RETHROW( (next_block.block_num())(skip) )  }

time_point database::get_next_generation_time( witness_id_type del_id )const
{
   const auto& gp = get_global_properties();
   auto now = bts::chain::now();
   const auto& active_witness = gp.active_witnesses;
   const auto& interval   = gp.parameters.block_interval;
   auto witness_slot = ((now.sec_since_epoch()+interval-1) /interval);
   for( uint32_t i = 0; i < active_witness.size(); ++i )
   {
      if( active_witness[ witness_slot % active_witness.size()] == del_id )
         return time_point_sec() + fc::seconds( witness_slot * interval );
      ++witness_slot;
   }
   FC_ASSERT( !"Not an Active Witness" );
}

std::pair<fc::time_point, witness_id_type> database::get_next_generation_time(const set<bts::chain::witness_id_type>& witnesses) const
{
   std::pair<fc::time_point, witness_id_type> result;
   result.first = fc::time_point::maximum();
   for( witness_id_type id : witnesses )
      result = std::min(result, std::make_pair(get_next_generation_time(id), id));
   return result;
}


signed_block database::generate_block( const fc::ecc::private_key& delegate_key,
                                       witness_id_type witness_id, uint32_t  skip )
{ try {
   const auto& witness_obj = witness_id(*this);

   if( !(skip & skip_delegate_signature) )
      FC_ASSERT( witness_obj.signing_key(*this).key() == delegate_key.get_public_key() );

   _pending_block.timestamp = get_next_generation_time( witness_id );

   secret_hash_type::encoder last_enc;
   fc::raw::pack( last_enc, delegate_key );
   fc::raw::pack( last_enc, witness_obj.last_secret );
   _pending_block.previous_secret = last_enc.result();

   secret_hash_type::encoder next_enc;
   fc::raw::pack( next_enc, delegate_key );
   fc::raw::pack( next_enc, _pending_block.previous_secret );
   _pending_block.next_secret_hash = secret_hash_type::hash(next_enc.result());

   _pending_block.witness = witness_id;
   if( !(skip & skip_delegate_signature) ) _pending_block.sign( delegate_key );

   FC_ASSERT( fc::raw::pack_size(_pending_block) <= get_global_properties().parameters.maximum_block_size );
   //This line used to std::move(_pending_block) but this is unsafe as _pending_block is later referenced without being
   //reinitialized. Future optimization could be to move it, then reinitialize it with the values we need to preserve.
   signed_block tmp = _pending_block;
   _pending_block.transactions.clear();
   push_block( tmp, skip );
   return tmp;
} FC_CAPTURE_AND_RETHROW( (witness_id) ) }

void database::update_active_witnesses()
{
   auto ids = sort_votable_objects<witness_object>();
   shuffle_vector(ids);

   modify( get_global_properties(), [&]( global_property_object& gp ){
      gp.active_witnesses = std::move(ids);
   });
}



void database::update_active_delegates()
{
   auto ids = sort_votable_objects<delegate_object>();

   // Update genesis authorities
   modify( get(account_id_type()), [&]( account_object& a ) {
      a.owner.weight_threshold = ids.size()/2 + 1;
      a.owner.auths.clear();
      for( delegate_id_type id : ids )
         a.owner.auths[get<delegate_object>(id).delegate_account]++;
      a.active = a.owner;
   });
   modify( get_global_properties(), [&]( global_property_object& gp ) {
      gp.active_delegates = std::move(ids);
   });
}

void database::update_global_dynamic_data( const signed_block& b )
{
   modify( dynamic_global_property_id_type(0)(*this), [&]( dynamic_global_property_object& dgp ){
      secret_hash_type::encoder enc;
      fc::raw::pack( enc, dgp.random );
      fc::raw::pack( enc, b.previous_secret );
      dgp.random = enc.result();
      dgp.head_block_number = b.block_num();
      dgp.head_block_id = b.id();
      dgp.time = b.timestamp;
      dgp.current_witness = b.witness;
   });
}

/**
 *  Removes the most recent block from the database and
 *  undoes any changes it made.
 */
void database::pop_block()
{ try {
   _pending_block_session.reset();
   _block_id_to_block.remove( _pending_block.previous );
   pop_undo();
   _pending_block.previous  = head_block_id();
   _pending_block.timestamp = head_block_time();
   _fork_db.pop_block();
} FC_CAPTURE_AND_RETHROW() }

void database::clear_pending()
{ try {
   _pending_block.transactions.clear();
   _pending_block_session.reset();
} FC_CAPTURE_AND_RETHROW() }

bool database::is_known_block( const block_id_type& id )const
{
   return _fork_db.is_known_block(id) || _block_id_to_block.find(id).valid();
}
/**
 *  Only return true *if* the transaction has not expired or been invalidated. If this
 *  method is called with a VERY old transaction we will return false, they should
 *  query things by blocks if they are that old.
 */
bool database::is_known_transaction( const transaction_id_type& id )const
{
  const auto& trx_idx = get_index_type<transaction_index>().indices().get<by_trx_id>();
  return trx_idx.find( id ) != trx_idx.end();
}

/**
 *  Push block "may fail" in which case every partial change is unwound.  After
 *  push block is successful the block is appended to the chain database on disk.
 *
 *  @return true if we switched forks as a result of this push.
 */
bool database::push_block( const signed_block& new_block, uint32_t skip )
{ try {
   if( !(skip&skip_fork_db) )
   {
      auto new_head = _fork_db.push_block( new_block );
      //If the head block from the longest chain does not build off of the current head, we need to switch forks.
      if( new_head->data.previous != head_block_id() )
      {
         edump((new_head->data.previous));
         //If the newly pushed block is the same height as head, we get head back in new_head
         //Only switch forks if new_head is actually higher than head
         if( new_head->data.block_num() > head_block_num() )
         {
            auto branches = _fork_db.fetch_branch_from( new_head->data.id(), _pending_block.previous );
            for( auto item : branches.first )
               wdump( ("new")(item->id)(item->data.previous) );
            for( auto item : branches.second )
               wdump( ("old")(item->id)(item->data.previous) );

            // pop blocks until we hit the forked block
            while( head_block_id() != branches.second.back()->data.previous )
               pop_block();

            // push all blocks on the new fork
            for( auto ritr = branches.first.rbegin(); ritr != branches.first.rend(); ++ritr )
            {
                optional<fc::exception> except;
                try {
                   auto session = _undo_db.start_undo_session();
                   apply_block( (*ritr)->data, skip );
                   _block_id_to_block.store( new_block.id(), (*ritr)->data );
                   session.commit();
                }
                catch ( const fc::exception& e ) { except = e; }
                if( except )
                {
                   edump( ("Encountered error when switching to a longer fork. Going back.")((*ritr)->id) );
                   // remove the rest of branches.first from the fork_db, those blocks are invalid
                   while( ritr != branches.first.rend() )
                   {
                      _fork_db.remove( (*ritr)->data.id() );
                      ++ritr;
                   }
                   _fork_db.set_head( branches.second.front() );

                   // pop all blocks from the bad fork
                   while( head_block_id() != branches.second.back()->data.previous )
                      pop_block();

                   // restore all blocks from the good fork
                   for( auto ritr = branches.second.rbegin(); ritr != branches.second.rend(); ++ritr )
                   {
                      auto session = _undo_db.start_undo_session();
                      apply_block( (*ritr)->data, skip );
                      _block_id_to_block.store( new_block.id(), (*ritr)->data );
                      session.commit();
                   }
                   throw *except;
                }
            }
            return true;
         }
         else return false;
      }
   }

   // If there is a pending block session, then the database state is dirty with pending transactions.
   // Drop the pending session to reset the database to a clean head block state.
   // TODO: Preserve pending transactions, and re-apply any which weren't included in the new block.
   clear_pending();

   try {
      auto session = _undo_db.start_undo_session();
      apply_block( new_block, skip );
      _block_id_to_block.store( new_block.id(), new_block );
      session.commit();
   } catch ( const fc::exception& e ) {
      elog("Failed to push new block:\n${e}", ("e", e.to_detail_string()));
      _fork_db.remove(new_block.id());
      throw;
   }

   return false;
} FC_CAPTURE_AND_RETHROW( (new_block) ) }

/**
 *  Attempts to push the transaction into the pending queue
 *
 * When called to push a locally generated transaction, set the skip_block_size_check bit on the skip argument. This
 * will allow the transaction to be pushed even if it causes the pending block size to exceed the maximum block size.
 * Although the transaction will probably not propagate further now, as the peers are likely to have their pending
 * queues full as well, it will be kept in the queue to be propagated later when a new block flushes out the pending
 * queues.
 */
processed_transaction database::push_transaction( const signed_transaction& trx, uint32_t skip )
{
   //wdump((trx.digest())(trx.id()));
   // If this is the first transaction pushed after applying a block, start a new undo session.
   // This allows us to quickly rewind to the clean state of the head block, in case a new block arrives.
   if( !_pending_block_session ) _pending_block_session = _undo_db.start_undo_session();
   auto session = _undo_db.start_undo_session();
   auto processed_trx = apply_transaction( trx, skip );
   _pending_block.transactions.push_back(processed_trx);

   FC_ASSERT( (skip & skip_block_size_check) ||
              fc::raw::pack_size(_pending_block) <= get_global_properties().parameters.maximum_block_size );

   // The transaction applied successfully. Merge its changes into the pending block session.
   session.merge();
   return processed_trx;
}

processed_transaction database::push_proposal(const proposal_object& proposal)
{
   transaction_evaluation_state eval_state(this);
   eval_state._is_proposed_trx = true;

   //Inject the approving authorities into the transaction eval state
   std::transform(proposal.required_active_approvals.begin(),
                  proposal.required_active_approvals.end(),
                  std::inserter(eval_state.approved_by, eval_state.approved_by.begin()),
                  []( account_id_type id ) {
                     return std::make_pair(id, authority::active);
                  });
   std::transform(proposal.required_owner_approvals.begin(),
                  proposal.required_owner_approvals.end(),
                  std::inserter(eval_state.approved_by, eval_state.approved_by.begin()),
                  []( account_id_type id ) {
                     return std::make_pair(id, authority::owner);
                  });

   ilog("Attempting to push proposal ${prop}", ("prop", proposal));
   idump((eval_state.approved_by));

   eval_state.operation_results.reserve(proposal.proposed_transaction.operations.size());
   processed_transaction ptrx(proposal.proposed_transaction);

   auto session = _undo_db.start_undo_session();
   for( auto& op : proposal.proposed_transaction.operations )
      eval_state.operation_results.emplace_back(apply_operation(eval_state, op));
   remove(proposal);
   session.merge();

   ptrx.operation_results = std::move(eval_state.operation_results);
   return ptrx;
}

processed_transaction database::apply_transaction( const signed_transaction& trx, uint32_t skip )
{ try {
   trx.validate();
   auto& trx_idx = get_mutable_index_type<transaction_index>();
   auto trx_id = trx.id();
   FC_ASSERT( (skip & skip_transaction_dupe_check) ||
              trx_idx.indices().get<by_trx_id>().find(trx_id) == trx_idx.indices().get<by_trx_id>().end() );
   transaction_evaluation_state eval_state(this, skip&skip_transaction_signatures );
   if( !(skip & skip_transaction_signatures) )
   {
      for( auto sig : trx.signatures )
      {
         eval_state.signed_by.insert( fc::ecc::public_key( sig, trx.digest() ) );
      }
   }
   eval_state.operation_results.reserve( trx.operations.size() );

   const chain_parameters& chain_parameters = get_global_properties().parameters;
   processed_transaction ptrx(trx);
   _current_op_in_trx = 0;
   for( auto op : ptrx.operations )
   {
      eval_state.operation_results.emplace_back(apply_operation(eval_state, op));
      ++_current_op_in_trx;
   }
   ptrx.operation_results = std::move( eval_state.operation_results );

   //If we're skipping tapos check, but not expiration check, assume all transactions have maximum expiration time.
   fc::time_point_sec trx_expiration = _pending_block.timestamp + chain_parameters.maximum_time_until_expiration;
   if( !(skip & skip_tapos_check) )
   {
      //Check the TaPoS reference and expiration time
      //Remember that the TaPoS block number is abbreviated; it contains only the lower 16 bits.
      //Lookup TaPoS block summary by block number (remember block summary instances are the block numbers)
      const block_summary_object& tapos_block_summary
            = static_cast<const block_summary_object&>(get_index<block_summary_object>()
                                                       .get(block_summary_id_type((head_block_num() & ~0xffff)
                                                                                  + trx.ref_block_num)));
      //Verify TaPoS block summary has correct ID prefix, and that this block's time is not past the expiration
      FC_ASSERT( trx.ref_block_prefix == tapos_block_summary.block_id._hash[1] );
      trx_expiration = tapos_block_summary.timestamp + chain_parameters.block_interval*trx.relative_expiration.value;
      if( tapos_block_summary.timestamp == time_point_sec() )
         trx_expiration = now() + chain_parameters.block_interval*trx.relative_expiration.value;
      FC_ASSERT( _pending_block.timestamp <= trx_expiration ||
                 (trx.ref_block_prefix == 0 && trx.ref_block_num == 0 && head_block_num() < trx.relative_expiration)
                 , "", ("exp", trx_expiration) );
      FC_ASSERT( trx_expiration <= head_block_time() + chain_parameters.maximum_time_until_expiration
                 //Allow transactions through on block 1
                 || head_block_num() == 0 );
   }

   //Insert transaction into unique transactions database.
   if( !(skip & skip_transaction_dupe_check) )
   {
      create<transaction_object>([&](transaction_object& transaction) {
         transaction.expiration = trx_expiration;
         transaction.trx_id = trx.id();
         transaction.trx = trx;
      });
   }
   return ptrx;
} FC_CAPTURE_AND_RETHROW( (trx) ) }

operation_result database::apply_operation(transaction_evaluation_state& eval_state, const operation& op)
{
   assert("No registered evaluator for this operation." &&
          _operation_evaluators.size() > op.which() && _operation_evaluators[op.which()]);
   auto op_id = push_applied_operation( op );
   auto result =  _operation_evaluators[op.which()]->evaluate( eval_state, op, true );
   set_applied_operation_result( op_id, result );
   return result;
}
uint32_t database::push_applied_operation( const operation& op )
{
   _applied_ops.emplace_back(op);
   auto& oh = _applied_ops.back();
   oh.block_num    = _current_block_num;
   oh.trx_in_block = _current_trx_in_block;
   oh.op_in_trx    = _current_op_in_trx;
   oh.virtual_op   = _current_virtual_op++;
   return _applied_ops.size() - 1;
}
void database::set_applied_operation_result( uint32_t op_id, const operation_result& result )
{
   assert( op_id < _applied_ops.size() );
   _applied_ops[op_id].result = result;
}

const vector<operation_history_object>& database::get_applied_operations() const
{
   return _applied_ops;
}

const global_property_object& database::get_global_properties()const
{
   return get( global_property_id_type() );
}

const dynamic_global_property_object&database::get_dynamic_global_properties() const
{
   return get( dynamic_global_property_id_type() );
}
const fee_schedule_type&  database::current_fee_schedule()const
{
   return get_global_properties().parameters.current_fees;
}
time_point_sec database::head_block_time()const
{
   return get( dynamic_global_property_id_type() ).time;
}
uint32_t       database::head_block_num()const
{
   return get( dynamic_global_property_id_type() ).head_block_number;
}
block_id_type       database::head_block_id()const
{
   return get( dynamic_global_property_id_type() ).head_block_id;
}

block_id_type  database::get_block_id_for_num( uint32_t block_num )const
{ try {
   block_id_type lb; lb._hash[0] = htonl(block_num);
   auto itr = _block_id_to_block.lower_bound( lb );
   FC_ASSERT( itr.valid() && itr.key()._hash[0] == lb._hash[0] );
   return itr.key();
} FC_CAPTURE_AND_RETHROW( (block_num) ) }

optional<signed_block> database::fetch_block_by_id( const block_id_type& id )const
{
   auto b = _fork_db.fetch_block( id );
   if( !b )
      return _block_id_to_block.fetch_optional(id);
   return b->data;
}

optional<signed_block> database::fetch_block_by_number( uint32_t num )const
{
   auto results = _fork_db.fetch_block_by_number(num);
   if( results.size() == 1 )
      return results[0]->data;
   else
   {
      block_id_type lb; lb._hash[0] = htonl(num);
      auto itr = _block_id_to_block.lower_bound( lb );
      if( itr.valid() && itr.key()._hash[0] == lb._hash[0] )
         return itr.value();
   }
   return optional<signed_block>();
}

const signed_transaction& database::get_recent_transaction(const transaction_id_type& trx_id) const
{
   auto& index = get_index_type<transaction_index>().indices().get<by_trx_id>();
   auto itr = index.find(trx_id);
   FC_ASSERT(itr != index.end());
   return itr->trx;
}

const witness_object& database::validate_block_header(uint32_t skip, const signed_block& next_block)
{
   auto now = bts::chain::now();
   const auto& global_props = get_global_properties();
   FC_ASSERT( _pending_block.timestamp <= (now  + fc::seconds(1)), "", ("now",now)("pending",_pending_block.timestamp) );
   FC_ASSERT( _pending_block.previous == next_block.previous, "", ("pending.prev",_pending_block.previous)("next.prev",next_block.previous) );
   FC_ASSERT( _pending_block.timestamp <= next_block.timestamp, "", ("_pending_block.timestamp",_pending_block.timestamp)("next",next_block.timestamp)("blocknum",next_block.block_num()) );
   FC_ASSERT( _pending_block.timestamp.sec_since_epoch() % global_props.parameters.block_interval == 0 );
   const witness_object& witness = next_block.witness(*this);
   FC_ASSERT( secret_hash_type::hash(next_block.previous_secret) == witness.next_secret, "",
              ("previous_secret", next_block.previous_secret)("next_secret", witness.next_secret));
   if( !(skip&skip_delegate_signature) ) FC_ASSERT( next_block.validate_signee( witness.signing_key(*this).key() ) );
   auto expected_witness_num = (next_block.timestamp.sec_since_epoch() /
                                global_props.parameters.block_interval) % global_props.active_witnesses.size();
   FC_ASSERT( next_block.witness == global_props.active_witnesses[expected_witness_num] );

   return witness;
}

void database::update_signing_witness(const witness_object& signing_witness, const signed_block& new_block)
{
   const auto& core_asset = get( asset_id_type() );
   const auto& asset_data = core_asset.dynamic_asset_data_id(*this);

   // Slowly pay out income averaged over 1M blocks
   share_type pay = std::min(get_global_properties().parameters.witness_pay, asset_data.accumulated_fees);
   modify( asset_data, [&]( asset_dynamic_data_object& o ){ o.accumulated_fees -= pay; } );

   modify( signing_witness, [&]( witness_object& obj ){
           obj.last_secret = new_block.previous_secret;
           obj.next_secret = new_block.next_secret_hash;
           obj.accumulated_income += pay;
           });
}

void database::update_pending_block(const signed_block& next_block, uint8_t current_block_interval)
{
   _pending_block.timestamp = next_block.timestamp + current_block_interval;
   _pending_block.previous = next_block.id();
   auto old_pending_trx = std::move(_pending_block.transactions);
   _pending_block.transactions.clear();
   for( auto old_trx : old_pending_trx )
      push_transaction( old_trx );
}

void database::perform_chain_maintenance(const signed_block& next_block, const global_property_object& global_props)
{
   update_active_witnesses();
   update_active_delegates();

   auto new_block_interval = global_props.parameters.block_interval;

   // if block interval CHANGED during this block *THEN* we cannot simply
   // add the interval if we want to maintain the invariant that all timestamps are a multiple
   // of the interval.
   _pending_block.timestamp = next_block.timestamp + fc::seconds(new_block_interval);
   uint32_t r = _pending_block.timestamp.sec_since_epoch()%new_block_interval;
   if( !r )
   {
      _pending_block.timestamp -=  r;
      assert( (_pending_block.timestamp.sec_since_epoch() % new_block_interval)  == 0 );
   }

   auto next_maintenance_time = get<dynamic_global_property_object>(dynamic_global_property_id_type()).next_maintenance_time;
   auto maintenance_interval = get_global_properties().parameters.maintenance_interval;

   if( next_maintenance_time <= next_block.timestamp )
   {
      if( next_block.block_num() == 1 )
         next_maintenance_time = time_point_sec() +
               (((next_block.timestamp.sec_since_epoch() / maintenance_interval) + 1) * maintenance_interval);
      else
         next_maintenance_time += maintenance_interval;
      assert( next_maintenance_time > next_block.timestamp );
   }

   modify(get_dynamic_global_properties(), [next_maintenance_time](dynamic_global_property_object& d) {
      d.next_maintenance_time = next_maintenance_time;
   });
}

void database::create_block_summary(const signed_block& next_block)
{
   const auto& sum = create<block_summary_object>( [&](block_summary_object& p) {
         p.block_id = next_block.id();
         p.timestamp = next_block.timestamp;
   });
   FC_ASSERT( sum.id.instance() == next_block.block_num(), "", ("summary.id",sum.id)("next.block_num",next_block.block_num()) );
}

void database::clear_expired_transactions()
{
   //Look for expired transactions in the deduplication list, and remove them.
   //Transactions must have expired by at least two forking windows in order to be removed.
   auto& transaction_idx = static_cast<transaction_index&>(get_mutable_index(implementation_ids, impl_transaction_object_type));
   const auto& dedupe_index = transaction_idx.indices().get<by_expiration>();
   const auto& global_parameters = get_global_properties().parameters;
   auto forking_window_time = global_parameters.maximum_undo_history * global_parameters.block_interval;
   while( !dedupe_index.empty()
          && head_block_time() - dedupe_index.rbegin()->expiration >= fc::seconds(forking_window_time) )
      transaction_idx.remove(*dedupe_index.rbegin());
}

void database::clear_expired_proposals()
{
   const auto& proposal_expiration_index = get_index_type<proposal_index>().indices().get<by_expiration>();
   while( !proposal_expiration_index.empty() && proposal_expiration_index.begin()->expiration_time <= head_block_time() )
   {
      const proposal_object& proposal = *proposal_expiration_index.begin();
      processed_transaction result;
      try {
         if( proposal.is_authorized_to_execute(this) )
         {
            result = push_proposal(proposal);
            //TODO: Do something with result so plugins can process it.
            continue;
         }
      } catch( const fc::exception& e ) {
         elog("Failed to apply proposed transaction on its expiration. Deleting it.\n${proposal}\n${error}",
              ("proposal", proposal)("error", e.to_detail_string()));
      }
      remove(proposal);
   }
}

void database::clear_expired_orders()
{
   transaction_evaluation_state cancel_context(this, true);

   //Cancel expired limit orders
   auto& limit_index = get_index_type<limit_order_index>().indices().get<by_expiration>();
   while( !limit_index.empty() && limit_index.begin()->expiration <= head_block_time() )
   {
      const limit_order_object& order = *limit_index.begin();
      limit_order_cancel_operation canceler;
      canceler.fee_paying_account = order.seller;
      canceler.order = order.id;
      apply_operation(cancel_context, canceler);
   }

   //Cancel expired short orders
   auto& short_index = get_index_type<short_order_index>().indices().get<by_expiration>();
   while( !short_index.empty() && short_index.begin()->expiration <= head_block_time() )
   {
      const short_order_object& order = *short_index.begin();
      short_order_cancel_operation canceler;
      canceler.fee_paying_account = order.seller;
      canceler.order = order.id;
      apply_operation(cancel_context, canceler);
   }
}

} } // namespace bts::chain
