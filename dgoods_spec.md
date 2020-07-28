dGoods: Digital Goods Token Spec v1.0
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
category, name, maximum supply, rev split, who has the ability to issue tokens, and
if the token is fungible or not etc. Name type is a string 12 characters
max a-z, 1-5. Max supply is given as an eosio asset. For non fungible
tokens, precision must be 0 (you must use ints). The symbol in the asset
must match the symbol in `setconfig`.

```c++
ACTION create(name issuer, name rev_partner, name category, name token_name, bool fungible, bool
burnable, bool sellable, bool transferable, double rev_split, string base_uri, uint32_t
max_issue_days, asset max_supply);
```

**ISSUE**: The issue method mints a token and gives ownership to the
'to' account name. For a valid call the `category`, and `token_name`
must have been first created. Quantity must be an int for NFT and if greater
than 1, will issue multiple tokens. Precision must match precision of
`max_supply`.

```c++
ACTION issue(name to, name category, name token_name, asset quantity, string relative_uri,
             string memo);
```

**PAUSEXFER**: Pauses all transfers of all tokens. Only callable by the
contract. If pause is true, will pause. If pause is false will unpause
transfers.

```c++
ACTION pausexfer(bool pause);
```

**BURNNFT**: Burn method destroys specified tokens and frees the RAM.
Only owner may call burn function, burnable must be true, and token must not be locked

```c++
ACTION burnnft(name owner, vector<uint64_t> dgood_ids);
```

**BURNFT**: Burn method destroys fungible tokens and frees the RAM if all
are deleted from an account. quantity must match precision of `max_supply`.
Only owner may call Burn function and burnable must be true.

```c++
ACTION burnft(name owner, uint64_t category_name_id, asset quantity);
```

**TRANSFERNFT**: Used to transfer non-fungible tokens. This allows for
the ability to batch send tokens in one function call by passing in a
list of token ids. Only the token owner can successfully call this
function, transferable must be true, and token must not be locked.

```c++
ACTION transfernft(name from, name to, vector<uint64_t> dgood_ids, string memo);
```

**TRANSFERFT**: The standard transfer method is callable only on fungible
tokens. Quantity must match precision of `max_supply`. Only token owner
may call and transferrable must be true.

```c++
ACTION transferft(name from, name to, name category, name token_name, asset quantity, string memo);
```

**LISTSALENFT**: Used to list nfts for sale in the token contract itself. Callable only by owner,
if sellable is true and token not locked, creates sale listing in the token contract, marks token as
not transferable while listed for sale. Sale is valid for `sell_by_days` number of days. If
`sell_by_days` is 0, listing is indefinite. An array of dgood_ids is required.

```c++
ACTION listsalenft(name seller, vector<uint64_t> dgood_ids, uint32_t sell_by_days, asset net_sale_amount);
```

**CLOSESALENFT**: Callable by seller if listing hasn't expired, or anyone if the listing is expired;
will remove listing, remove lock and return nft to seller

```c++
ACTION closesalenft(name seller, uint64_t batch_id);
```

*FREEZEMAXSUP*: Used either to end the time based minting early or to finalize the max supply after
the minting window has passed. Only callable if time based minting and max supply is not set. Once
successfully called, will set max supply to current supply and end the minting period.

```c++
ACTION freezemaxsup(name category, name token_name)
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
    uint64_t next_dgood_id;
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

Asks Table
--------------

Holds listings for sale in the built in decentralized exchange (DEX)

```c++
// scope is self
TABLE asks {
  uint64_t batch_id;
  vector<uint64_t> dgood_ids;
  name seller;
  asset amount;
  time_point_sec expiration;

  uint64_t primary_key() const { return batch_id; }
  uint64_t get_seller() const { return seller.value; }
};
```

Locked NFT Table
----------------

Table corresponding to tokens that are locked and temporarily not transferable

```c++
// scope is self
TABLE lockednfts {
  uint64_t dgood_id;

  uint64_t primary_key() const { return dgood_id; }
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
    asset amount;

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

Type: 3dgameAsset
-----------------

    {
      // Required Fields
      "type": string; "3dgameAsset"
      "name": string; identifies the asset the token represents
      "description": string; short description of the asset the token represents
      "imageSmall": URI pointing to image resource size 150 x 150
      "imageLarge": URI pointing to image resource size 1024 x 1024
      "3drender": URI pointing to js webgl for rendering 3d object
      "details": Key Value pairs to render in a detail view, could be things like {"strength": 5}
      // Optional Fields
      "authenticityImage": URI pointing to resource with mime type image representing certificate of authenticity
    }

Type: 2dgameAsset
-----------------

    {
      // Required Fields
      "type": string; "2dgameAsset"
      "name": string; identifies the asset the token represents
      "description": string; short description of the asset the token represents
      "imageSmall": URI pointing to image resource size 150 x 150
      "imageLarge": URI pointing to image resource size 1024 x 1024
      "details": Key Value pairs to render in a detail view, could be things like {"strength": 5}
      // Optional Fields
      "authenticityImage": URI pointing to resource with mime type image representing certificate of authenticity
    }


Type: ticket
------------

    {
      // Required Fields
      "type": string; "ticket"
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
    }

Type: art
---------

    {
      // Required Fields
      "type": string; "art"
      "name": string; identifies the asset the token represents
      "description": string; short description of the asset the token represents
      "creator": string; creator(s) of the art
      "imageSmall": URI pointing to image resource size 150 x 150
      "imageLarge": URI pointing to image resource size 1024 x 1024
      "date": unix time; date artwork was created
      "details": Key Value pairs to render in a detail view, could be things like {"materials": ["oil", "wood"], "location": "Norton Simon Museum"}
      // Optional Fields
      "authenticityImage": URI pointing to resource with mime type image representing certificate of authenticity or signature
    }

Type: jewelry
-------------

    {
      //Required Fields
      "type": string; "jewelry"
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
    }

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
