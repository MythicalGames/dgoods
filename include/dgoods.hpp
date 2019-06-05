#pragma once

#include <eosio/asset.hpp>
#include <eosio/symbol.hpp>
#include <eosio/eosio.hpp>
#include <eosio/time.hpp>
#include <eosio/singleton.hpp>
#include <string>
#include <vector>

#include "dasset.hpp"

using namespace std;
using namespace eosio;
using namespace dgoods_asset;

CONTRACT dgoods: public contract {
    public:
        using contract::contract;

        const int WEEK_SEC = 3600*24*7;

        dgoods(name receiver, name code, datastream<const char*> ds)
            : contract(receiver, code, ds) {}

        ACTION setconfig(symbol_code symbol,
                         string version);


        ACTION create(name issuer,
                      name category,
                      name token_name,
                      bool fungible,
                      bool burnable,
                      bool transferable,
                      string base_uri,
                      string max_supply);

        ACTION issue(name to,
                     name category,
                     name token_name,
                     string quantity,
                     string relative_uri,
                     string memo);

        ACTION burnnft(name owner,
                       vector<uint64_t> dgood_ids);

        ACTION burnft(name owner,
                      uint64_t category_name_id,
                      string quantity);

        ACTION buynft(name from, name to, asset quantity, string memo);

        ACTION transfernft(name from,
                           name to,
                           vector<uint64_t> dgood_ids,
                           string memo);

        ACTION transferft(name from,
                          name to,
                          name category,
                          name token_name,
                          string quantity,
                          string memo);

        ACTION listsalenft(name seller,
                           uint64_t dgood_id,
                           asset net_sale_amount);

        ACTION closesalenft(name seller,
                            uint64_t dgood_id);

        ACTION logcall(uint64_t dgood_id);

        // now() gets current time in sec
        // uint32_t 604800 is 1 week in seconds
        TABLE asks {
            uint64_t dgood_id;
            name seller;
            asset amount;
            time_point_sec expiration;

            uint64_t primary_key() const { return dgood_id; }
            uint64_t get_seller() const { return seller.value; }
        };

        TABLE tokenconfigs {
            name standard;
            string version;
            symbol_code symbol;
            uint64_t category_name_id;
        };

        TABLE categoryinfo {
            name category;

            uint64_t primary_key() const { return category.value; }
        };


        // scope is category, then token_name is unique
        TABLE dgoodstats {
            bool    fungible;
            bool    burnable;
            bool    transferable;
            name    issuer;
            name    token_name;
            uint64_t category_name_id;
            dasset  max_supply;
            uint64_t current_supply;
            uint64_t issued_supply;
            string base_uri;

            uint64_t primary_key() const { return token_name.value; }
        };

        // scope is self
        TABLE dgood {
            uint64_t id;
            uint64_t serial_number;
            name owner;
            name category;
            name token_name;
            std::optional<string> relative_uri;

            uint64_t primary_key() const { return id; }
            uint64_t get_owner() const { return owner.value; }

        };

        EOSLIB_SERIALIZE( dgood, (id)(serial_number)(owner)(category)(token_name)(relative_uri) )

        // scope is owner
        TABLE accounts {
            uint64_t category_name_id;
            name category;
            name token_name;
            dasset amount;

            uint64_t primary_key() const { return category_name_id; }
        };

        using config_index = singleton< "tokenconfigs"_n, tokenconfigs >;

        using account_index = multi_index< "accounts"_n, accounts >;

        using category_index = multi_index< "categoryinfo"_n, categoryinfo>;

        using stats_index = multi_index< "dgoodstats"_n, dgoodstats>;

        using dgood_index = multi_index< "dgood"_n, dgood,
            indexed_by< "byowner"_n, const_mem_fun< dgood, uint64_t, &dgood::get_owner> > >;

        using ask_index = multi_index< "asks"_n, asks,
            indexed_by< "byseller"_n, const_mem_fun< asks, uint64_t, &asks::get_seller> > >;

      private:

        void mint(name to, name issuer, name category, name token_name,
                  uint64_t issued_supply, string relative_uri);
        void add_balance(name owner, name issuer, name category, name token_name,
                         uint64_t category_name_id, dasset quantity);
        void sub_balance(name owner, uint64_t category_name_id, dasset quantity);
};
