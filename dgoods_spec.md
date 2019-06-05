dGoods: Digital Goods Token Spec v0.4beta
=====================================

Cameron Thacker, Stephan Cunningham, Rudy Koch, John Linden

[@Mythical Games](https://mythical.games/) 


    Copyright (C) 2019 MYTHICAL GAMES.
    Permission is granted to copy, distribute and/or modify this document
    under the terms of the GNU Free Documentation License, Version 1.3
    or any later version published by the Free Software Foundation;
    with no Invariant Sections, no Front-Cover Texts, and no Back-Cover Texts.
    A copy of the license is included in the file entitled "gnu_free_documentation_license".

Introduction
============

dGoods is an open source and free standard for handling the virtual
representation of items, both digital and physical, on the EOS
blockchain. While our initial focus is on assets for video games, we are
designing something that does not preclude use for other types of goods.
We believe this specification could become a standard when there is a
need for supporting non-fungible, semi-fungible, and or fungible tokens
on the EOS blockchain.

Our ansatz is that a game or company will want to create many different
tokens that live under one main token symbol. Some of these tokens may
be fungible or semi-fungible while others are non-fungible. What really
separates this standard from other NFT standards that exist is the
hierarchical naming structure of the token. Fungible tokens are
identified by symbol:category:name and NFTs have an extra token id
associated. This allows for unprecedented organization of tokens. It
also enables wallets and dApps to surface tokens by category or name,
providing search and filtering functionality.

Our design philosophy is to start small, remain flexible, and provide a
robust digital goods standard that supports a diverse and innovative
development community. Therefore, much of this specification is a
logical extension of the standard EOS token with the addition of
significant functionality improvements that will allow teams to easily
integrate and display virtual items. In the following sections we
describe the minimum set of required methods and functionality for
implementing this specification, along with the table structure, and
finally some example metadata templates.

Required Methods
================

**SETCONFIG**: Set config adds the symbol, and version of dgoods spec.
It also initializes the `category_name_id` to zero. Must be called
first. Can be called again to update the version, but the symbol will
not update.

```c++
ACTION setconfig(symbol_code sym, string version);
```

**CREATE**: The create method instantiates a token. This is required
before any tokens can be issued and sets properties such as the
category, name, maximum supply, who has the ability to issue tokens, and
if the token is fungible or not etc. Name type is a string 12 characters
max a-z, 1-5. Supply is given as a string and converted to the dasset
type internally. For fungible tokens precision is set by including
values after a decimal point (similar to a normal EOS asset).

```c++
ACTION create(name issuer, name category, name token_name, bool fungible, bool
              burnable, bool transferable, string base_uri, string max_supply);
```

**ISSUE**: The issue method mints a token and gives ownership to the
'to' account name. For a valid call the `category`, and `token_name`
must have been first created. Quantity will be set to 1 if non-fungible
or semi-fungible, otherwise quantity must match precision of
`max_supply`. `Metadata_type` must be one of the accepted metadata type
templates.

```c++
ACTION issue(name to, name category, name token_name, string quantity, string relative_uri,
             string memo);
```

**PAUSEXFER**: Pauses all transfers of all tokens. Only callable by the
contract. If pause is true, will pause. If pause is false will unpause
transfers.

```c++
ACTION pausexfer(bool pause);
```

**BURNNFT**: Burn method destroys specified tokens and frees the RAM.
Only owner may call burn function and burnable must be true.

```c++
ACTION burnnft(name owner, vector<uint64_t> dgood_ids);
```

**BURNFT**: Burn method destroys fungible tokens and frees the RAM if all
are deleted from an account. Only owner may call Burn function and
burnable must be true.

```c++
ACTION burnft(name owner, uint64_t category_name_id, string quantity);
```

**TRANSFERNFT**: Used to transfer non-fungible tokens. This allows for
the ability to batch send tokens in one function call by passing in a
list of token ids. Only the token owner can successfully call this
function and transferable must be true.

```c++
ACTION transfernft(name from, name to, vector<uint64_t> dgood_ids, string memo);
```

**TRANSFERFT**: The standard transfer method is callable only on fungible
tokens. Quantity must match precision of `max_supply`. Only token owner
may call and transferrable must be true.

```c++
ACTION transferft(name from, name to, name category, name token_name, string quantity, string memo);
```

**LISTSALENFT**: Used to list an nft for sale in the token contract itself. Callable only by owner,
creates sale listing in the token contract; transfers ownership to token contract while sale is
active

```c++
ACTION listsalenft(name seller, uint64_t dgood_id, asset net_sale_amount);
```

**CLOSESALENFT**: Callable by seller if listing hasn't expired, or anyone if the listing is expired;
will remove listing and return nft to seller

```c++
ACTION closesalenft(name seller, uint64_t dgood_id);
```

Dasset Type
===========

To accommodate the ability to specify precision in a token, we take an
approach similar to EOS, by creating an asset type that stores the
amount in a `uint64_t` and the precision in a `uint8_t`. Values with
decimal points like 1.23 become a uint of 123 with precision of 2.
Additionally, the struct checks values fall within accepted limits.

There are two reasons for this type rather than using the
`<eoslib/asset.hpp>` type. The first reason is that we do not want a
token symbol to be attached to this type. We have a more complicated
structure for naming already and only require properly converting and
storing amounts. This saves RAM that would be unused. The second reason
to create our own asset is to have a method for generating an asset from
a string.

Dasset Struct
-------------

```c++
struct dasset {
    uint64_t amount;
    uint8_t precision; 

    void from_string(const string& string_amount) {}        
    void from_string(const string& string_amount, const uint64_t& p) {}
}
EOSLIB_SERIALIZE( dasset, (amount)(precision) )
```

Token Data
==========

Token Config Table
------------------

Singleton which holds the symbol for the contract, version, type of
standard, and `category_name_id`, which is like a global id for category
and token name pairs. This is the first table created and must be
defined with `setconfig` before any tokens can be created. As part of a
\"standard among standards\" the token configs table will have a version
and standard field that lets wallets know what they will need to support
from this contract.

`category_name_id` is incremented each time `create` is successfully
called

```c++
// scope is self
TABLE tokenconfigs {
    name standard;
    string version;
    symbol_code symbol;
    uint64_t category_name_id;
};
```

dGood Stats Table
-----------------

Ensures there can be only one `category`, `token_name pair`. Stores
whether a given token is fungible, burnable, or transferrable and what
the current and max supplies are. Info is written when a token is
created. Issued supply is needed to keep track of unique id's when
tokens are burned as issued supply never decreases.

```c++
// scope is category, then token_name is unique
TABLE dgoodstats {
    bool     fungible;
    bool     burnable;
    bool     transferable;
    name     issuer;
    name     token_name;
    uint64_t category_name_id;
    dasset   max_supply;
    uint64_t current_supply;
    uint64_t issued_supply;
    string base_uri;
     
    uint64_t primary_key() const { return token_name.value; }
};
```

dGood Table
----------------

This is the global list of non or semi-fungible tokens. Secondary
indices provide search by owner.

```c++
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
```

Category Table
--------------

Holds all category names for easy querying.

```c++
// scope is self
TABLE categoryinfo {
  name category;

  uint64_t primary_key() const { return category.value; }
};
```

ASKS Table
--------------

Holds listings for sale in the built in decentralized exchange (DEX)

```c++
// scope is self
TABLE asks {
  uint64_t dgood_id;
  name seller;
  asset amount;
  time_point_sec expiration;

  uint64_t primary_key() const { return dgood_id; }
  uint64_t get_seller() const { return seller.value; }
};    
```

Account Table
-------------

The Account table holds the fungible tokens for an account, and a
reference to how many NFTs that account owns of a given type.

```c++
// scope is owner
TABLE accounts {
    uint64_t category_name_id;
    name category;
    name token_name;
    dasset amount;
    
    uint64_t primary_key() const { return category_name_id; }
}; 
```

Metadata Templates
==================

In order for wallets or dApps to support various digital goods, there
need to be standards associated with the metadata. Our approach is to
define templates based on the type of good. The following templates are
candidates we have put forth, but this is to be a collaborative
exercise. We want to provide a repository of templates that are agreed
upon by the community. All metadata is formatted as JSON objects
specified from the template types.

To accommodate multiple languages, the metadata is an array of JSON
objects each with a unique language code and translation that can easily
be filtered on. All keys are in English, but values can be in whatever
language specified by the `lang` field.

Type: 3dgameAsset
-----------------

    [{                 
      // Required Fields
      "lang": string; two-letter code specified in the ISO 639-1 language standard
      "name": string; identifies the asset the token represents
      "description": string; short description of the asset the token represents
      "imageSmall": URI pointing to image resource size 150 x 150 
      "imageLarge": URI pointing to image resource size 1024 x 1024
      "3drender": URI pointing to js webgl for rendering 3d object
      "details": Key Value pairs to render in a detail view, could be things like {"strength": 5}
      // Optional Fields
      "authenticityImage": URI pointing to resource with mime type image representing certificate of authenticity
    }]

Type: 2dgameAsset
-----------------

    [{
      // Required Fields
      "lang": string; two-letter code specified in the ISO 639-1 language standard
      "name": string; identifies the asset the token represents
      "description": string; short description of the asset the token represents
      "imageSmall": URI pointing to image resource size 150 x 150 
      "imageLarge": URI pointing to image resource size 1024 x 1024
      "details": Key Value pairs to render in a detail view, could be things like {"strength": 5}
      // Optional Fields
      "authenticityImage": URI pointing to resource with mime type image representing certificate of authenticity
    }]


Type: ticket
------------

    [{
      // Required Fields
      "lang": string; two-letter code specified in the ISO 639-1 language standard
      "name": string; identifies the event the ticket is for
      "description": string; short description of the event the ticket is for
      "location": string; name of the location where the event is being held
      "address": string; address of the location where the event is being held
      "date": Unix time; starting date of the event
      "time": Unix time; starting time of the event
      "imageSmall": URI pointing to image resource size 150 x 150
      "imageLarge": URI pointing to image resource size 1024 x 1024
      "details": Key Value pairs to render in a detail view, could be things like {"openingAct": "Nickelback"}
      // Optional Fields
      "authenticityImage": URI pointing to resource with mime type image representing certificate of authenticity
      "duration": string; length of time the event lasts for
      "row": string; row where seat is located
      "seat": string; seat number or GA for General Admission
    }]

Type: art
---------

    [{
      // Required Fields
      "lang": string; two-letter code specified in the ISO 639-1 language standard
      "name": string; identifies the asset the token represents
      "description": string; short description of the asset the token represents
      "creator": string; creator(s) of the art
      "imageSmall": URI pointing to image resource size 150 x 150 
      "imageLarge": URI pointing to image resource size 1024 x 1024
      "date": unix time; date artwork was created
      "details": Key Value pairs to render in a detail view, could be things like {"materials": ["oil", "wood"], "location": "Norton Simon Museum"}
      // Optional Fields
      "authenticityImage": URI pointing to resource with mime type image representing certificate of authenticity or signature
    }]

Type: jewelry
-------------

    [{
      //Required Fields
      "lang": string; two-letter code specified in the ISO 639-1 language standard
      "name": string; identifies the watch the token represents
      "description": string; short description of the watch the token represents
      "imageSmall": URI pointing to image resource size 150 x 150 
      "imageLarge": URI pointing to image resource size 1024 x 1024
      "image360": URI pointing to image resource for 360 image
      "manufactureDate": Unix time; date watch was manufactured
      "manufacturePlace": string; place watch was manufactured
      "details": Key Value pairs to render in a detail view, could be things like {"caret": 1}
      //Optional Fields
      "authenticityImage": URI pointing to resource with mime type image representing certificate of authenticity
    }]

Definitions
===========

-   **RAM** - Refers to the permanent storage space on the EOS
    blockchain. Must be purchased from the EOS blockchain itself by
    sending EOS. Can also be sold if unused.

-   **Account** - An EOS account is up to 12 characters and contains
    letters a-z, numbers 1-5 and a period (if using name service)

-   **Symbol** - Defined to be a max of 7 characters capital letters A-Z
    only

-   **Fungible** - in this context it refers to tokens which are
    interchangeable like EOS itself. When tokens are fungible,
    transferring any token is equivalent to any other.

-   **Semi-fungible** - Tokens which are technically unique in that they
    have a unique identifier, but may share the same or similar data to
    other tokens. Many physical goods would fall into this category as
    they have a unique serial number but otherwise are
    indistinguishable.

-   **Non-fungible** - Tokens that are unique
