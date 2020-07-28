#pragma once

#include <eosio/asset.hpp>
#include <eosio/symbol.hpp>
#include <eosio/eosio.hpp>
#include <eosio/time.hpp>
#include <eosio/singleton.hpp>
#include <string>
#include <vector>

#include "utility.hpp"

using namespace std;
using namespace eosio;
using namespace utility;

CONTRACT dgoods: public contract {
    public:
        using contract::contract;

        dgoods(name receiver, name code, datastream<const char*> ds)
            : contract(receiver, code, ds) {}

        ACTION setconfig(const symbol_code& symbol,
                         const string& version);

        ACTION create(const name& issuer,
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
                      const asset& max_supply);

        ACTION issue(const name& to,
                     const name& category,
                     const name& token_name,
                     const asset& quantity,
                     const string& relative_uri,
                     const string& memo);

        ACTION freezemaxsup( const name& category, const name& token_name );

        ACTION burnnft(const name& owner,
                       const vector<uint64_t>& dgood_ids);

        ACTION burnft(const name& owner,
                      const uint64_t& category_name_id,
                      const asset& quantity);

        void buynft(const name& from, const name& to, const asset& quantity, const string& memo);

        ACTION transfernft(const name& from,
                           const name& to,
                           const vector<uint64_t>& dgood_ids,
                           const string& memo);

        ACTION transferft(const name& from,
                          const name& to,
                          const name& category,
                          const name& token_name,
                          const asset& quantity,
                          const string& memo);

        ACTION listsalenft(const name& seller,
                           const vector<uint64_t>& dgood_ids,
                           const uint32_t sell_by_days,
                           const asset& net_sale_amount);

        ACTION closesalenft(const name& seller,
                            const uint64_t& batch_id);

        ACTION logcall(const uint64_t& dgood_id);

        ACTION logsale(const vector<uint64_t>& dgood_ids,
                       const name& seller,
                       const name& buyer,
                       const name& receiver);


        TABLE lockednfts {
            uint64_t dgood_id;

            uint64_t primary_key() const { return dgood_id; }
        };

        // now() gets current time in sec
        // uint32_t 604800 is 1 week in seconds
        TABLE asks {
            uint64_t batch_id;
            vector<uint64_t> dgood_ids;
            name seller;
            asset amount;
            time_point_sec expiration;

            uint64_t primary_key() const { return batch_id; }
            uint64_t get_seller() const { return seller.value; }
        };

        TABLE tokenconfigs {
            name standard;
            string version;
            symbol_code symbol;
            uint64_t category_name_id;
            uint64_t next_dgood_id;
        };

        TABLE categoryinfo {
            name category;

            uint64_t primary_key() const { return category.value; }
        };


        // scope is category, then token_name is unique
        TABLE dgoodstats {
            bool           fungible;
            bool           burnable;
            bool           sellable;
            bool           transferable;
            name           issuer;
            name           rev_partner;
            name           token_name;
            uint64_t       category_name_id;
            asset          max_supply;
            time_point_sec max_issue_window;
            asset          current_supply;
            asset          issued_supply;
            double         rev_split;
            string         base_uri;

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
            asset amount;

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

        using lock_index = multi_index< "lockednfts"_n, lockednfts>;

      private:
        map<name, asset> _calcfees(vector<uint64_t> dgood_ids, asset ask_amount, name seller);
        void _changeowner( const name& from, const name& to, const vector<uint64_t>& dgood_ids, const string& memo, const bool& istransfer);
        void _checkasset( const asset& amount, const bool& fungible );
        void _mint(const name& to, const name& issuer, const name& category, const name& token_name,
                  const asset& issued_supply, const string& relative_uri);
        uint64_t _nextdgoodid();
        void _add_balance(const name& owner, const name& issuer, const name& category, const name& token_name,
                         const uint64_t& category_name_id, const asset& quantity);
        void _sub_balance(const name& owner, const uint64_t& category_name_id, const asset& quantity);
};
