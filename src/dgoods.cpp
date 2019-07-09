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
                      bool sellable,
                      bool transferable,
                      string base_uri,
                      asset max_supply) {

    require_auth( get_self() );

    checkasset( max_supply, fungible );
    // check if issuer account exists
    check( is_account( issuer ), "issuer account does not exist" );

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

    asset current_supply = asset( 0, symbol( config_singleton.symbol, max_supply.symbol.precision() ));
    asset issued_supply = asset( 0, symbol( config_singleton.symbol, max_supply.symbol.precision() ));


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
        stats.sellable = sellable;
        stats.transferable = transferable;
        stats.current_supply = current_supply;
        stats.issued_supply = issued_supply;
        stats.base_uri = base_uri;
        stats.max_supply = max_supply;
    });

    // successful creation of token, update category_name_id to reflect
    config_singleton.category_name_id++;
    config_table.set( config_singleton, get_self() );
}


ACTION dgoods::issue(name to,
                     name category,
                     name token_name,
                     asset quantity,
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

    checkasset( quantity, dgood_stats.fungible );
    string string_precision = "precision of quantity must be " + to_string( dgood_stats.max_supply.symbol.precision() );
    check( quantity.symbol == dgood_stats.max_supply.symbol, string_precision.c_str() );
    // check cannot issue more than max supply, careful of overflow of uint
    check( quantity.amount <= (dgood_stats.max_supply.amount - dgood_stats.current_supply.amount), "Cannot issue more than max supply" );

    if (dgood_stats.fungible == false) {
        if ( quantity.amount > 1 ) {
            for ( uint64_t i = 1; i <= quantity.amount; i++ ) {
                mint(to, dgood_stats.issuer, category, token_name,
                     dgood_stats.issued_supply, relative_uri);
            }
        } else {
            mint(to, dgood_stats.issuer, category, token_name,
                 dgood_stats.issued_supply, relative_uri);
        }
    }
    add_balance(to, get_self(), category, token_name, dgood_stats.category_name_id, quantity);

    // increase current supply
    stats_table.modify( dgood_stats, same_payer, [&]( auto& s ) {
        s.current_supply += quantity;
        s.issued_supply += quantity;
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

        asset quantity(1, dgood_stats.max_supply.symbol);
        // decrease current supply
        stats_table.modify( dgood_stats, same_payer, [&]( auto& s ) {
            s.current_supply -= quantity;
        });

        // lower balance from owner
        sub_balance(owner, dgood_stats.category_name_id, quantity);

        // erase token
        dgood_table.erase( token );
    }
}

ACTION dgoods::burnft(name owner,
                      uint64_t category_name_id,
                      asset quantity) {
    require_auth(owner);


    account_index from_account( get_self(), owner.value );
    const auto& acct = from_account.get( category_name_id, "token does not exist in account" );

    stats_index stats_table( get_self(), acct.category.value );
    const auto& dgood_stats = stats_table.get( acct.token_name.value, "dgood stats not found" );

    checkasset( quantity, true );
    string string_precision = "precision of quantity must be " + to_string( dgood_stats.max_supply.symbol.precision() );
    check( quantity.symbol == dgood_stats.max_supply.symbol, string_precision.c_str() );
    // lower balance from owner
    sub_balance(owner, category_name_id, quantity);

    // decrease current supply
    stats_table.modify( dgood_stats, same_payer, [&]( auto& s ) {
        s.current_supply -= quantity;
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

    changeowner( from, to, dgood_ids, memo, true );

}

ACTION dgoods::transferft(name from,
                          name to,
                          name category,
                          name token_name,
                          asset quantity,
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

    checkasset( quantity, true );
    string string_precision = "precision of quantity must be " + to_string( dgood_stats.max_supply.symbol.precision() );
    check( quantity.symbol == dgood_stats.max_supply.symbol, string_precision.c_str() );
    sub_balance(from, dgood_stats.category_name_id, quantity);
    add_balance(to, get_self(), category, token_name, dgood_stats.category_name_id, quantity);

}

ACTION dgoods::listsalenft(name seller,
                           vector<uint64_t> dgood_ids,
                           asset net_sale_amount) {
    require_auth( seller );

    check( net_sale_amount.amount > .02, "minimum price of at least 0.02 EOS");
    check( net_sale_amount.symbol == symbol( symbol_code("EOS"), 4), "only accept EOS for sale" );

    dgood_index dgood_table( get_self(), get_self().value );
    for ( auto const& dgood_id: dgood_ids ) {
        const auto& token = dgood_table.get( dgood_id, "token does not exist" );

        stats_index stats_table( get_self(), token.category.value );
        const auto& dgood_stats = stats_table.get( token.token_name.value, "dgood stats not found" );

        check( dgood_stats.sellable == true, "not sellable");
        check ( seller == token.owner, "not token owner");

        // make sure token not locked;
        lock_index lock_table( get_self(), get_self().value );
        auto locked_nft = lock_table.find( dgood_id );
        check(locked_nft == lock_table.end(), "token locked");

        // add token to lock table
        lock_table.emplace( seller, [&]( auto& l ) {
            l.dgood_id = dgood_id;
        });

    }

    ask_index ask_table( get_self(), get_self().value );
    // add batch to table of asks
    // set id to the first dgood being listed, if only one being listed, simplifies life
    ask_table.emplace( seller, [&]( auto& a ) {
        a.batch_id = dgood_ids[0];
        a.dgood_ids = dgood_ids;
        a.seller = seller;
        a.amount = net_sale_amount;
        a.expiration = time_point_sec(current_time_point()) + WEEK_SEC;
    });
}

ACTION dgoods::closesalenft(name seller,
                            uint64_t batch_id) {
    ask_index ask_table( get_self(), get_self().value );
    const auto& ask = ask_table.get( batch_id, "cannot find sale to close" );

    lock_index lock_table( get_self(), get_self().value );
    // if sale has expired anyone can call this and ask removed, token sent back to orig seller
    if ( time_point_sec(current_time_point()) > ask.expiration ) {
        for ( auto const& dgood_id: ask.dgood_ids ) {
            const auto& locked_nft = lock_table.get( dgood_id, "dgood not found in lock table" );
            lock_table.erase( locked_nft );
        }
        ask_table.erase( ask );

    } else {
        require_auth( seller );
        check( ask.seller == seller, "only the seller can cancel a sale in progress");
        for ( auto const& dgood_id: ask.dgood_ids ) {
            const auto& locked_nft = lock_table.get( dgood_id, "dgood not found in lock table" );
            lock_table.erase( locked_nft );
        }
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
    check( quantity.symbol == symbol( symbol_code("EOS"), 4), "Buy only with EOS" );
    check( memo.length() <= 32, "memo too long" );

    //memo format comma separated
    //batch_id,to_account
    uint64_t batch_id;
    name to_account;
    tie( batch_id, to_account ) = parsememo(memo);

    ask_index ask_table( get_self(), get_self().value );
    const auto& ask = ask_table.get( batch_id, "cannot find listing" );
    check ( ask.amount.amount == quantity.amount, "send the correct amount");
    check (ask.expiration > time_point_sec(current_time_point()), "sale has expired");

    // nft(s) bought, change owner to buyer regardless of transferable
    //SEND_INLINE_ACTION( *this, changeowner, { { get_self(), "active"_n } }, { ask.seller, to_account, ask.dgood_ids, "bought by: " + to_account.to_string(), false } );
    changeowner( ask.seller, to_account, ask.dgood_ids, "bought by: " + to_account.to_string(), false);

    // send EOS to seller
    action( permission_level{ get_self(), name("active") }, name("eosio.token"), name("transfer"),
            make_tuple( get_self(), ask.seller, quantity, string("listing sold"))).send();

    // remove locks, remove from ask table
    lock_index lock_table( get_self(), get_self().value );

    for ( auto const& dgood_id: ask.dgood_ids ) {
        const auto& locked_nft = lock_table.get( dgood_id, "dgood not found in lock table" );
        lock_table.erase( locked_nft );
    }
    ask_table.erase( ask );
}

// method to log dgood_id and match transaction to action
ACTION dgoods::logcall(uint64_t dgood_id) {
    require_auth( get_self() );
}

// Private
void dgoods::changeowner(name from, name to, vector<uint64_t> dgood_ids, string memo, bool istransfer) {
    // loop through vector of dgood_ids, check token exists
    dgood_index dgood_table( get_self(), get_self().value );
    lock_index lock_table( get_self(), get_self().value );
    for ( auto const& dgood_id: dgood_ids ) {
        const auto& token = dgood_table.get( dgood_id, "token does not exist" );

        stats_index stats_table( get_self(), token.category.value );
        const auto& dgood_stats = stats_table.get( token.token_name.value, "dgood stats not found" );

        if ( istransfer ) {
            check( token.owner == from, "must be token owner" );
            check( dgood_stats.transferable == true, "not transferable");
            auto locked_nft = lock_table.find( dgood_id );
            check( locked_nft == lock_table.end(), "token locked, cannot transfer");
        }

        // notifiy both parties
        require_recipient( from );
        require_recipient( to );
        dgood_table.modify( token, same_payer, [&] (auto& t ) {
            t.owner = to;
        });

        // amount 1, precision 0 for NFT
        asset quantity(1, dgood_stats.max_supply.symbol);
        sub_balance(from, dgood_stats.category_name_id, quantity);
        add_balance(to, get_self(), token.category, token.token_name, dgood_stats.category_name_id, quantity);
    }
}

// Private
void dgoods::checkasset(asset amount, bool fungible) {
    auto sym = amount.symbol;
    if (fungible) {
        check( amount.amount > 0, "amount must be positive" );
    } else {
        check( sym.precision() == 0, "NFT must be an int, precision of 0" );
        check( amount.amount >= 1, "NFT amount must be >= 1" );
    }

    config_index config_table(get_self(), get_self().value);
    auto config_singleton  = config_table.get();
    check( config_singleton.symbol.raw() == sym.code().raw(), "Symbol must match symbol in config" );
    check( amount.is_valid(), "invalid amount" );
}

// Private
void dgoods::mint(name to,
                  name issuer,
                  name category,
                  name token_name,
                  asset issued_supply,
                  string relative_uri) {

    dgood_index dgood_table( get_self(), get_self().value);
    auto dgood_id = dgood_table.available_primary_key();
    if ( relative_uri.empty() ) {
        dgood_table.emplace( issuer, [&]( auto& dg) {
            dg.id = dgood_id;
            dg.serial_number = issued_supply.amount + 1;
            dg.owner = to;
            dg.category = category;
            dg.token_name = token_name;
        });
    } else {
        dgood_table.emplace( issuer, [&]( auto& dg ) {
            dg.id = dgood_id;
            dg.serial_number = issued_supply.amount + 1;
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
                         uint64_t category_name_id, asset quantity) {
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
            a.amount += quantity;
        });
    }
}

// Private
void dgoods::sub_balance(name owner, uint64_t category_name_id, asset quantity) {

    account_index from_account( get_self(), owner.value );
    const auto& acct = from_account.get( category_name_id, "token does not exist in account" );
    check( acct.amount.amount >= quantity.amount, "quantity is more than account balance");

    if ( acct.amount.amount == quantity.amount ) {
        from_account.erase( acct );
    } else {
        from_account.modify( acct, same_payer, [&]( auto& a ) {
            a.amount -= quantity;
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

