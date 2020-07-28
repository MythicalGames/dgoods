#include <dgoods.hpp>
#include <math.h>

ACTION dgoods::setconfig(const symbol_code& sym, const string& version) {

    require_auth( get_self() );
    // valid symbol
    check( sym.is_valid(), "not valid symbol" );

    // can only have one symbol per contract
    config_index config_table(get_self(), get_self().value);
    auto config_singleton  = config_table.get_or_create( get_self(), tokenconfigs{ "dgoods"_n, version, sym, 1, 1 } );

    // setconfig will always update version when called
    config_singleton.version = version;
    config_table.set( config_singleton, get_self() );
}

ACTION dgoods::create(const name& issuer,
                      const name& rev_partner,
                      const name& category,
                      const name& token_name,
                      const bool& fungible,
                      const bool& burnable,
                      const bool& sellable,
                      const bool& transferable,
                      const double& rev_split,
                      const string& base_uri,
                      const uint32_t& max_issue_days,
                      const asset& max_supply) {

    require_auth( get_self() );

    time_point_sec max_issue_window = time_point_sec(0);
    // true max supply, not time based supply window
    if ( max_issue_days == 0 ) {
        // cannot have both max_supply and max_issue_days be 0
        _checkasset( max_supply, fungible );
    } else {
        uint32_t max_issue_seconds = max_issue_days * 24 * 3600;
        max_issue_window = time_point_sec(current_time_point()) + max_issue_seconds;
        check( max_supply.amount >= 0, "max supply must be 0 or greater" );
        if (fungible == false) {
            check( max_supply.symbol.precision() == 0, "NFT must have max supply as int, precision of 0" );
        }
    }

    // check if issuer account exists
    check( is_account( issuer ), "issuer account does not exist" );
    check( is_account( rev_partner), "rev_partner account does not exist" );
    // check split frac is between 0 and 1
    check( ( rev_split <= 1.0 ) && (rev_split >= 0.0), "rev_split must be between 0 and 1" );

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
        stats.rev_partner= rev_partner;
        stats.token_name = token_name;
        stats.fungible = fungible;
        stats.burnable = burnable;
        stats.sellable = sellable;
        stats.transferable = transferable;
        stats.current_supply = current_supply;
        stats.issued_supply = issued_supply;
        stats.rev_split = rev_split;
        stats.base_uri = base_uri;
        stats.max_supply = max_supply;
        stats.max_issue_window = max_issue_window;
    });

    // successful creation of token, update category_name_id to reflect
    config_singleton.category_name_id++;
    config_table.set( config_singleton, get_self() );
}


ACTION dgoods::issue(const name& to,
                     const name& category,
                     const name& token_name,
                     const asset& quantity,
                     const string& relative_uri,
                     const string& memo) {

    check( is_account( to ), "to account does not exist");
    check( memo.size() <= 256, "memo has more than 256 bytes" );

    // dgoodstats table
    stats_index stats_table( get_self(), category.value );
    const auto& dgood_stats = stats_table.get( token_name.value,
                                               "Token with category and token_name does not exist" );

    // ensure have issuer authorization and valid quantity
    require_auth( dgood_stats.issuer );

    _checkasset( quantity, dgood_stats.fungible );
    string string_precision = "precision of quantity must be " + to_string( dgood_stats.max_supply.symbol.precision() );
    check( quantity.symbol == dgood_stats.max_supply.symbol, string_precision.c_str() );

    // time based minting
    if ( dgood_stats.max_issue_window != time_point_sec(0) ) {
        check(time_point_sec(current_time_point()) <= dgood_stats.max_issue_window, "issue window has closed, cannot issue more");
    }

    if (dgood_stats.max_supply.amount != 0) {
        // check cannot issue more than max supply, careful of overflow of uint
        check( quantity.amount <= (dgood_stats.max_supply.amount - dgood_stats.issued_supply.amount), "Cannot issue more than max supply" );
    }

    if (dgood_stats.fungible == false) {
        check( quantity.amount <= 100, "can issue up to 100 at a time");
        asset issued_supply = dgood_stats.issued_supply;
        asset one_token = asset( 1, dgood_stats.max_supply.symbol);
        for ( uint64_t i = 1; i <= quantity.amount; i++ ) {
            _mint(to, dgood_stats.issuer, category, token_name,
                  issued_supply, relative_uri);
            // used to keep track of serial number when minting multiple
            issued_supply += one_token;
        }
    }
    _add_balance(to, get_self(), category, token_name, dgood_stats.category_name_id, quantity);

    // increase current supply
    stats_table.modify( dgood_stats, same_payer, [&]( auto& s ) {
        s.current_supply += quantity;
        s.issued_supply += quantity;
    });
}

ACTION dgoods::burnnft(const name& owner,
                       const vector<uint64_t>& dgood_ids) {
    require_auth(owner);

    check( dgood_ids.size() <= 20, "max batch size of 20" );
    // loop through vector of dgood_ids, check token exists
    lock_index lock_table( get_self(), get_self().value );
    dgood_index dgood_table( get_self(), get_self().value );
    for ( auto const& dgood_id: dgood_ids ) {
        const auto& token = dgood_table.get( dgood_id, "token does not exist" );
        check( token.owner == owner, "must be token owner" );

        stats_index stats_table( get_self(), token.category.value );
        const auto& dgood_stats = stats_table.get( token.token_name.value, "dgood stats not found" );

        check( dgood_stats.burnable == true, "Not burnable");
        check( dgood_stats.fungible == false, "Cannot call burnnft on fungible token, call burnft instead");
        // make sure token not locked;
        auto locked_nft = lock_table.find( dgood_id );
        check(locked_nft == lock_table.end(), "token locked");

        asset quantity(1, dgood_stats.max_supply.symbol);
        // decrease current supply
        stats_table.modify( dgood_stats, same_payer, [&]( auto& s ) {
            s.current_supply -= quantity;
        });

        // lower balance from owner
        _sub_balance(owner, dgood_stats.category_name_id, quantity);

        // erase token
        dgood_table.erase( token );
    }
}

ACTION dgoods::burnft(const name& owner,
                      const uint64_t& category_name_id,
                      const asset& quantity) {
    require_auth(owner);


    account_index from_account( get_self(), owner.value );
    const auto& acct = from_account.get( category_name_id, "token does not exist in account" );

    stats_index stats_table( get_self(), acct.category.value );
    const auto& dgood_stats = stats_table.get( acct.token_name.value, "dgood stats not found" );

    _checkasset( quantity, true );
    string string_precision = "precision of quantity must be " + to_string( dgood_stats.max_supply.symbol.precision() );
    check( quantity.symbol == dgood_stats.max_supply.symbol, string_precision.c_str() );
    // lower balance from owner
    _sub_balance(owner, category_name_id, quantity);

    // decrease current supply
    stats_table.modify( dgood_stats, same_payer, [&]( auto& s ) {
        s.current_supply -= quantity;
    });
}

ACTION dgoods::transfernft(const name& from,
                           const name& to,
                           const vector<uint64_t>& dgood_ids,
                           const string& memo ) {

    check( dgood_ids.size() <= 20, "max batch size of 20" );
    // ensure authorized to send from account
    check( from != to, "cannot transfer to self" );
    require_auth( from );

    // ensure 'to' account exists
    check( is_account( to ), "to account does not exist");

    // check memo size
    check( memo.size() <= 256, "memo has more than 256 bytes" );

    _changeowner( from, to, dgood_ids, memo, true );
}

ACTION dgoods::transferft(const name& from,
                          const name& to,
                          const name& category,
                          const name& token_name,
                          const asset& quantity,
                          const string& memo ) {
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

    _checkasset( quantity, true );
    string string_precision = "precision of quantity must be " + to_string( dgood_stats.max_supply.symbol.precision() );
    check( quantity.symbol == dgood_stats.max_supply.symbol, string_precision.c_str() );
    _sub_balance(from, dgood_stats.category_name_id, quantity);
    _add_balance(to, get_self(), category, token_name, dgood_stats.category_name_id, quantity);
}

ACTION dgoods::listsalenft(const name& seller,
                           const vector<uint64_t>& dgood_ids,
                           const uint32_t sell_by_days,
                           const asset& net_sale_amount) {
    require_auth( seller );

    time_point_sec expiration = time_point_sec(0);

    if ( sell_by_days != 0 ) {
        uint32_t sell_by_seconds = sell_by_days * 24 * 3600;
        expiration = time_point_sec(current_time_point()) + sell_by_seconds;
    }

    check (dgood_ids.size() <= 20, "max batch size of 20");
    check( net_sale_amount.amount > .02 * pow(10, net_sale_amount.symbol.precision()), "minimum price of at least 0.02 EOS");
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
        a.expiration = expiration;
    });
}

ACTION dgoods::closesalenft(const name& seller,
                            const uint64_t& batch_id) {
    ask_index ask_table( get_self(), get_self().value );
    const auto& ask = ask_table.get( batch_id, "cannot find sale to close" );

    lock_index lock_table( get_self(), get_self().value );
    if ( ask.expiration == time_point_sec(0) || time_point_sec(current_time_point()) <= ask.expiration ) {
        require_auth( seller );
        check( ask.seller == seller, "only the seller can cancel a sale in progress");
    }
    // sale has expired anyone can call this and ask removed, token removed from asks/lock
    for ( auto const& dgood_id: ask.dgood_ids ) {
        const auto& locked_nft = lock_table.get( dgood_id, "dgood not found in lock table" );
        lock_table.erase( locked_nft );
    }
    ask_table.erase( ask );
}

void dgoods::buynft(const name& from,
                    const name& to,
                    const asset& quantity,
                    const string& memo) {
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
    check ( ask.expiration == time_point_sec(0) || ask.expiration > time_point_sec(current_time_point()), "sale has expired");

    // nft(s) bought, change owner to buyer regardless of transferable
    _changeowner( ask.seller, to_account, ask.dgood_ids, "bought by: " + to_account.to_string(), false);

    // amounts owed to all parties
    map<name, asset> fee_map = _calcfees(ask.dgood_ids, ask.amount, ask.seller);
    for(auto const& fee : fee_map) {
        auto account = fee.first;
        auto amount = fee.second;

        // if seller is contract, no need to send EOS again
        if ( account != get_self() ) {
            // send EOS to account owed
            action( permission_level{ get_self(), name("active") },
                    name("eosio.token"), name("transfer"),
                    make_tuple( get_self(), account, amount, string("sale of dgood") ) ).send();
        }
    }

    // remove locks, remove from ask table
    lock_index lock_table( get_self(), get_self().value );

    for ( auto const& dgood_id: ask.dgood_ids ) {
        const auto& locked_nft = lock_table.get( dgood_id, "dgood not found in lock table" );
        lock_table.erase( locked_nft );
    }

    SEND_INLINE_ACTION( *this, logsale, { { get_self(), "active"_n } }, { ask.dgood_ids, ask.seller, from, to_account } );

    // remove sale listing
    ask_table.erase( ask );
}

// method to log dgood_id and match transaction to action
ACTION dgoods::logcall(const uint64_t& dgood_id) {
    require_auth( get_self() );
}

// method to logsuccessful sale
ACTION dgoods::logsale(const vector<uint64_t>& dgood_ids, const name& seller, const name& buyer, const name& receiver) {
    require_auth( get_self() );
}

ACTION dgoods::freezemaxsup(const name& category, const name& token_name) {
    require_auth( get_self() );

    // dgoodstats table
    stats_index stats_table( get_self(), category.value );
    const auto& dgood_stats = stats_table.get( token_name.value,
                                               "Token with category and token_name does not exist" );
    check(dgood_stats.max_issue_window != time_point_sec(0), "can't freeze max supply unless time based minting");
    check(dgood_stats.issued_supply.amount != 0, "need to issue at least one token before freezing");
    stats_table.modify( dgood_stats, same_payer, [&]( auto& s ) {
        s.max_supply = dgood_stats.issued_supply;
        s.max_issue_window = time_point_sec(0);
    });
}

// Private
map<name, asset> dgoods::_calcfees(vector<uint64_t> dgood_ids, asset ask_amount, name seller) {
    map<name, asset> fee_map;
    dgood_index dgood_table( get_self(), get_self().value );
    int64_t tot_fees = 0;
    for ( auto const& dgood_id: dgood_ids ) {
        const auto& token = dgood_table.get( dgood_id, "token does not exist" );

        stats_index stats_table( get_self(), token.category.value );
        const auto& dgood_stats = stats_table.get( token.token_name.value, "dgood stats not found" );

        name rev_partner = dgood_stats.rev_partner;
        if ( dgood_stats.rev_split == 0.0 ) {
            continue;
        }

        double fee = static_cast<double>(ask_amount.amount) * dgood_stats.rev_split / static_cast<double>( dgood_ids.size() );
        asset fee_asset(fee, ask_amount.symbol);
        auto ret_val = fee_map.insert({rev_partner, fee_asset});
        tot_fees += fee_asset.amount;
        if ( ret_val.second == false ) {
            fee_map[rev_partner] += fee_asset;
        }
    }
    //add seller to fee_map minus fees
    asset seller_amount(ask_amount.amount - tot_fees, ask_amount.symbol);
    auto ret_val = fee_map.insert({seller, seller_amount});
    if ( ret_val.second == false ) {
        fee_map[seller] += seller_amount;
    }
    return fee_map;
}

// Private
void dgoods::_changeowner(const name& from, const name& to, const vector<uint64_t>& dgood_ids, const string& memo, const bool& istransfer) {
    check (dgood_ids.size() <= 20, "max batch size of 20");
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
        _sub_balance(from, dgood_stats.category_name_id, quantity);
        _add_balance(to, get_self(), token.category, token.token_name, dgood_stats.category_name_id, quantity);
    }
}

// Private
void dgoods::_checkasset(const asset& amount, const bool& fungible) {
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
void dgoods::_mint(const name& to,
                   const name& issuer,
                   const name& category,
                   const name& token_name,
                   const asset& issued_supply,
                   const string& relative_uri) {

    dgood_index dgood_table( get_self(), get_self().value);
    auto dgood_id = _nextdgoodid();
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

// available_primary_key() will reuise id's if last minted token is burned -- bad
uint64_t dgoods::_nextdgoodid() {

    // get next_dgood_id and increment
    config_index config_table( get_self(), get_self().value );
    check(config_table.exists(), "dgoods config table does not exist, setconfig first");
    auto config_singleton  = config_table.get();
    auto next_dgood_id = config_singleton.next_dgood_id++;
    config_table.set( config_singleton, _self );
    return next_dgood_id;
}

// Private
void dgoods::_add_balance(const name& owner, const name& ram_payer, const name& category, const name& token_name,
                         const uint64_t& category_name_id, const asset& quantity) {
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
void dgoods::_sub_balance(const name& owner, const uint64_t& category_name_id, const asset& quantity) {

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
                EOSIO_DISPATCH_HELPER( dgoods, (setconfig)(create)(issue)(burnnft)(burnft)(transfernft)(transferft)(listsalenft)(closesalenft)(logcall)(freezemaxsup) )
            }
        }

        else {
            if ( code == name("eosio.token").value && action == name("transfer").value ) {
                execute_action( name(receiver), name(code), &dgoods::buynft );
            }
        }
    }
}

