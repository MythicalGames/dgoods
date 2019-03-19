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

2) focus on semi-fungible tokens -- that is, tokens that are 1 of n with a serial number

3) opensource metadata types and standardization for each type with localization, eg `3dGameAsset`, `Ticket`, etc

---

[View the latest spec](dgoods_spec.md)
======================================

Changes
=======

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


