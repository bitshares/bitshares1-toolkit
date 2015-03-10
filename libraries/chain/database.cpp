#include <bts/chain/types.hpp>
#include <bts/chain/time.hpp>
#include <bts/chain/database.hpp>

#include <bts/chain/key_object.hpp>
#include <bts/chain/account_object.hpp>
#include <bts/chain/asset_object.hpp>
#include <bts/chain/delegate_object.hpp>
#include <bts/chain/block_summary_object.hpp>
#include <bts/chain/simple_index.hpp>
#include <bts/chain/flat_index.hpp>

#include <bts/chain/transaction_evaluation_state.hpp>
#include <bts/chain/key_evaluator.hpp>
#include <bts/chain/account_evaluator.hpp>
#include <bts/chain/delegate_evaluator.hpp>
#include <bts/chain/asset_evaluator.hpp>
#include <bts/chain/transaction_object.hpp>
#include <bts/chain/transfer_evaluator.hpp>

#include <fc/io/raw.hpp>
#include <fc/crypto/digest.hpp>
#include <fc/container/flat.hpp>
#include <fc/uint128.hpp>

namespace bts { namespace chain {
template<typename T>
struct restore_on_scope_exit
{
   restore_on_scope_exit( T& v)
   :original_copy(v),value(v){}
   ~restore_on_scope_exit(){ value = original_copy; }
   T   original_copy;
   T&  value;
};

template<typename T>
restore_on_scope_exit<T> make_restore_on_exit( T& v ) { return restore_on_scope_exit<T>(v); }

database::database()
:_undo_db(*this)
{
   _index.resize(255);

   _undo_db.enable();
   _operation_evaluators.resize(255);
   register_evaluator<key_create_evaluator>();
   register_evaluator<account_create_evaluator>();
   register_evaluator<account_update_evaluator>();
   register_evaluator<delegate_create_evaluator>();
   register_evaluator<delegate_update_evaluator>();
   register_evaluator<asset_create_evaluator>();
   register_evaluator<asset_issue_evaluator>();
   register_evaluator<transfer_evaluator>();

   _object_id_to_object = std::make_shared<db::level_map<object_id_type,vector<char>>>();

   initialize_indexes();
}

database::~database(){
   if( _pending_block_session )
      _pending_block_session->commit();
}

void database::close()
{
   _pending_block_session.reset();
   flush();

   if( _block_id_to_block.is_open() )
      _block_id_to_block.close();
   if( _object_id_to_object->is_open() )
      _object_id_to_object->close();
   _fork_db.reset();
}

const object* database::find_object( object_id_type id )const
{
   return get_index(id.space(),id.type()).find( id );
}
const object& database::get_object( object_id_type id )const
{
   return get_index(id.space(),id.type()).get( id );
}

const index& database::get_index(uint8_t space_id, uint8_t type_id)const
{
   FC_ASSERT( _index.size() > space_id, "", ("space_id",space_id)("type_id",type_id)("index.size",_index.size()) );
   const auto& tmp = _index[space_id][type_id];
   FC_ASSERT( tmp );
   return *tmp;
}
index& database::get_index(uint8_t space_id, uint8_t type_id)
{
   FC_ASSERT( _index.size() > space_id, "", ("space_id",space_id)("type_id",type_id)("index.size",_index.size()) );
   FC_ASSERT( _index[space_id].size() > type_id , "", ("space_id",space_id)("type_id",type_id)("index[space_id].size",_index[space_id].size()) );
   const auto& idx = _index[space_id][type_id];
   FC_ASSERT( idx, "", ("space",space_id)("type",type_id) );
   return *idx;
}

const asset_object& database::get_core_asset() const
{
   return get(asset_id_type());
}

void database::flush()
{
   if( !_object_id_to_object->is_open() )
      return;

   vector<object_id_type> next_ids;
   for( auto& space : _index )
      for( const unique_ptr<index>& type_index : space )
         if( type_index )
         {
            type_index->inspect_all_objects([&] (const object& obj) {
               _object_id_to_object->store(obj.id, obj.pack());
            });
            next_ids.push_back( type_index->get_next_id() );
         }
   _object_id_to_object->store( object_id_type(), fc::raw::pack(next_ids) );
}

void database::wipe(bool include_blocks)
{
   close();
   fc::remove_all(_data_dir / "database" / "objects");
   if( include_blocks )
      fc::remove_all(_data_dir / "database" / "block_id_to_block" );
}

void database::open( const fc::path& data_dir, const genesis_allocation& initial_allocation )
{ try {
   ilog("Open database in ${d}", ("d", data_dir));

   _block_id_to_block.open( data_dir / "database" / "block_num_to_block" );
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
   try {
   auto next_ids = fc::raw::unpack<vector<object_id_type>>( _object_id_to_object->fetch( object_id_type() ) );
   for( auto id : next_ids )
      get_index( id ).set_next_id( id );
   }
   catch ( const fc::exception& e )
   {
      wlog( "unable to fetch next ids, must be new database" );
   }

   if( !find(global_property_id_type()) )
      init_genesis(initial_allocation);

   _pending_block.previous  = head_block_id();
   _pending_block.timestamp = head_block_time();

   auto last_block_itr = _block_id_to_block.last();
   if( last_block_itr.valid() )
      _fork_db.start_block( last_block_itr.value() );

   _data_dir = data_dir;
} FC_CAPTURE_AND_RETHROW( (data_dir) ) }

void database::initialize_indexes()
{
   _index.clear();
   _index.resize(255);
   _index[protocol_ids].resize( 10 );
   _index[implementation_ids].resize( 10 );
   _index[meta_info_ids].resize( 10 );

   add_index< primary_index< asset_index> >();
   add_index< primary_index< account_index> >();
   add_index< primary_index< transaction_index> >();
   add_index< primary_index< simple_index<key_object>> >();
   add_index< primary_index< simple_index<delegate_object>> >();

   add_index< primary_index< simple_index< global_property_object         >> >();
   add_index< primary_index< simple_index< dynamic_global_property_object >> >();
   add_index< primary_index< simple_index< account_balance_object         >> >();
   add_index< primary_index< simple_index< account_debt_object            >> >();
   add_index< primary_index< simple_index< asset_dynamic_data_object      >> >();
   add_index< primary_index< flat_index<   delegate_vote_object           >> >();
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
      const delegate_vote_object& vote =
         create<delegate_vote_object>( [&](delegate_vote_object& v) {
            // Nothing to do here...
            (void)v;
         });
      const delegate_object& init_delegate = create<delegate_object>( [&](delegate_object& d) {
         d.delegate_account = delegate_account.id;
         d.signing_key = genesis_key.id;
         secret_hash_type::encoder enc;
         fc::raw::pack( enc, genesis_private_key );
         fc::raw::pack( enc, d.last_secret );
         d.next_secret = secret_hash_type::hash(enc.result());
         d.vote = vote.id;
      });
      init_delegates.push_back(init_delegate.id);
   }
   create<block_summary_object>( [&](block_summary_object& p) {
           /** default block 0 */
   });

   const global_property_object& properties =
      create<global_property_object>( [&](global_property_object& p) {
         p.active_delegates = init_delegates;
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
         trx.operations.emplace_back(account_create_operation({
                                                                 genesis_account.id,
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
                                skip_transaction_dupe_check );
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
   auto now = bts::chain::now();
   FC_ASSERT( _pending_block.timestamp <= (now  + fc::seconds(1)), "", ("now",now)("pending",_pending_block.timestamp) );
   FC_ASSERT( _pending_block.previous == next_block.previous, "", ("pending.prev",_pending_block.previous)("next.prev",next_block.previous) );
   FC_ASSERT( _pending_block.timestamp <= next_block.timestamp, "", ("_pending_block.timestamp",_pending_block.timestamp)("next",next_block.timestamp)("blocknum",next_block.block_num()) );
   FC_ASSERT( _pending_block.timestamp.sec_since_epoch() % get_global_properties().block_interval == 0 );
   const delegate_object& del = next_block.delegate_id(*this);
   FC_ASSERT( secret_hash_type::hash(next_block.previous_secret) == del.next_secret, "",
              ("previous_secret", next_block.previous_secret)("next_secret", del.next_secret));

   const auto& global_props = get_global_properties();

   auto expected_delegate_num = (next_block.timestamp.sec_since_epoch() / global_props.block_interval)%global_props.active_delegates.size();

   if( !(skip&skip_delegate_signature) ) FC_ASSERT( next_block.validate_signee( del.signing_key(*this).key() ) );

   FC_ASSERT( next_block.delegate_id == global_props.active_delegates[expected_delegate_num] );

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

   const auto& core_asset = get( asset_id_type() );
   const auto& asset_data = core_asset.dynamic_asset_data_id(*this);

   // Slowly pay out income averaged over 1M blocks
   uint64_t pay = asset_data.accumulated_fees.value / (1024*1024); // close enough to 1M but more effecient
   pay *= del.pay_rate;
   pay /= 100;
   modify( asset_data, [&]( asset_dynamic_data_object& o ){ o.accumulated_fees -= pay; } );

   modify( del, [&]( delegate_object& obj ){
           obj.last_secret = next_block.previous_secret;
           obj.next_secret = next_block.next_secret_hash;
           obj.accumulated_income += pay;
           });


   if( 0 == (next_block.block_num() % global_props.active_delegates.size()) )
   {
      update_active_delegates();
   }

   auto current_block_interval = global_props.block_interval;
   if( next_block.block_num() % (global_props.maintenance_interval / current_block_interval) == 0 )
   {
      update_global_properties();

      auto new_block_interval = global_props.block_interval;
     // wdump( (current_block_interval)(new_block_interval) );

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
   }
   else
   {
      _pending_block.timestamp = next_block.timestamp + current_block_interval;
   }

   _pending_block.previous = next_block.id();

   const auto& sum = create<block_summary_object>( [&](block_summary_object& p) {
              p.block_id = _pending_block.previous;
   });
   FC_ASSERT( sum.id.instance() == next_block.block_num(), "", ("sim->id",sum.id)("next.block_num",next_block.block_num()) );

   auto old_pending_trx = std::move(_pending_block.transactions);
   _pending_block.transactions.clear();
   for( auto old_trx : old_pending_trx )
      push_transaction( old_trx );
} FC_CAPTURE_AND_RETHROW( (next_block.block_num())(skip) )  }

time_point database::get_next_generation_time( delegate_id_type del_id )const
{
   const auto& gp = get_global_properties();
   auto now = bts::chain::now();
   const auto& active_del = gp.active_delegates;
   const auto& interval   = gp.block_interval;
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
                                       delegate_id_type del_id, uint32_t  skip )
{
   const auto& del_obj = del_id(*this);

   if( !(skip & skip_delegate_signature) )
      FC_ASSERT( del_obj.signing_key(*this).key() == delegate_key.get_public_key() );

   _pending_block.timestamp = get_next_generation_time( del_id );

   secret_hash_type::encoder last_enc;
   fc::raw::pack( last_enc, delegate_key );
   fc::raw::pack( last_enc, del_obj.last_secret );
   _pending_block.previous_secret = last_enc.result();

   secret_hash_type::encoder next_enc;
   fc::raw::pack( next_enc, delegate_key );
   fc::raw::pack( next_enc, _pending_block.previous_secret );
   _pending_block.next_secret_hash = secret_hash_type::hash(next_enc.result());

   _pending_block.delegate_id = del_id;
   if( !(skip & skip_delegate_signature) ) _pending_block.sign( delegate_key );
   //This line used to std::move(_pending_block) but this is unsafe as _pending_block is later referenced without being
   //reinitialized. Future optimization could be to move it, then reinitialize it with the values we need to preserve.
   signed_block tmp = _pending_block;
   _pending_block.transactions.clear();
   push_block( tmp, skip );
   return tmp;
}


void database::update_active_delegates()
{
    vector<delegate_id_type> ids( dynamic_cast<simple_index<delegate_object>&>(get_index<delegate_object>()).size() );
    for( uint32_t i = 0; i < ids.size(); ++i ) ids[i] = delegate_id_type(i);
    std::sort( ids.begin(), ids.end(), [&]( delegate_id_type a,delegate_id_type b )->bool {
       return a(*this).vote(*this).total_votes >
              b(*this).vote(*this).total_votes;
    });

    uint64_t base_threshold = ids[9](*this).vote(*this).total_votes.value;
    uint64_t threshold =  (base_threshold / 100) * 75;
    uint32_t i = 10;

    for( ; i < ids.size(); ++i )
    {
       if( ids[i](*this).vote(*this).total_votes < threshold ) break;
       threshold = (base_threshold / (100) ) * (75 + i/(ids.size()/4));
    }
    ids.resize( i );

    // shuffle ids
    auto randvalue = dynamic_global_property_id_type()(*this).random;
    for( uint32_t i = 0; i < ids.size(); ++i )
    {
       const auto rands_per_hash = sizeof(secret_hash_type) / sizeof(randvalue._hash[0]);
       std::swap( ids[i], ids[ i + (randvalue._hash[i%rands_per_hash] % (ids.size()-i))] );
       if( i % rands_per_hash == (rands_per_hash-1) )
          randvalue = secret_hash_type::hash( randvalue );
    }

    modify( get_global_properties(), [&]( global_property_object& gp ){
       gp.active_delegates = std::move(ids);
    });
}

void database::update_global_properties()
{
   global_property_object tmp;
   vector<delegate_id_type> ids = get_global_properties().active_delegates;
   std::nth_element( ids.begin(), ids.begin() + ids.size()/2, ids.end(),
                     [&]( delegate_id_type a, delegate_id_type b )->bool {
                          return a(*this).block_interval_sec < b(*this).block_interval_sec;
                     });
   tmp.block_interval = ids[ids.size()/2](*this).block_interval_sec;
   std::nth_element( ids.begin(), ids.begin() + ids.size()/2, ids.end(),
                     [&]( delegate_id_type a, delegate_id_type b )->bool {
                          return a(*this).max_block_size < b(*this).max_block_size;
                     });
   tmp.maximum_block_size = ids[ids.size()/2](*this).max_block_size;
   std::nth_element( ids.begin(), ids.begin() + ids.size()/2, ids.end(),
                     [&]( delegate_id_type a, delegate_id_type b )->bool {
                          return a(*this).max_transaction_size < b(*this).max_transaction_size;
                     });
   tmp.maximum_transaction_size = ids[ids.size()/2](*this).max_transaction_size;
   std::nth_element( ids.begin(), ids.begin() + ids.size()/2, ids.end(),
                     [&]( delegate_id_type a, delegate_id_type b )->bool {
                          return a(*this).max_sec_until_expiration < b(*this).max_sec_until_expiration;
                     });
   tmp.maximum_time_until_expiration = ids[ids.size()/2](*this).max_sec_until_expiration;

   for( uint32_t f = 0; f < tmp.current_fees.size(); ++f )
   {
      std::nth_element( ids.begin(), ids.begin() + ids.size()/2, ids.end(),
                        [&]( delegate_id_type a, delegate_id_type b )->bool {
                             return a(*this).fee_schedule.at(f) < b(*this).fee_schedule.at(f);
                        });
      tmp.current_fees.at(f)  = ids[ids.size()/2](*this).fee_schedule.at(f);
   }
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
      dgp.current_delegate = b.delegate_id;
   });
}

void database::pop_block()
{ try {
   _pending_block_session.reset();
   _block_id_to_block.remove( _pending_block.previous );
   _undo_db.pop_commit();
   _pending_block.previous  = head_block_id();
   _pending_block.timestamp = head_block_time();
   _fork_db.pop_block();
   } FC_CAPTURE_AND_RETHROW() }

void database::clear_pending()
{ try {
   _pending_block.transactions.clear();
   _pending_block_session.reset();
} FC_CAPTURE_AND_RETHROW() }

/**
 *  Push block "may fail" in which case every partial change is unwound.  After
 *  push block is successful the block is appended to the chain database on disk.
 */
void database::push_block( const signed_block& new_block, uint32_t skip )
{ try {
   // wdump( (new_block.id())(new_block.previous) );
   if( !(skip&skip_fork_db) )
   {
      auto new_head = _fork_db.push_block( new_block );
      //If the head block from the longest chain does not build off of the current head, we need to switch forks.
      if( new_head->data.previous != head_block_id() )
      {
         wlog( "head on new fork: block ${b} prev ${p}  ", ("b",new_block.id())("p",new_block.previous) );
         if( new_head->data.block_num() >= _pending_block.block_num() )
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
                  apply_block( (*ritr)->data, skip );
                  _block_id_to_block.store( new_block.id(), (*ritr)->data );
                }
                catch ( const fc::exception& e ) { except = e; }
                if( except )
                {
                   edump( ("oops!")((*ritr)->id) );
                   // undo();
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
                      //push_undo_state();
                      apply_block( (*ritr)->data, skip );
                      _block_id_to_block.store( new_block.id(), (*ritr)->data );
                   }
                   throw *except;
                }
            }
         }
         ilog("done");
         return;
      }
   }

   // If there is a pending block session, then the database state is dirty with pending transactions.
   // Drop the pending session to reset the database to a clean head block state.
   _pending_block_session.reset();

   auto session = _undo_db.start_undo_session();
   apply_block( new_block, skip );
   _block_id_to_block.store( new_block.id(), new_block );
   session.commit();
} FC_CAPTURE_AND_RETHROW( (new_block) ) }

/**
 *  Attempts to push the transaction into the pending queue
 */
bool database::push_transaction( const signed_transaction& trx, uint32_t skip )
{
   // If this is the first transaction pushed after applying a block, start a new undo session.
   // This allows us to quickly rewind to the clean state of the head block, in case a new block arrives.
   if( !_pending_block_session ) _pending_block_session = _undo_db.start_undo_session();
   auto session = _undo_db.start_undo_session();
   apply_transaction( trx, skip );
   _pending_block.transactions.push_back(trx);
   // The transaction applied successfully. Merge its changes into the pending block session.
   session.merge();
   return true;
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

const global_property_object& database::get_global_properties()const
{
   return get( global_property_id_type() );
}
const fee_schedule_type&  database::current_fee_schedule()const
{
   return get_global_properties().current_fees;
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

optional<signed_block> database::fetch_block_by_id( const block_id_type& id )const
{
   auto b = _fork_db.fetch_block( id );
   if( !b )
      return optional<signed_block>();
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
void database::save_undo( const object& obj )
{
   _undo_db.on_modify( obj );
}

void database::save_undo_add( const object& obj )
{
   _undo_db.on_create( obj );
}

} } // namespace bts::chain
