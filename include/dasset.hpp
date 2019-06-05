#pragma once

#include <string>
#include "utility.hpp"

using namespace std;
using namespace eosio;
using namespace utility;

namespace dgoods_asset {
    struct dasset {
        uint64_t amount;
        uint8_t precision;

        uint64_t _get_precision(uint8_t p) {
            uint64_t p10 = 1;
            while( p > 0 ) {
                p10 *= 10; --p;
            }
            return p10;
        }

        void _check_precision(const uint8_t& p) {
            static constexpr uint8_t max_precision = 18;
            check( p <= max_precision, "precision must be less than 19");
        }

        void _check_max(const uint64_t& a) {
            static constexpr uint64_t max_amount = ( 1LL << 62 ) - 1;
            check( a < max_amount, "max supply must be less than 2^62 - 1");
        }

        dasset() {}

        void from_string(const string& s) {
            string string_amount = trim(s);
            check( ( string_amount[0] != '-' ), "Amount can not be negative" );
            auto dot_pos = string_amount.find('.');
            uint64_t uint_part;
            uint64_t frac_part;
            uint64_t p10;
            if ( dot_pos != string::npos ) {
                check( ( dot_pos != string_amount.size() - 1 ), "missing decimal fraction after decimal point");
            }
            if ( dot_pos == string::npos ) {
                uint_part = stoull(string_amount);
                frac_part = 0;
                p10 = _get_precision(0);
            } else {
                precision = (uint8_t)(string_amount.size() -1 - dot_pos);
                _check_precision(precision);
                p10 = _get_precision(precision);

                uint_part = stoull(string_amount.substr( 0, dot_pos ));
                _check_max(uint_part);
                frac_part = stoull(string_amount.substr( dot_pos + 1 ));
            }
            amount = uint_part * p10 + frac_part;
            check( amount > 0, "max supply must be greater than 0");

        }

        // when you want to specify precision and do not care about how the string was formatted,
        // just get the main int part
        void from_string(const string& s, const uint64_t& p) {
            _check_precision(p);

            string string_amount = trim(s);
            check( ( string_amount[0] != '-' ), "Amount can not be negative" );
            // 1.0 1. 1
            uint64_t uint_part;
            uint64_t frac_part;
            auto dot_pos = string_amount.find('.');
            if ( dot_pos == string::npos || dot_pos == string_amount.size() - 1 ) {
                uint_part = stoull(string_amount);
                frac_part = 0;
            } else {
                uint_part = stoull(string_amount.substr( 0, dot_pos ));
                frac_part = stoull(string_amount.substr( dot_pos + 1 ));

            }

            _check_max(uint_part);
            uint64_t p10 = _get_precision(p);
            amount = uint_part * p10 + frac_part;
            check( amount > 0, "max supply must be greater than 0");

        }

    EOSLIB_SERIALIZE( dasset, (amount)(precision) )
    };
}
