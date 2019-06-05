#include <dgoods.hpp>

ACTION dgoods::setconfig(symbol_code sym, string version) {

    require_auth( get_self() );
    // valid symbol
    check( sym.is_valid(), "not valid symbol" );

    // can only have one symbol per contract
    config_index config_table(get_self(), get_self().value);
    auto config_singleton  = config_table.get_or_create( get_self(), tokenconfigs{ "dgoods"_n, version, sym, 0 } );

    // setconfig will always update version when called
    config_singleton.version = version;
    config_table.set( config_singleton, get_self() );
}

ACTION dgoods::create(name issuer,
                      name category,
                      name token_name,
                      bool fungible,
                      bool burnable,
                      bool transferable,
                      string base_uri,
                      string max_supply) {

    require_auth( get_self() );

    // check if issuer account exists
    check( is_account( issuer ), "issuer account does not exist" );

    dasset m_supply;
    if ( fungible == true ) {
        m_supply.from_string(max_supply);
    } else {
        m_supply.from_string(max_supply, 0);
        check(m_supply.amount >= 1, "max_supply for nft must be at least 1");
    }

    // get category_name_id
    config_index config_table( get_self(), get_self().value );
    check(config_table.exists(), "Symbol table does not exist, setconfig first");
    auto config_singleton  = config_table.get();
    auto category_name_id = config_singleton.category_name_id;


    category_index category_table( get_self(), get_self().value );
    auto existing_category = category_table.find( category.value );
    // category hasn't been created before, create it

    if ( existing_category == category_table.end() ) {
        category_table.emplace( get_self(), [&]( auto& cat ) {
            cat.category = category;
        });
    }

    stats_index stats_table( get_self(), category.value );
    auto existing_token = stats_table.find( token_name.value );
    check( existing_token == stats_table.end(), "Token with category and token_name exists" );
    // token type hasn't been created, create it
    stats_table.emplace( get_self(), [&]( auto& stats ) {
        stats.category_name_id = category_name_id;
        stats.issuer = issuer;
        stats.token_name = token_name;
        stats.fungible = fungible;
        stats.burnable = burnable;
        stats.transferable = transferable;
        stats.current_supply = 0;
        stats.issued_supply = 0;
        stats.base_uri = base_uri;
        stats.max_supply = m_supply;
    });

    // successful creation of token, update category_name_id to reflect
    config_singleton.category_name_id++;
    config_table.set( config_singleton, get_self() );
}


ACTION dgoods::issue(name to,
                     name category,
                     name token_name,
                     string quantity,
                     string relative_uri,
                     string memo) {

    check( is_account( to ), "to account does not exist");
    check( memo.size() <= 256, "memo has more than 256 bytes" );

    // dgoodstats table
    stats_index stats_table( get_self(), category.value );
    const auto& dgood_stats = stats_table.get( token_name.value,
                                               "Token with category and token_name does not exist" );

    // ensure have issuer authorization and valid quantity
    require_auth( dgood_stats.issuer );

    dasset q;
    if (dgood_stats.fungible == false) {
        // mint nft
        q.from_string("1", 0);
        // check cannot issue more than max supply, careful of overflow of uint
        check( q.amount <= (dgood_stats.max_supply.amount - dgood_stats.current_supply), "Cannot issue more than max supply" );

         mint(to, dgood_stats.issuer, category, token_name,
             dgood_stats.issued_supply, relative_uri);
        add_balance(to, dgood_stats.issuer, category, token_name, dgood_stats.category_name_id, q);
    } else {
        // issue fungible
        q.from_string(quantity);
        // check cannot issue more than max supply, careful of overflow of uint
        check( q.amount <= (dgood_stats.max_supply.amount - dgood_stats.current_supply), "Cannot issue more than max supply" );
        string string_precision = "precision of quantity must be " + to_string(dgood_stats.max_supply.precision);
        check( q.precision == dgood_stats.max_supply.precision, string_precision.c_str() );
        add_balance(to, dgood_stats.issuer, category, token_name, dgood_stats.category_name_id, q);

    }

    // increase current supply
    stats_table.modify( dgood_stats, same_payer, [&]( auto& s ) {
        s.current_supply += q.amount;
        s.issued_supply += q.amount;
    });

}

ACTION dgoods::burnnft(name owner,
                       vector<uint64_t> dgood_ids) {
    require_auth(owner);


    // loop through vector of dgood_ids, check token exists
    dgood_index dgood_table( get_self(), get_self().value );
    for ( auto const& dgood_id: dgood_ids ) {
        const auto& token = dgood_table.get( dgood_id, "token does not exist" );
        check( token.owner == owner, "must be token owner" );

        stats_index stats_table( get_self(), token.category.value );
        const auto& dgood_stats = stats_table.get( token.token_name.value, "dgood stats not found" );

        check( dgood_stats.burnable == true, "Not burnable");
        check( dgood_stats.fungible == false, "Cannot call burnnft on fungible token, call burn instead");

        dasset quantity;
        quantity.from_string("1", 0);

        // decrease current supply
        stats_table.modify( dgood_stats, same_payer, [&]( auto& s ) {
            s.current_supply -= quantity.amount;
        });

        // lower balance from owner
        sub_balance(owner, dgood_stats.category_name_id, quantity);

        // erase token
        dgood_table.erase( token );
    }
}

ACTION dgoods::burnft(name owner,
                      uint64_t category_name_id,
                      string quantity) {
    require_auth(owner);


    account_index from_account( get_self(), owner.value );
    const auto& acct = from_account.get( category_name_id, "token does not exist in account" );

    stats_index stats_table( get_self(), acct.category.value );
    const auto& dgood_stats = stats_table.get( acct.token_name.value, "dgood stats not found" );

    dasset q;
    q.from_string(quantity);
    string string_precision = "precision of quantity must be " + to_string(dgood_stats.max_supply.precision);
    check( q.precision == dgood_stats.max_supply.precision, string_precision.c_str() );
    // lower balance from owner
    sub_balance(owner, category_name_id, q);

    // decrease current supply
    stats_table.modify( dgood_stats, same_payer, [&]( auto& s ) {
        s.current_supply -= q.amount;
    });


}


ACTION dgoods::transfernft(name from,
                           name to,
                           vector<uint64_t> dgood_ids,
                           string memo ) {
    // ensure authorized to send from account
    check( from != to, "cannot transfer to self" );
    require_auth( from );

    // ensure 'to' account exists
    check( is_account( to ), "to account does not exist");

    // check memo size
    check( memo.size() <= 256, "memo has more than 256 bytes" );

    // loop through vector of dgood_ids, check token exists
    dgood_index dgood_table( get_self(), get_self().value );
    for ( auto const& dgood_id: dgood_ids ) {
        const auto& token = dgood_table.get( dgood_id, "token does not exist" );
        check( token.owner == from, "must be token owner" );

        stats_index stats_table( get_self(), token.category.value );
        const auto& dgood_stats = stats_table.get( token.token_name.value, "dgood stats not found" );

        check( dgood_stats.transferable == true, "not transferable");

        // notifiy both parties
        require_recipient( from );
        require_recipient( to );
        dgood_table.modify( token, same_payer, [&] (auto& t ) {
            t.owner = to;
        });

        // amount 1, precision 0 for NFT
        dasset quantity;
        quantity.amount = 1;
        quantity.precision = 0;
        sub_balance(from, dgood_stats.category_name_id, quantity);
        add_balance(to, from, token.category, token.token_name, dgood_stats.category_name_id, quantity);
    }
}

ACTION dgoods::transferft(name from,
                          name to,
                          name category,
                          name token_name,
                          string quantity,
                          string memo ) {
    // ensure authorized to send from account
    check( from != to, "cannot transfer to self" );
    require_auth( from );


    // ensure 'to' account exists
    check( is_account( to ), "to account does not exist");

    // check memo size
    check( memo.size() <= 256, "memo has more than 256 bytes" );

    require_recipient( from );
    require_recipient( to );

    stats_index stats_table( get_self(), category.value );
    const auto& dgood_stats = stats_table.get( token_name.value, "dgood stats not found" );
    check( dgood_stats.transferable == true, "not transferable");
    check( dgood_stats.fungible == true, "Must be fungible token");

    dasset q;
    q.from_string(quantity);
    string string_precision = "precision of quantity must be " + to_string(dgood_stats.max_supply.precision);
    check( q.precision == dgood_stats.max_supply.precision, string_precision.c_str() );
    sub_balance(from, dgood_stats.category_name_id, q);
    add_balance(to, from, category, token_name, dgood_stats.category_name_id, q);

}

ACTION dgoods::listsalenft(name seller,
                           uint64_t dgood_id,
                           asset net_sale_amount) {
    require_auth( seller );

    vector<uint64_t> dgood_ids = {dgood_id};

    check( net_sale_amount.amount > .02, "minimum price of at least 0.02 EOS");
    check( net_sale_amount.symbol == symbol( symbol_code("EOS"), 4), "only accept EOS for sale" );

    ask_index ask_table( get_self(), get_self().value );
    auto ask = ask_table.find( dgood_id);
    check ( ask == ask_table.end(), "already listed for sale" );

    // inline action checks ownership, permissions, and transferable prop
    SEND_INLINE_ACTION( *this, transfernft, { seller, name("active")},
                        {seller, get_self(), dgood_ids, "listing," + seller.to_string()} );

    // add token to table of asks
    ask_table.emplace( seller, [&]( auto& ask ) {
        ask.dgood_id = dgood_id;
        ask.seller = seller;
        ask.amount = net_sale_amount;
        ask.expiration = time_point_sec(current_time_point()) + WEEK_SEC;
    });
}

ACTION dgoods::closesalenft(name seller,
                            uint64_t dgood_id) {
    ask_index ask_table( get_self(), get_self().value );
    const auto& ask = ask_table.get( dgood_id, "cannot cancel sale that doesn't exist" );
    // if sale has expired anyone can call this and ask removed, token sent back to orig seller
    if ( time_point_sec(current_time_point()) > ask.expiration ) {
        ask_table.erase( ask );

        vector<uint64_t> dgood_ids = {dgood_id};
        // inline action checks ownership, permissions, and transferable prop
        SEND_INLINE_ACTION( *this, transfernft, { get_self(), name("active")},
                           {get_self(), ask.seller, dgood_ids, "clear sale returning to: " + ask.seller.to_string()} );
    } else {
        require_auth( seller );
        check( ask.seller == seller, "only the seller can cancel a sale in progress");

        vector<uint64_t> dgood_ids = {dgood_id};
        // inline action checks ownership, permissions, and transferable prop
        SEND_INLINE_ACTION( *this, transfernft, { get_self(), name("active")},
                           {get_self(), seller, dgood_ids, "close sale returning to: " + seller.to_string()} );
        ask_table.erase( ask );
    }
}

ACTION dgoods::buynft(name from,
                      name to,
                      asset quantity,
                      string memo) {
    // allow EOS to be sent by sending with empty string memo
    if ( memo == "deposit" ) return;
    // don't allow spoofs
    if ( to != get_self() ) return;
    if ( from == name("eosio.stake") ) return;
    if ( quantity.symbol != symbol( symbol_code("EOS"), 4) ) return;
    if ( memo.length() > 32 ) return;
    //memo format comma separated
    //dgood_id,to_account

    uint64_t dgood_id;
    name to_account;
    tie( dgood_id, to_account ) = parsememo(memo);

    ask_index ask_table( get_self(), get_self().value );
    const auto& ask = ask_table.get( dgood_id, "token not listed for sale" );
    check ( ask.amount.amount == quantity.amount, "send the correct amount");
    check (ask.expiration > time_point_sec(current_time_point()), "sale has expired");

    action( permission_level{ get_self(), name("active") }, name("eosio.token"), name("transfer"),
            make_tuple( get_self(), ask.seller, quantity, "sold token: " + to_string(dgood_id))).send();

    ask_table.erase(ask);

    // inline action checks ownership, permissions, and transferable prop
    vector<uint64_t> dgood_ids = {dgood_id};
    SEND_INLINE_ACTION( *this, transfernft, { get_self(), name("active")},
                        {get_self(), to_account, dgood_ids, "bought by: " + to_account.to_string()} );

}

// method to log dgood_id and match transaction to action
ACTION dgoods::logcall(uint64_t dgood_id) {
    require_auth( get_self() );
}


// Private
void dgoods::mint(name to,
                  name issuer,
                  name category,
                  name token_name,
                  uint64_t issued_supply,
                  string relative_uri) {

    dgood_index dgood_table( get_self(), get_self().value);
    auto dgood_id = dgood_table.available_primary_key();
    if ( relative_uri.empty() ) {
        dgood_table.emplace( issuer, [&]( auto& dg) {
            dg.id = dgood_id;
            dg.serial_number = issued_supply + 1;
            dg.owner = to;
            dg.category = category;
            dg.token_name = token_name;
        });
    } else {
        dgood_table.emplace( issuer, [&]( auto& dg ) {
            dg.id = dgood_id;
            dg.serial_number = issued_supply + 1;
            dg.owner = to;
            dg.category = category;
            dg.token_name = token_name;
            dg.relative_uri = relative_uri;
        });
    }
    SEND_INLINE_ACTION( *this, logcall, { { get_self(), "active"_n } }, { dgood_id } );

}

// Private
void dgoods::add_balance(name owner, name ram_payer, name category, name token_name,
                         uint64_t category_name_id, dasset quantity) {
    account_index to_account( get_self(), owner.value );
    auto acct = to_account.find( category_name_id );
    if ( acct == to_account.end() ) {
        to_account.emplace( ram_payer, [&]( auto& a ) {
            a.category_name_id = category_name_id;
            a.category = category;
            a.token_name = token_name;
            a.amount = quantity;
        });
    } else {
        to_account.modify( acct, same_payer, [&]( auto& a ) {
            a.amount.amount += quantity.amount;
        });
    }
}

// Private
void dgoods::sub_balance(name owner, uint64_t category_name_id, dasset quantity) {

    account_index from_account( get_self(), owner.value );
    const auto& acct = from_account.get( category_name_id, "token does not exist in account" );
    check( acct.amount.amount >= quantity.amount, "quantity is more than account balance");

    if ( acct.amount.amount == quantity.amount ) {
        from_account.erase( acct );
    } else {
        from_account.modify( acct, same_payer, [&]( auto& a ) {
            a.amount.amount -= quantity.amount;
        });
    }
}

extern "C" {
    void apply (uint64_t receiver, uint64_t code, uint64_t action ) {
        auto self = receiver;

        if ( code == self ) {
            switch( action ) {
                EOSIO_DISPATCH_HELPER( dgoods, (setconfig)(create)(issue)(burnnft)(burnft)(transfernft)(transferft)(listsalenft)(closesalenft)(logcall) )
            }
        }

        else {
            if ( code == name("eosio.token").value && action == name("transfer").value ) {
                execute_action( name(receiver), name(code), &dgoods::buynft );
            }
        }
    }
}

