dGoods
======

[dGoods](https://dgoods.org/) is an open source and free standard for handling the virtual representation of items, both
digital and physical, on the EOS blockchain led by [Mythical Games](https://mythical.games).
Fungible and non-fungible tokens are represented in this standard, as well as what we term
semi-fungible tokens. These are nft's that share many properties but have at minimum a unique serial
number. Things like tickets, luxury goods, or even fractionalized ownership are well suited to this
classification and therefore, a major focus of this standard.

---

Three most important properties of dGoods which differentiate from other token standards

1) hierarchical naming structure of category:name for each token or set of tokens, enabling filtering
  and organization of tokens

2) focus on semi-fungible tokens -- that is, tokens that are 1 of n with a serial number or slightly
different metadata

3) opensource metadata types and standardization for each type with localization, eg `3dGameAsset`, `Ticket`, etc

---

[View the latest spec](dgoods_spec.md)
======================================

Changes
=======

v0.4
----

* update code to be compatible with CDT 1.6.1
* add checks for invalid amounts
* allow EOS to be sent without invoking `buynft` by setting memo to "deposit"


v0.3beta
--------

* release of beta implementation

DEX in a token (simple marketplace functionality)

* listing tokens for sale is now built into the token itself
* `listsalenft`: callable ownly by owner; creates a sale listing in the token contract valid
  for 1 week, transfers ownership to token contract
* `closesalenft`: callable by seller if listing hasn't expired, or anyone if the listing is expired;
  will remove listing and return nft to seller
* `buynft`: called when a user sends EOS to the dgood contract with a memo of "dgood_id,to_account"
* the buy action allows a user (or marketplace) to buy on behalf of another user

Metadata URI changes

* `metadata_uri` is now formed from a `base_uri` +  either the `dgood_id` or `relative_uri` if it
  exists
* `create` now takes a `base_uri` string
* `issue` has a `relative_uri` parameter, but it is now optional (enter empty string)
* if no `relative_uri` is specified on a given token, the metadata should be accessed by taking
  `base_uri` + `dgood_id`
* if a `relative_uri` exists, the metadata is `base_uri` + `relative_uri`
* this serves multiple purposes, but the most important is that it saves an immense amount of RAM
  compared to the total cost of creating a single token
* can still accommodate ipfs

Table Changes

* table names and table types now matchin abi making decoding easier from state history
* some table names were changed to be more precise
* `tokenstats` -> `dgoodstats`
* `tokeninfo` -> `dgood`
* `tokeninfo_id` -> `dgood_id`

Misc

* replaced `_self` with `get_self()` for future proofing
* actions dealing with fungible tokens now have `ft` appended (ex burn -> burnft)
* trim functionality implemented when parsing string
* fungible tokens can be created with precision of 0 and no decimal place (ex 10)
* precision mismatch assert will print what precision is required
* logcall function logs the `dgood_id` during issuance to aid in matching transaction with action


v0.2
----      

* contract implementation shared with collaborating teams for feedback/security checks

Token config

* renamed `symbolinfo` table to `tokenconfigs` 
* added name of the standard (dgoods), version, and changed symbol type to symbol_code
* added corresponding method called `setconfig` to set the symbol and version

Asset / Quantities

* introduce dasset type, similar to EOS asset but without unnecessary symbol 
* dasset has functionality to convert from string
* all quantities are passed in as strings and converted to a dasset type
* precision is set similar to EOS asset and all numbers are converted to uint64_t

Issuance
 
* `issue` now takes a metadata_type
* `tokeninfo` table adds metadata_type onto the token instead of just in the metadata itself

Transfer

* fungible transfer parameters change to include category, token_name

Token Stats Table

* added `issued_supply` to keep track of `serial_number` when a token has been burned (ie `current_supply` is decreased but `issued_supply` never decreases)

Metadata Templates

* localization added
* Instead of just json object, metadata is an array of json objects with a `lang` key that specifies the language


