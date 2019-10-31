Example dGoods Usage
====================
                     
The following tutorial walks through an example creating, issuing, selling, and buying a
hypothetical ticket (semi-fungible asset). This assumes access and understanding regarding EOSIO
software -- as this tutorial is designed to explain the dGoods contract and not how to use EOSIO.
Specifically, all of the following actions are called from a command line environment and should be
straight forward translating these actions to their corresponding rpc calls.

If a more basic understanding is needed first, visit https://developers.eos.io/. Additionally, there
will be future tutorials aimed at beginning EOS developers.

Contract requires ~862kb of RAM


Initializing Contract
---------------------

For this tutorial I will assume an account name of `dgood.token`.  
* deploy the contract `cleos set contract dgood.token /dir/to/dgoods --abi dgoods.abi -p dgood.token@active`
* add eosio.code permissions: `cleos set account permission dgood.token active --add-code`
* The first action you must call on the contract is `setconfig`. 
* This is done to let wallets/users know what version of the contract is being used and which symbol to use for all assets in the contract.

#### setconfig
```
cleos push action dgood.token setconfig '{"symbol": "TCKT", "version": "1.0"}' -p dgood.token@active
```

To check that this worked, read from the table that was just written to.

```
cleos get table dgood.token dgood.token tokenconfigs
```
```json
{
  "rows": [{
      "standard": "dgoods",
      "version": "1.0",
      "symbol": "TCKT",
      "category_name_id": 0,
      "next_dgood_id": 0
    }
  ],
  "more": false
}

```

#### create

Now that we have set the tokenconfig data, we are free to create and issue tokens. Before actually
issuing tokens to an account, we must first create the token type. This allows us to set properties
such as if the token will be fungible, transferable, burnable etc and what the max supply should be.
Note we are not doing time based mintint, but if we wanted to we would set `max_issue_days` to some
value other than 0 and set max_supply to 0. Alternatively, you can set a max issue time and a max
supply.

```
cleos push action dgood.token create '{"issuer": "dgood.token", 
                                       "rev_partner": "dgood.token",
                                       "category": "concert1",
                                       "token_name": "ticket1",
                                       "fungible": false,
                                       "burnable": false,
                                       "sellable": true,
                                       "transferable": false,
                                       "rev_split": 0.05,
                                       "base_uri": "https://myticketingsite.com/concert1/ticket1/",
                                       "max_issue_days": 0,
                                       "max_supply": "1000 TCKT"}' -p dgood.token
```
Here we created a type of token that could be a hypothetical concert ticket with a max supply of 1000. 
* They are nonfungible since each of these tickets will have their own assigned seat. 
* They are also not burnable or transferable. 
* The only way to give your ticket to someone else would be to list it for sale through the contract. 
* If they are resold the `rev_partner` (in this case specified to the contract itself) will get 5%
  of the resold amount, with the seller getting the other 95%.
  
#### issue

One possibility is to issue all of the tokens to one of our accounts and then list it for sale
through the marketplace. The other option is to issue directly to consumers who bought a
ticket through a normal payment processor. Either way issuing is done as follows:

```
cleos push action dgood.token issue '{"to": "someaccount",
                                      "category": "concert1",
                                      "token_name": "ticket1",
                                      "quantity": "5 TCKT",
                                      "relative_uri": "",
                                      "memo": "Enjoy the concert!"}' -p dgood.token
```

* Notice quantity is an asset and requires a precision match
* Since this is an NFT quantity must always be an integer
* Relative_uri is an empty string, which means the metadata should be stored at `base_uri` +
  `dgood_id`
* For the first token issued therefore the metadata can be found at
  https://myticketingsite.com/concert1/ticket1/0
* If we wanted to use ipfs to store the metadata we should issue one token at a time and put the
  ipfs hash in `relative_uri` and the metadata for this token would be `base_uri` + `relative_uri`

Let's explore what this data looks like on chain so far:
    
##### exploring the data

To query the dgoodstats table -- which is essentially the token defintion aka information common to
all tokens of a specific type. This table is scoped by the category name.

`cleos get table dgood.token concert1 dgoodstats`

```
{
  "rows": [{
      "fungible": 0,
      "burnable": 0,
      "sellable": 1,
      "transferable": 0,
      "issuer": "dgood.token",
      "rev_partner": "dgood.token",
      "token_name": "ticket1",
      "category_name_id": 0.
      "max_supply": "1000 TCKT",
      "max_issue_window": '1970-01-01T00:00:00',
      "current_supply": "5 TCKT",
      "issued_supply": "5 TCKT",
      "rev_split": "0.05000000000000000",
      "base_uri": "https://myticketingsite.com/concert1/ticket1"
    }
  ],
  "more": false
}
```

Now let's look at all of the dgoods the contract holds.

`cleos get table dgood.token dgood.token dgood`


```
{
  "rows": [{
      "id": 0,
      "serial_number": 1,
      "owner": "someaccount",
      "category": "concert1",
      "token_name": "ticket1",
      "relative_uri": null
    },{
      "id": 1,
      "serial_number": 2,
      "owner": "someaccount",
      "category": "concert1",
      "token_name": "ticket1",
      "relative_uri": null
    },{
      "id": 2,
      "serial_number": 3,
      "owner": "someaccount",
      "category": "concert1",
      "token_name": "ticket1",
      "relative_uri": null
    },{
      "id": 3,
      "serial_number": 4,
      "owner": "someaccount",
      "category": "concert1",
      "token_name": "ticket1",
      "relative_uri": null
    },{
      "id": 4,
      "serial_number": 5,
      "owner": "someaccount",
      "category": "concert1",
      "token_name": "ticket1",
      "relative_uri": null
    }
  ],
  "more": false
}
```

Finally, let's query the account of `someaccount`

`cleos get table dgood.token someaccount accounts`

```
{
  "rows": [{
      "category_name_id": 0,
      "category": "concert1",
      "token_name": "ticket1",
      "amount": "5 TCKT"
    }
  ],
  "more": false
}
```

#### transfernft

If the token is transferable (this example is not) the owner of the token can transfer it to another
account by specifying the tokens to transfer by their dgood_id.

```
cleos push action dgood.token transfernft '{"from": "someaccount",
                                            "to": "afriendacct",
                                            "dgood_ids": [0,1],
                                            "memo": "got your tickets!"}' -p someaccount
```

Two tickets were transfered to the account "afriendacct" -- specifically the tickets with dgood_id 0
and 1.

#### burnnft

To burn one:

```
cleos push action dgood.token burnnft '{"owner": "someaccount",
                                        "dgood_ids": [0]}' -p someaccount
```

#### listsalenft

One of the biggest features of dgoods is the built in exchange. To list a dgood for sale you simply
call:

```
cleos push action dgood.token listsalenft '{"seller": "someaccount",
                                            "dgood_ids": [0],
                                            "net_sale_amount": "1.0000 EOS"}' -p someaccount
```
We have litsted the ticket with dgood_id == 0 for 1.0000 EOS. Now anyone can query the contract and
see that this is listed for sale.

`cleos get table dgood.token dgood.token asks`

```
{
  "rows": [{
      "batch_id": 0,
      "dgood_ids": [
        0
      ],
      "seller": "atestertest1",
      "amount": "1.0000 EOS",
      "expiration": "2019-08-12T18:53:49"
    }
  ],
  "more": false
}
```

But maybe the user made a mistake and would rather list two tickets together. The user can then
cancel the listing and relist two items.

```
cleos push action dgood.token closesalenft '{"seller": "someaccount",
                                             "batch_id": 0}' -p someaccount
```

```
cleos push action dgood.token listsalenft '{"seller": "someaccount",
                                            "dgood_ids": [0, 1],
                                            "net_sale_amount": "1.0000 EOS"}' -p someaccount
``` 

Now querying the asks table shows that both dgoods are listed in one batch.

```
{
  "rows": [{
      "batch_id": 0,
      "dgood_ids": [
        0,
        1
      ],
      "seller": "atestertest1",
      "amount": "2.0000 EOS",
      "expiration": "2019-08-12T19:00:49"
    }
  ],
  "more": false
}
```

Something interseting to note is that for a listing of a single dgood, the `batch_id` will be the
`dgood_id`. However, if the batch size is greater than 1 then `batch_id` will be the first
`dgood_id` in the batch. There is no way to directly query if a dgood is in a specific batch without
indexing all of the data in the asks table.

#### purchasing a listing

Finally to make a purchase, a user sends EOS to the dgoods contract with a memo in the form of
"batch_id,to_acount". That is, you may buy on behalf of others as long as you specify which account
you want the dgood to be transfered to -- of course in most cases the `to_account` will be your
account.

Using a shorthand for EOS transfers built into cleos:

```
cleos transfer buyeracct dgood.token "2.0000 EOS" "0,buyeracct" -p buyeracct
```
