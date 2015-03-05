#include <bts/chain/types.hpp>
#include <bts/chain/time.hpp>
#include <bts/chain/database.hpp>
#include <fc/io/raw.hpp>
#include <fc/crypto/digest.hpp>
#include <fc/container/flat.hpp>

#include <bts/chain/key_object.hpp>
#include <bts/chain/account_object.hpp>
#include <bts/chain/asset_object.hpp>
#include <bts/chain/delegate_object.hpp>
#include <bts/chain/simple_index.hpp>
#include <bts/chain/flat_index.hpp>
#include <bts/chain/account_index.hpp>
#include <bts/chain/asset_index.hpp>

#include <bts/chain/transaction_evaluation_state.hpp>
#include <bts/chain/key_evaluator.hpp>
#include <bts/chain/account_evaluator.hpp>
#include <bts/chain/delegate_evaluator.hpp>
#include <bts/chain/asset_evaluator.hpp>
#include <bts/chain/transfer_evaluator.hpp>

namespace bts { namespace chain {

database::database()
{
   _operation_evaluators.resize(255);
   register_evaluator<key_create_evaluator>();
   register_evaluator<account_create_evaluator>();
   register_evaluator<account_update_evaluator>();
   register_evaluator<delegate_create_evaluator>();
   register_evaluator<delegate_update_evaluator>();
   register_evaluator<asset_create_evaluator>();
   register_evaluator<asset_issue_evaluator>();
   register_evaluator<transfer_evaluator>();

   _object_id_to_object = std::make_shared<db::level_map<object_id_type,packed_object>>();

   _index.resize(255);
   _index[protocol_ids].resize( 10 );
   _index[implementation_ids].resize( 10 );
   _index[meta_info_ids].resize( 10 );

   add_index<primary_index<asset_index> >();
   add_index<primary_index<account_index> >();
   add_index<primary_index<simple_index<key_object>> >();
   add_index<primary_index<simple_index<delegate_object>> >();

   add_index<primary_index<simple_index<global_property_object>> >();
   add_index<primary_index<simple_index<dynamic_global_property_object>> >();
   add_index<primary_index<simple_index<account_balance_object>> >();
   add_index<primary_index<simple_index<account_debt_object>> >();
   add_index<primary_index<simple_index<asset_dynamic_data_object>> >();
   add_index<primary_index<flat_index<delegate_vote_object>> >();

   push_undo_state();
}

database::~database(){}

void database::close()
{
   flush();

   _block_num_to_block.close();
   _block_id_to_num.close();
   _object_id_to_object->close();
}

const object* database::get_object( object_id_type id )const
{
   return get_index(id.space(),id.type()).get( id );
}

const index& database::get_index(uint8_t space_id, uint8_t type_id)const
{
   FC_ASSERT( _index.size() > space_id );
   FC_ASSERT( _index[space_id].size() > type_id );
   const auto& tmp = _index[space_id][type_id];
   FC_ASSERT( tmp );
   return *tmp;
}
index& database::get_index(uint8_t space_id, uint8_t type_id)
{
   FC_ASSERT( _index.size() > space_id );
   FC_ASSERT( _index[space_id].size() > type_id );
   const auto& tmp = _index[space_id][type_id];
   FC_ASSERT( tmp );
   return *tmp;
}

const account_index& database::get_account_index()const
{
   return dynamic_cast<const account_index&>( get_index<account_object>() );
}
account_index& database::get_account_index()
{
   return dynamic_cast<account_index&>( get_index<account_object>() );
}

const asset_index&   database::get_asset_index()const
{
   return dynamic_cast<const asset_index&>( get_index<asset_object>() );
}
asset_index&   database::get_asset_index()
{
   return dynamic_cast<asset_index&>( get_index<asset_object>() );
}

const asset_object*database::get_base_asset() const
{
   return get_asset_index().get(BTS_SYMBOL);
}

void database::flush()
{
   for( auto& space : _index )
      for( const unique_ptr<index>& type_index : space )
         if( type_index )
            type_index->inspect_all_objects([&] (const object* object) {
               _object_id_to_object->store(object->id, type_index->pack(object));
            });
}

void database::open( const fc::path& data_dir, const genesis_allocation& initial_allocation )
{ try {
   ilog("Open database in ${d}", ("d", data_dir));

   _block_num_to_block.open( data_dir / "database" / "block_num_to_block" );
   _block_id_to_num.open( data_dir / "database" / "block_id_to_num" );
   _object_id_to_object->open( data_dir / "database" / "objects" );

   for( auto& space : _index )
   {
      for( auto& type_index : space )
      {
         if( type_index )
         {
            type_index->open( _object_id_to_object );
         }
      }
   }

   if( !get_global_properties() )
      init_genesis(initial_allocation);
   assert(get_global_properties());
} FC_CAPTURE_AND_RETHROW( (data_dir) ) }

void database::init_genesis(const genesis_allocation& initial_allocation)
{
   _save_undo = false;

   fc::ecc::private_key genesis_private_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("genesis")));
   const key_object* genesis_key =
      create<key_object>( [&genesis_private_key](key_object* k) {
         k->key_data = public_key_type(genesis_private_key.get_public_key());
      });
   const account_balance_object* genesis_balance =
      create<account_balance_object>( [&](account_balance_object* b){
         b->add_balance(asset(BTS_INITIAL_SUPPLY));
      });
   const account_object* genesis_account =
      create<account_object>( [&](account_object* n) {
         n->owner.add_authority(genesis_key->get_id(), 1);
         n->owner.weight_threshold = 1;
         n->active = n->owner;
         n->voting_key = genesis_key->id;
         n->memo_key = genesis_key->id;
         n->balances = genesis_balance->id;
      });

   vector<delegate_id_type> init_delegates;

   for( int i = 0; i < BTS_MIN_DELEGATE_COUNT; ++i )
   {
      const account_balance_object* balance_obj =
         create<account_balance_object>( [&](account_balance_object* b){
            (void)b;
         });
      const account_object* delegate_account =
         create<account_object>( [&](account_object* a) {
            a->active = a->owner = genesis_account->owner;
            a->name = string("init") + fc::to_string(i);
            a->balances = balance_obj->id;
         });
      const delegate_vote_object* vote =
         create<delegate_vote_object>( [&](delegate_vote_object* v) {
            // Nothing to do here...
            (void)v;
         });
      const delegate_object* init_delegate = create<delegate_object>( [&](delegate_object* d) {
         d->delegate_account = delegate_account->id;
         d->signing_key = genesis_key->id;
         secret_hash_type::encoder enc;
         fc::raw::pack( enc, genesis_private_key );
         fc::raw::pack( enc, d->last_secret );
         d->next_secret = secret_hash_type::hash(enc.result());
         d->vote = vote->id;
      });
      init_delegates.push_back(init_delegate->id);
   }

   const global_property_object* properties =
      create<global_property_object>( [&](global_property_object* p) {
         p->active_delegates = init_delegates;
      });
   (void)properties;

   create<dynamic_global_property_object>( [&](dynamic_global_property_object* p) {
      });

   const asset_dynamic_data_object* dyn_asset =
      create<asset_dynamic_data_object>( [&]( asset_dynamic_data_object* a ) {
         a->current_supply = BTS_INITIAL_SUPPLY;
      });

   const asset_object* core_asset =
     create<asset_object>( [&]( asset_object* a ) {
         a->symbol = BTS_SYMBOL;
         a->max_supply = BTS_INITIAL_SUPPLY;
         a->flags = 0;
         a->issuer_permissions = 0;
         a->issuer = genesis_account->id;
         a->core_exchange_rate.base.amount = 1;
         a->core_exchange_rate.base.asset_id = 0;
         a->core_exchange_rate.quote.amount = 1;
         a->core_exchange_rate.quote.asset_id = 0;
         a->dynamic_asset_data_id = dyn_asset->id;
      });
   assert( asset_id_type(core_asset->id) == asset().asset_id );
   assert( genesis_balance->get_balance(core_asset->id) == asset(dyn_asset->current_supply) );
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
         trx.operations.emplace_back(key_create_operation({genesis_account->id, asset(), handout.first}));
         object_id_type key_id(relative_protocol_ids, 0, 0);
         authority account_authority;
         account_authority.add_authority(key_id_type(key_id), 1);
         trx.operations.emplace_back(account_create_operation({
                                                                 genesis_account->id,
                                                                 asset(),
                                                                 string(),
                                                                 account_authority,
                                                                 account_authority,
                                                                 key_id,
                                                                 key_id
                                                              }));
         trx.validate();
         auto ptrx = apply_transaction(trx, ~0);
         trx = signed_transaction();
         account_id_type account_id(ptrx.operation_results.back());
         trx.operations.emplace_back(transfer_operation({
                                                           genesis_account->id,
                                                           account_id,
                                                           amount,
                                                           asset(),
                                                           vector<char>()
                                                        }));
         trx.validate();
         apply_transaction(trx, ~0);
      }

      if( genesis_balance->get_balance(asset_id_type()).amount > 0 )
      {
         asset leftovers = genesis_balance->get_balance(asset_id_type());
         modify(genesis_balance, [](account_balance_object* b) {
            b->balances.clear();
         });
         modify(core_asset->dynamic_asset_data_id(*this), [&leftovers](asset_dynamic_data_object* d) {
            d->accumulated_fees += leftovers.amount;
         });
      }

      fc::microseconds duration = fc::time_point::now() - start_time;
      ilog("Finished allocating to ${n} accounts in ${t} milliseconds.",
           ("n", initial_allocation.size())("t", duration.count() / 1000));
   }

   push_undo_state();
   _save_undo = true;
   _pending_block.block_num = 1;
}

void database::reindex()
{
   auto itr = _block_num_to_block.begin();
   while( itr.valid() )
   {
      apply_block( itr.value(), skip_delegate_signature |
                                skip_transaction_signatures |
                                skip_undo_block |
                                skip_undo_transaction |
                                skip_transaction_dupe_check );
      ++itr;
   }
}


asset database::current_delegate_registration_fee()const
{
   return asset();
}

void database::save_undo( const object* obj )
{
   if( !_save_undo ) return;
   FC_ASSERT( obj );
   auto id = obj->id;
   auto current_undo = _undo_state.back().old_values.find(id);
   if( current_undo == _undo_state.back().old_values.end() )
   {
      _undo_state.back().old_values[id] = get_index(obj->id.space(),obj->id.type()).pack( obj );
   }
}


void database::pop_undo_state()
{
   _undo_state.pop_back();
}
void database::push_undo_state()
{
   _undo_state.push_back( undo_state() );
   if( _undo_state.size() > 2 )
   {
      const auto& prev = _undo_state[_undo_state.size()-2];
      auto& cur = _undo_state.back();
      for( auto new_id : prev.new_ids )
      {
         const object* obj = get_object( new_id );
         assert( obj != nullptr );
         cur.old_values[obj->id] = get_index(obj->id.space(),obj->id.type()).pack(obj);
      }
   }
}

void database::undo()
{
   _save_undo = false;
   for( auto item : _undo_state.back().old_values )
   {
      auto& index = get_index( item.first.space(), item.first.type() );
      if( item.second.is_null() )
      {
         index.remove( item.first );
      }
      else
      {
         const object* obj = index.get( item.first );
         if( obj )
         {
            index.modify( obj, [&](object* o){
                          index.unpack( o, item.second );
                         });
         }
      }
   }
   for( const auto& old_index_meta : _undo_state.back().old_index_meta_objects )
   {
      get_index( old_index_meta.first.space(), old_index_meta.first.type() ).set_meta_object( old_index_meta.second );
   }
   _undo_state.pop_back();
   _save_undo = true;
}


void database::apply_block( const signed_block& next_block, uint32_t skip )
{ try {
   auto now = bts::chain::now();
   FC_ASSERT( _pending_block.timestamp <= (now  + fc::seconds(1)), "", ("now",now)("pending",_pending_block.timestamp) );
   FC_ASSERT( _pending_block.block_num = next_block.block_num );
   FC_ASSERT( _pending_block.previous == next_block.previous );
   FC_ASSERT( _pending_block.timestamp <= next_block.timestamp );
   FC_ASSERT( _pending_block.timestamp.sec_since_epoch() % get_global_properties()->block_interval == 0 );
   const delegate_object* del = next_block.delegate_id(*this);
   FC_ASSERT( del != nullptr );
   FC_ASSERT( secret_hash_type::hash(next_block.previous_secret) == del->next_secret );

   auto expected_delegate_num = (next_block.timestamp.sec_since_epoch() / get_global_properties()->block_interval)%get_global_properties()->active_delegates.size();

   if( !(skip&skip_delegate_signature) ) FC_ASSERT( next_block.validate_signee( del->signing_key(*this)->key() ) );

   FC_ASSERT( next_block.delegate_id == get_global_properties()->active_delegates[expected_delegate_num] );

   for( const auto& trx : next_block.transactions )
   {
      /* We do not need to push the undo state for each transaction
       * because they either all apply and are valid or the
       * entire block fails to apply.  We only need an "undo" state
       * for transactions when validating broadcast transactions or
       * when building a block.
       */
      apply_transaction( trx, skip );
   }
   update_global_dynamic_data( next_block );

   modify( del, [&]( delegate_object* obj ){
           obj->last_secret = next_block.previous_secret;
           obj->next_secret = next_block.next_secret_hash;
           });


   if( 0 == (next_block.block_num % get_global_properties()->active_delegates.size()) )
   {
      update_active_delegates();
   }

   auto current_block_interval = get_global_properties()->block_interval;
   if( next_block.block_num % get_global_properties()->maintenance_interval == 0 )
   {
      update_global_properties();

      // if block interval CHANGED durring this block *THEN* we cannot simply
      // add the interval if we want to maintain the invariant that all timestamps are a multiple
      // of the interval.
      _pending_block.timestamp = next_block.timestamp + fc::seconds(current_block_interval);
      uint32_t r = _pending_block.timestamp.sec_since_epoch()%current_block_interval;
      if( !r )
      {
         _pending_block.timestamp += current_block_interval - r;
         assert( (_pending_block.timestamp.sec_since_epoch() % current_block_interval)  == 0 );
      }
   }
   else
   {
      _pending_block.timestamp = next_block.timestamp + current_block_interval;
   }

   _pending_block.block_num++;
   _pending_block.previous = next_block.id();
   auto old_pending_trx = std::move(_pending_block.transactions);
   _pending_block.transactions.clear();
   for( auto old_trx : old_pending_trx )
      push_transaction( old_trx );
} FC_CAPTURE_AND_RETHROW( (next_block.block_num)(skip) )  }

time_point   database::get_next_generation_time( delegate_id_type del_id )const
{
   auto gp = get_global_properties();
   auto now = bts::chain::now();
   const auto& active_del = gp->active_delegates;
   const auto& interval   = gp->block_interval;
   auto delegate_slot = ((now.sec_since_epoch()+1) /interval);
   for( uint32_t i = 0; i < active_del.size(); ++i )
   {
      if( active_del[ delegate_slot % active_del.size()] == del_id )
         return time_point_sec() + fc::seconds( delegate_slot * interval );
      ++delegate_slot;
   }
   FC_ASSERT( !"Not an Active Delegate" );
}

signed_block database::generate_block( const fc::ecc::private_key& delegate_key,
                                       delegate_id_type del_id )
{
   auto del_obj = del_id(*this);
   FC_ASSERT( del_obj );
   FC_ASSERT( del_obj->signing_key(*this)->key() == delegate_key.get_public_key() );
   _pending_block.timestamp = get_next_generation_time( del_id );


   secret_hash_type::encoder last_enc;
   fc::raw::pack( last_enc, delegate_key );
   fc::raw::pack( last_enc, del_obj->last_secret );
   _pending_block.previous_secret = last_enc.result();

   secret_hash_type::encoder next_enc;
   fc::raw::pack( next_enc, delegate_key );
   fc::raw::pack( next_enc, _pending_block.previous_secret );
   _pending_block.next_secret_hash = secret_hash_type::hash(next_enc.result());

   _pending_block.delegate_id = del_id;
   _pending_block.sign( delegate_key );
   signed_block tmp = std::move(_pending_block);
   push_block( signed_block(_pending_block) );
   return tmp;
}

void database::update_active_delegates()
{
    vector<delegate_id_type> ids( get_index<delegate_object>().size() );
    for( uint32_t i = 0; i < ids.size(); ++i ) ids[i] = delegate_id_type(i);
    std::sort( ids.begin(), ids.end(), [&]( delegate_id_type a,delegate_id_type b )->bool {
       return a(*this)->vote(*this)->total_votes >
              b(*this)->vote(*this)->total_votes;
    });

    uint64_t base_threshold = ids[9](*this)->vote(*this)->total_votes.value;
    uint64_t threshold =  (base_threshold / 100) * 75;
    uint32_t i = 10;

    for( ; i < ids.size(); ++i )
    {
       if( ids[i](*this)->vote(*this)->total_votes < threshold ) break;
       threshold = (base_threshold / (100) ) * (75 + i/(ids.size()/4));
    }
    ids.resize( i );

    // shuffle ids
    auto randvalue = get(dynamic_global_property_id_type(0))->random;
    for( uint32_t i = 0; i < ids.size(); ++i )
    {
       const auto rands_per_hash = sizeof(secret_hash_type) / sizeof(randvalue._hash[0]);
       std::swap( ids[i], ids[ i + (randvalue._hash[i%rands_per_hash] % (ids.size()-i))] );
       if( i % rands_per_hash == (rands_per_hash-1) )
          randvalue = secret_hash_type::hash( randvalue );
    }

    modify( get_global_properties(), [&]( global_property_object* gp ){
       gp->active_delegates = std::move(ids);
    });
}

void database::update_global_properties()
{
   global_property_object tmp;
   vector<delegate_id_type> ids = get_global_properties()->active_delegates;
   std::nth_element( ids.begin(), ids.begin() + ids.size()/2, ids.end(),
                     [&]( delegate_id_type a, delegate_id_type b )->bool {
                          return a(*this)->block_interval_sec < b(*this)->block_interval_sec;
                     });
   tmp.block_interval = ids[ids.size()/2](*this)->block_interval_sec;
   std::nth_element( ids.begin(), ids.begin() + ids.size()/2, ids.end(),
                     [&]( delegate_id_type a, delegate_id_type b )->bool {
                          return a(*this)->max_block_size < b(*this)->max_block_size;
                     });
   tmp.maximum_block_size = ids[ids.size()/2](*this)->max_block_size;
   std::nth_element( ids.begin(), ids.begin() + ids.size()/2, ids.end(),
                     [&]( delegate_id_type a, delegate_id_type b )->bool {
                          return a(*this)->max_transaction_size < b(*this)->max_transaction_size;
                     });
   tmp.maximum_transaction_size = ids[ids.size()/2](*this)->max_transaction_size;
   std::nth_element( ids.begin(), ids.begin() + ids.size()/2, ids.end(),
                     [&]( delegate_id_type a, delegate_id_type b )->bool {
                          return a(*this)->max_sec_until_expiration < b(*this)->max_sec_until_expiration;
                     });
   tmp.maximum_time_until_expiration = ids[ids.size()/2](*this)->max_sec_until_expiration;

   for( uint32_t f = 0; f < tmp.current_fees.size(); ++f )
   {
      std::nth_element( ids.begin(), ids.begin() + ids.size()/2, ids.end(),
                        [&]( delegate_id_type a, delegate_id_type b )->bool {
                             return a(*this)->fee_schedule.at(f) < b(*this)->fee_schedule.at(f);
                        });
      tmp.current_fees.at(f)  = ids[ids.size()/2](*this)->fee_schedule.at(f);
   }
}

void database::update_global_dynamic_data( const signed_block& b )
{
   modify( dynamic_global_property_id_type(0)(*this), [&]( dynamic_global_property_object* dgp ){
      secret_hash_type::encoder enc;
      fc::raw::pack( enc, dgp->random );
      fc::raw::pack( enc, b.previous_secret );
      dgp->random = enc.result();
      dgp->head_block_number = b.block_num;
      dgp->time = b.timestamp;
      dgp->current_delegate = b.delegate_id;
   });
}

/**
 *  Push block "may fail" in which case every partial change is unwound.  After
 *  push block is successful the block is appended to the chain database on disk.
 */
void database::push_block( const signed_block& new_block, uint32_t skip )
{ try {
   auto old_save_undo = _save_undo;
   if( skip & skip_undo_block ) _save_undo = false;
   pop_pending_block();
   { // logically connect pop/push of pending block
      if( !(skip&skip_undo_block) )push_undo_state();
      optional<fc::exception> except;
      try {
        apply_block( new_block, skip );
        _block_num_to_block.store( new_block.block_num, new_block );
        _block_id_to_num.store( new_block.id(), new_block.block_num );
      }
      catch ( const fc::exception& e ) { except = e; }
      _save_undo = old_save_undo;
      if( except )
      {
         if( !(skip&skip_undo_block) ) undo();
         throw *except;
      }
   }
   push_pending_block();
} FC_CAPTURE_AND_RETHROW( (new_block) ) }

/**
 *  Attempts to push the transaction into the pending queue
 */
bool database::push_transaction( const signed_transaction& trx, uint32_t skip )
{
   if( !(skip&skip_undo_transaction) ) push_undo_state(); // trx undo
   optional<fc::exception> except;
   try {
      apply_transaction( trx, skip );
      _pending_block.transactions.push_back(trx);
      if( !(skip&skip_undo_transaction) ) pop_undo_state(); // everything was OK.
      return true;
   } catch ( const fc::exception& e ) { except = e; }
   if( except )
   {
      // wlog( "${e}", ("e",except->to_detail_string() ) );
      if( !(skip&skip_undo_transaction) ) undo();
      throw *except;
   }
   return false;
}

void database::push_pending_block()
{
   block old_pending = std::move( _pending_block );
   _pending_block.transactions.clear();
   push_undo_state();
   for( auto trx : old_pending.transactions )
   {
      push_transaction( trx );
   }
}

void database::pop_pending_block()
{
   undo();
}

processed_transaction database::apply_transaction( const signed_transaction& trx, uint32_t skip )
{ try {
   trx.validate();
   transaction_evaluation_state eval_state(this, skip&skip_transaction_signatures );
   if( !(skip & skip_transaction_signatures) )
   {
      for( auto sig : trx.signatures )
      {
         eval_state.signed_by.insert( fc::ecc::public_key( sig, trx.digest() ) );
      }
   }
   eval_state.operation_results.reserve( trx.operations.size() );

   processed_transaction ptrx(trx);
   for( auto op : ptrx.operations )
   {
      auto r = _operation_evaluators[op.which()]->evaluate( eval_state, op, true );
      eval_state.operation_results.push_back(r);
   }
   ptrx.operation_results = std::move( eval_state.operation_results );
   return ptrx;
} FC_CAPTURE_AND_RETHROW( (trx) ) }

const global_property_object* database::get_global_properties()const
{
   return get<global_property_object>( object_id_type( global_property_object::space_id, global_property_object::type_id, 0 ) );
}
const fee_schedule_type&  database::current_fee_schedule()const
{
   return get_global_properties()->current_fees;
}
} } // namespace bts::chain
