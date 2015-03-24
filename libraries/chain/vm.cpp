#include "vm.hpp"

namespace bts { namespace chain {


   enum op_codes
   {
      SET32_OP,

   };

   #define GET64(X)  (*static_cast<uint64_t*>(data+cop.args[X]));
   #define GET32(X)  (*static_cast<uint32_t*>(data+cop.args[X]));
   #define GET16(X)  (*static_cast<uint16_t*>(data+cop.args[X]));
   #define GET8(X)   (*static_cast<uint8_t*>(data+cop.args[X]));

   #define GET_IND64(X) (*static_cast<uint64_t*>(data+GET16(X)));
   #define GET_IND32(X) (*static_cast<uint32_t*>(data+GET16(X)));
   #define GET_IND16(X) (*static_cast<uint16_t*>(data+GET16(X)));
   #define GET_IND8(X)  (*static_cast<uint8_t*>(data+GET16(X)));

   #define GET_IND64_OFF(X,Y) (*static_cast<uint64_t*>(data+uint16_t(GET16(X)+cop.args[Y])));
   #define GET_IND32_OFF(X,Y) (*static_cast<uint32_t*>(data+uint16_t(GET16(X)+cop.args[Y])));
   #define GET_IND16_OFF(X,Y) (*static_cast<uint16_t*>(data+uint16_t(GET16(X)+cop.args[Y])));
   #define GET_IND8_OFF(X,Y)  (*static_cast<uint8_t*> (data+uint16_t(GET16(X)+cop.args[Y])));


   // return a reference to the  item at pos X on the stack cast to the appropriate size
   #define STACK64(X)                (*static_cast<uint64_t>(data+uint16_t(stackp-(8*X)-8)));
   #define STACK32(X)                (*static_cast<uint32_t>(data+uint16_t(stackp-(8*X)-8)));
   #define STACK16(X)                (*static_cast<uint16_t>(data+uint16_t(stackp-(8*X)-8)));
   #define STACK8(X)                 (*static_cast<uint8_t>(data+uint16_t(stackp-(8*X)-8)));

   // return a referecne to the item in memory based upon dereferenceing the head of the stack as
   // a 16 bit value.
   #define STACK_IND64(X)            (*static_cast<uint64_t>(data+STACK16(X)));
   #define STACK_IND32(X)            (*static_cast<uint32_t>(data+STACK16(X)));
   #define STACK_IND16(X)            (*static_cast<uint16_t>(data+STACK16(X)));
   #define STACK_IND8(X)             (*static_cast<uint8_t>(data+STACK16(X)));

   #define STACK_IND_COFF64(X,Y)     (*static_cast<uint64_t>(data+uint16_t(STACK16(X)+(Y))));
   #define STACK_IND_COFF32(X,Y)     (*static_cast<uint32_t>(data+uint16_t(STACK16(X)+(Y))));
   #define STACK_IND_COFF16(X,Y)     (*static_cast<uint16_t>(data+uint16_t(STACK16(X)+(Y))));
   #define STACK_IND_COFF8(X,Y)      (*static_cast<uint8_t>(data+uint16_t(STACK16(X)+(Y))));

   #define STACK_IND_OFF64(X,OFFSET) (*static_cast<uint64_t>(data+uint16_t(STACK16(X)+STACK16(OFFSET))));
   #define STACK_IND_COFF64(X,OFFSET)(*static_cast<uint64_t>(data+uint16_t(STACK16(X)+OFFSET)));

   /**
    *  Every operation is viewed as a mini function that pops its arguments off of the
    *  stack and pushes its results onto the stack.  
    */
   void virtual_machine::run()
   {
      assert( mem->size == 0x00ffff+8 );
      char* data          = data->data.data();
      const op* ops       = code->code.data();
      const auto  num_ops = code->code.size();

      while( gas )
      {
         op& cop = ops[ pc >= num_ops ? 0 : pc]; 
         switch( cop.code )
         {
            case PUSH_STACK:       stackp += cop.args[0]; break;
            case POP_STACK:        stackp -= cop.args[0]; break;
            case PUSH32_OP:        stackp += 8; STACK64(0) = 0; memcpy( data + uint16_t(stackp-8), &cop.args[1], 4 ); break;
            case PUSH16_OP:        stackp += 8; STACK64(0) = 0; memcpy( data + uint16_t(stackp-8), &cop.args[1], 2 ); break;
            case PUSH8_OP:         stackp += 8; STACK64(0) = 0; memcpy( data + uint16_t(stackp-8), &cop.args[1], 1 ); break;
            case PUSHCPY_OP:       stackp += 8; STACK64(0) = STACK64(1);         break;
            case PUSHPC_OP:        stackp += 8; STACK64(0) = pc;                 break;
            case SET64_OP:         stackp += 8; STACK64(0) = GET64(1);           break;
            case SET32_OP:         stackp += 8; STACK64(0) = GET32(1);           break;
            case SET16_OP:         stackp += 8; STACK64(0) = GET16(1);           break;
            case SET8_OP:          stackp += 8; STACK64(0) = GET8(1);            break;
            case SET64_IND_OP:     stackp += 8; STACK64(0) = GET_IND64(1);       break;
            case SET32_IND_OP:     stackp += 8; STACK64(0) = GET_IND32(1);       break;
            case SET16_IND_OP:     stackp += 8; STACK64(0) = GET_IND16(1);       break;
            case SET8_IND_OP:      stackp += 8; STACK64(0) = GET_IND8(1);        break;
            // STORE items from the stack back to main memory
            case STORE8:  STACK_IND8(1)  = STACK8(0);  stackp -= 8; break;
            case STORE16: STACK_IND16(1) = STACK16(0); stackp -= 8; break;
            case STORE32: STACK_IND32(1) = STACK32(0); stackp -= 8; break;
            case STORE64: STACK_IND64(1) = STACK64(0); stackp -= 8; break;

            // treat the item at the head of the stack as a pointer and dereference it.
            case DEREF64_OP:        STACK64(0) = STACK_IND64(0);                  break;
            case DEREF32_OP:        STACK64(0) = STACK_IND32(0);                  break;
            case DEREF16_OP:        STACK64(0) = STACK_IND16(0);                  break;
            case DEREF8_OP:         STACK64(0) = STACK_IND8(0);                   break;
            case DEREF64_COFF_OP:   STACK64(0) = STACK_IND_COFF64(0,cop.args[1]); break;
            case DEREF32_COFF_OP:   STACK64(0) = STACK_IND_COFF32(0,cop.args[1]); break;
            case DEREF16_COFF_OP:   STACK64(0) = STACK_IND_COFF16(0,cop.args[1]); break;
            case DEREF8_COFF_OP:    STACK64(0) = STACK_IND_COFF8(0,cop.args[1]);  break;

            case INC64_OP:    ++STACK64(0);                                         break;
            case DEC64_OP:    --STACK64(0);                                         break;
            case INV64_OP:    STACK64(0) = ~STACK64(0);                             break; 
            case NOT64_OP:    STACK64(0) = !STACK64(0);                             break; 
            case CLR64_OP:    STACK64(0) = 0;                                       break;
            case ADD64_OP:    STACK64(1) += STACK64(0);               stackp -= 8;  break; 
            case SUB64_OP:    STACK64(1) -= STACK64(0);               stackp -= 8;  break; 
            case MUL64_OP:    STACK64(1) *= STACK64(0);               stackp -= 8;  break; 
            case AND64_OP:    STACK64(1) &= STACK64(0);               stackp -= 8;  break; 
            case OR64_OP:     STACK64(1) |= STACK64(0);               stackp -= 8;  break; 
            case XOR64_OP:    STACK64(1) ^= STACK64(0);               stackp -= 8;  break; 
            case EQ64_OP:     STACK64(1) = STACK64(0) == STACK64(1);  stackp -= 8;  break; 
            case NEQ64_OP:    STACK64(1) = STACK64(0) != STACK64(1);  stackp -= 8;  break; 
            case LT64_OP:     STACK64(1) = STACK64(0) <  STACK64(1);  stackp -= 8;  break; 
            case LTEQ64_OP:   STACK64(1) = STACK64(0) <= STACK64(1);  stackp -= 8;  break; 
            case GT64_OP:     STACK64(1) = STACK64(0) >  STACK64(1);  stackp -= 8;  break; 
            case GTEQ64_OP:   STACK64(1) = STACK64(0) >= STACK64(1);  stackp -= 8;  break; 
            case RSHIFT_OP:   STACK64(1) = STACK64(0) << STACK64(1);  stackp -= 8;  break; 
            case LSHIFT_OP:   STACK64(1) = STACK64(0) >> STACK64(1);  stackp -= 8;  break; 

            case DIV64_OP:              
                 FC_ASSERT( STACK64(1) != 0 ); 
                 STACK64(1) = STACK64(0) / STACK64(1);                stackp -= 8;  break;
            case GOTO: pc = STACK16(0); stackp -= 8; break;
            case JUMP: pc += STACK16(0); stackp -= 8; break 
            case GOTOC: pc = cop.args[0]; break;
            case JUMPC: pc += cop.args[0]; break;
            case JUMP_IF:             pc += STACK64(0) ? cop.args[1] : 0;                     stackp -= 8;  break;
            case JUMP_IF_IND_OP:      pc += STACK_IND64(0) ? cop.args[1] : 0;                 stackp -= 8;  break;
            case JUMP_IF_IND_OFF_OP:  pc += STACK_IND_OFF64(0,1) ? cop.args[0] : 0;           stackp -= 16; break;
            case JUMP_IF_IND_COFF_OP: pc += STACK_IND_OFF64(0,cop.args[0]) ? cop.args[1] : 0; stackp -= 8;  break;
            case STOP_OP: 
                 return;
            case ERROR_OP: FC_ASSERT( false, "error detected" );
            case MEMCPY_OP:  
                 FC_ASSERT( STACK16(0) + STACK16(2) > STACK16(0) && STACK16(1) + STACK16(2) > STACK16(1) );
                 gas -= STACK16(2)/8; 
                 memcpy( data + STACK16(0), data+STACK16(1), STACK16(2) );  stackp -= 8*3; break;
            case CPY_ARGS_OP: // read memory positions from stack, then pop the stack by 3 items
                 FC_ASSERT( STACK(1) + STACK16(2) <= args.size && STACK16(0) + STACK16(2) > STACK16(0) );
                 gas -= STACK16(2)/8; 
                 memcpy( data + STACK16(0), args.data + STACK16(1), STACK16(2) );  stackp -= 8*3; break;
            case CPY_RESULT_OP: // read memory positions from stack, then pop the stack by 3 items
                 FC_ASSERT( STACK16(0) + STACK16(2) <= result.size && STACK16(0) + STACK16(2) > STACK16(0) );
                 gas -= STACK16(2)/8; 
                 memcpy( result.data + STACK16(0), data + STACK16(1), STACK16(2) );  stackp -= 8*3; break;
            case CACHE_PERMISSION: // head of the stack should be a object_id_type
                 FC_ASSERT( data.saved_permissions.size() <= BTS_MAX_CACHED_PERMISSIONS );
                 STACK64(0) = data->saved_permissions.insert(object_id_type(STACK64(0))).second; break;
            case UNCACHE_PERMISSION:
                 STACK64(0) = data->saved_permissions.erase( object_id_type(STACK64(0)) );  break;
            case HAS_CACHED_PERMISSION:
                 STACK64(0) = data->saved_permissions.find( object_id_type(STACK64(0)) ) != data->saved_permissions.end(); break;
            case PUSH_PERMISSION:
            {
               auto itr = data->saved_permissions.find( object_id_type(STACK64(0)) ); 
               FC_ASSERT( itr != data->saved_permissions.end() );
               STACK64(0) = active_permissions.insert( *itr ).second;
            }
            case POP_PERMISSION:
                 STACK64(0) = active_permissions.erase( object_id_type(STACK64(0)) );  break;
            case HAS_PERMISSION:
                 STACK64(0) = active_permissions.find( object_id_type(STACK64(0)) ) != data->saved_permissions.end(); break;
            case COPY_FOREIGN_OP: // (DEST_LOCATION(0), SRC_DATA_OBJECT_ID(1), SRC_OFFSET(2), SIZE(3)
            {
               const auto& src = db->get<data_object>( object_id_type(STACK64(1)) );
               FC_ASSERT( STACK16(0) + STACK16(3) > STACK16(0) && STACK16(2) + STACK16(3) > STACK16(2) );
               gas -= STACK16(3)/8; 
               memcpy( data + STACK16(0), src.data.data()+STACK16(2), STACK16(3) );  stackp -= 8*4; break;
            }
            case CALL_FOREIGN_OP: // FOREIGN ACCOUNT_ID, ARG_POS, ARG_SIZE, RESULT_POS, RESULT_SIZE 
            {
               FC_ASSERT( depth < BTS_MAX_FOREIGN_CALL_DEPTH );
               account_id_type foreign_account_id(STACK64(0));
               const account_object& foreign_account = foreign_account_id(*db);
               buffer<const char> args{ data + STACK16(1), STACK16(2) };
               buffer<char> result{ data + STACK16(3), STACK16(4) };
               FC_ASSERT( STACK16(1) + STACK16(2) > STACK16(1) );
               FC_ASSERT( STACK16(3) + STACK16(3) > STACK16(3) );

               virtual_machine nested;
               db->modify( foreign_account.data(*db), [&]( data_object& d ) { nested.data = &d; } );
               nested.code               = &foreign_account.code(*db);
               nested.account            = &foreign_account;
               nested.args               = args;
               nested.result             = result;
               nested.active_permissions = active_permissions;
               nested.db                 = db;
               nested.gas                = gas;
               nested.depth              = depth + 1;

               nested.run();
               stackp -= 5*8;
               // see what we have left over
               gas = nested.gas;
            }
            case SHA256_OP: // START_LOC, SIZE =>    pop2, push4
            case SHA512_OP: // START_LOC, SIZE =>    pop2, push8
            case RAND_OP:   // BLOCK_NUM             pop1, push rand seed for block
            case TIME_OP:   // PUSH HEAD_BLOCK_TIME
            case EXECUTE_DB_OPERATION_OP: // ONE CODE PER OP
         }
         ++pc;
         --gas;
      }

      

   }

} } // bts::chain 
