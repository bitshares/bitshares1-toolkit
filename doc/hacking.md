
Intro for new developers
------------------------

This is a quick introduction to get new developers up to speed on Graphene.

Starting Graphene
-----------------

    git clone https://gitlab.bitshares.org/dlarimer/graphene
    cd graphene
    git submodule init
    git submodule update

TODO:  Are recursive flags needed for submodules?

    cmake -DCMAKE_BUILD_TYPE=Debug .
    make witness_node && make cli_wallet
    ./witness_node

In a separate window, start `cli_wallet`:

    ./cli_wallet

You will get a transport error because the default initialization of witness node configuration does not specify a port.
You can edit `witneess_node_data_dir/config.json` like this:

    "websocket_endpoint": "127.0.0.1:8090"

If you send private keys over this connection, clearly `websocket_endpoint` should be bound to localhost for security.

BitAssets 3.0 partial cover vs fractional cover
-----------------------------------------------

The main purpose of this article is to address whether BitAssets 3.0 should *cover* positions or *release* positions.  In particular,
let us suppose we have the following book:



Core mechanics
--------------

- Witnesses
- Key members
- Price feeds
- Global parameters
- Voting on witnesses
- Voting on key members
- Witness pay
- Transfers
- Markets
- Escrow
- Recurring payments

Witness node
------------

The role of the witness node is to broadcast transactions, download blocks, and optionally sign them.

TODO:  How do you get block signing keys into the witness node?

Stuff to know about the code
----------------------------

`static_variant<t1, t2>` is a *union type* which says "this variable may be either t1 or t2."  It is serializable if t1 and t2 are both serializable (TODO:  Is this accurate?)

The 'operations.hpp` documents the available operations, and `database_fixture.hpp` shows the way to do many things.

Tests also show the way to do many things, but are often cluttered with code that generates corner cases to try to break things in every possible way.

Visitors are at the end of `operations.hpp` after the large typedef for `operation` as a `static_variant`.  TODO:  They should be refactored into a separate header.

TODO: Questions
---------------

This section contains questions for more experienced developers to answer.

Is there a way to generate help with parameter names and method descriptions?

Is there a way to allow external program to drive `cli_wallet` via websocket, JSONRPC, or HTTP?

