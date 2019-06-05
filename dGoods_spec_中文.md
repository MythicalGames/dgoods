dGoods: 虚拟物品代币规范 v0.4 beta
=====================================
Cameron Thacker, Stephan Cunningham, Rudy Koch, John Linden
[译:TokenPocket](https://www.tokenpocket.pro/) 

[@Mythical Games](https://mythical.games/) 


    Copyright (C) 2019 MYTHICAL GAMES.
    根据GNU自由文档许可证1.3版或自由软件基金会发布的任何更新版本的条款，允许复制、分发和/或修改本文档;没有不变的部分，没有封面文本，也没有封底文本。
	许可证的副本包含在名为“gnu_free_documentation_license”的文件中。



介绍
============

dGoods是EOS上用于操作虚拟代表（包括虚拟的和真实的）的开源免费标准。虽然我们最初关注的是给游戏设计虚拟资产，但是我们正在设计一些不排除用于其他类型物品的内容。我们相信，当在 EOS 区块链上有需要支持非同质化，半同质化或同质化的代币需求时，该规范可以成为标准。

假设一款游戏或一个公司想要基于一个主代币下创建许多不同的代币， 这些代币中的可能部分是同质化的或半同质化的，而其余的是非同质化的。真正将此标准与其他现有的 NFT 标准区分开来的是代币的分层命名结构模式。同质化代币都可以通过 符号:类别:名字 进行识别，而 NFT 还有一个额外的 token_id 与之关联。这个代币提供了一种崭新的组织方式，它也让钱包和 dApps 通过类别和名字进行代币展示，并且提供了搜索和过滤功能。

我们的设计理念是轻量化和灵活化，并通过提供强大的虚拟物品标准，以支持多元化和创新的开发社区。因此，该规范的大部分内容都是标准 EOS 代币的逻辑扩展，并增加了重要的功能改进，使项目方可以轻松集成和显示虚拟物品。在以下部分中，我们将介绍实现此规范所需的最小方法集和功能集，以及表结构，最后展示一些元数据模板的示例。


必选函数
================

**SETCONFIG**: 这个函数用于添加代币符号还有dGoods协议的版本。并且初始化`category_name_id` 为0，而且该函数必须首先被调用，可以通过再次调用来更新版本号，但是代币符号无法被升级。

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
create函数用于实例化一个代币。它是在任何代币进行发行前，或者设置诸如代币类别、名字、最大供应量、谁有权限进行发行代币，是否是同质化的等等属性前，必须执行的一个方法。Name类型是一个由a-z,1-5组成的最长12位字符的字符串。供应量被设置为字符串类型，并且会在内部被转化为dasset类型。对于同质化代币，精度由小数点后的数值决定，和普通的EOS资产相似。

```c++
ACTION create(name issuer, name category, name token_name, bool fungible, bool
              burnable, bool transferable, string base_uri, string max_supply);
```

**ISSUE**: issue函数可以发行一个代币，并且将所有权指定给账户名’to’。对于一个有效的函数调用，代币的`category`和`token_name`都必须都先被创建好了。如果是非同质化或半同质化的代币，则数量必须等于1；如果是同质化代币，数量则必须和`max_supply`的精度匹配。`Metadata_type`必须是已接受的元数据类型模板之一。

```c++
ACTION issue(name to, name category, name token_name, string quantity, string relative_uri,
             string memo);
```
 
 **PAUSEXFER**: 暂停所有代币的所有转账，仅有合约可以调用该函数。如果 pause 参数值为 true，则暂停转账;否则，不暂停。
 
```c++
ACTION pausexfer(bool pause);
```

**BURNNFT**: 该函数可用于销毁第三方代币并且释放内存，只有代币的拥有者 (owner) 可以调用该方法，并且 burnable 参数值必须为 true

```c++
ACTION burnnft(name owner, vector<uint64_t> dgood_ids);
```

**BURNFT**: 该方法用于销毁同质化代币，并且在所有代币都销毁后释放内存
(RAM)，只有拥有者 (owner) 可以调用该方法，并且 burnable 参数值必须为
true

```c++
ACTION burnft(name owner, uint64_t category_name_id, string quantity);
```

**TRANSFERNFT**: 用于非同质化代币的转账。并且允许通过传递代币的 id 列表进行批量转账。仅有代币拥有者 (owner) 可以调用该函数并且 transferable 参数值必须为 true。

```c++
ACTION transfernft(name from, name to, vector<uint64_t> dgood_ids, string memo);
```

**TRANSFERFT**: 该标准的转账方法仅用于同质化代币;数量必须与`max_supply`的精度匹配，只有代币的拥有者 (owner) 可以调用，并且transferable的值必须为true。

```c++
ACTION transferft(name from, name to, name category, name token_name, string quantity, string memo);
```

**LISTSALENFT**: 用于代币合约中出售NFT。只有owner可以调用合约中的这个方法来创建订单，在出售状态，将会把拥有权转交给代币合约。

```c++
ACTION listsalenft(name seller, uint64_t dgood_id, asset net_sale_amount);
```

**CLOSESALENFT**: 在订单没有过期前，可以由出售者取消订单；亦可在订单过期的时候，由任何人来取消订单。订单取消之后，将会把NFT退回给出售者。

```c++
ACTION closesalenft(name seller, uint64_t dgood_id);
```

Dasset 类型
===========

为了支持显示代币精度的能力，我们采用了类似 EOS 的方法，创建了一种资产类型，将数量存储在一个`uint64_t`，并将精度存储在`uint8_t`类型里。像1.23一样具有小数点的值，变成一个uint值为123且精度为2的表示。此外，结构会检查值落在可接受的限制范围内。

有两个原因使我们使用这个类型而不是使用`<eoslib/asset.hpp>` 类型。第一是我们不想代币符号被添加到这个类型里。我们已经有更为复杂的命名结构，只要进行正确的转换和数值存储即可，这会节省下那些不被使用的RAM。第二个原因是我们想通过一个字符串去生成我们自己的资产。

Dasset 结构
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

Token 数据
==========

Token 配置表
------------------

操作合约符号、版本、标准类型和`category_name_id`的单例，就像类别和代币名字对的全局id。该表是在任何代币被创建前必须被创建的，且必须通过`setconfig` 进行定义。作为\"标准中的标准\"的一部分，token配置表将会有版本和标准字段，以便钱包让钱包知道他们将要支持这个合约中的内容。

`category_name_id` 每次都会随着 `create` 的成功调用而自增

```c++
// scope is self
TABLE tokenconfigs {
    name standard;
    string version;
    symbol_code symbol;
    uint64_t category_name_id;
};
```

dGood 数据表
-----------------

确保只有一对`category`, `token_name`对。存储着token是否同质化的，是否可销毁，是否可以进行转账，当前及最大的供应量是多少。token被创建的时候，信息就被写入。由于供应量不会下降，在销毁代币的时候，供应量要记录唯一编码。

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
    string 	base_uri;
     
    uint64_t primary_key() const { return token_name.value; }
};
```

dGood 信息表
----------------

这是非同质化和半同质化token的全局列表。辅助索引提供了按照所有者 (owner) 进行搜索。

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

分类表
--------------

存储所有的分类名称，以便进行快查。

```c++
// scope is self
TABLE categoryinfo {
  name category;

  uint64_t primary_key() const { return category.value; }
};
```

ASKS（卖） 表
--------------

维护内置的去中心化交易所的卖单表

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

账户表
-------------

账户表存储了账户的同质化代币，以及对应账户拥有指定类型 NFT 的数量

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

元数据模板
==================

为了使钱包和 dApp 可以支持各种数字物品，需要有与元数据相关的标准。我们的 方法是根据物品的类型定义模板。以下几个是我们提出的候选模板，但这是一项协作 练习，同时我们希望提供一个被社区所认可的模板仓库。所有的元数据都以 JSON 格 式化，并且必须指定模板的类型。

为了适配多语言，元数据是一组JSON对象数组，每个JSON对象都是单独的语言及翻译，并且很容易被索引。所有的键值都是英文的，但是对应的值是 `lang` 字段相对应的语言。

类型: 3d游戏资产
-----------------

    [{                 
      // 必选字段
      "lang": 字符串; ISO 639-1语言标准的两字母编码
      "name": 字符串; 标识代币所代表的资产
      "description": 字符串; 简述代币所代表的资产
      "imageSmall": 指向图像资源大小为150 x 150的URI
      "imageLarge": 指向图像资源大小为1024x1024的URI
      "3drender": 指向渲染3d图像的js webgl的URI
      "details":  渲染详细内容的键值对，例如{"strength": 5}
      // 可选字段
      "authenticityImage": 指向表示真伪性证书的mime类型图像资源的URI
    }]

类型: 2d游戏资产
-----------------

    [{
      // 必选字段
      "lang": 字符串; ISO 639-1语言标准的两字母编码
      "name": 字符串; 标识代币所代表的资产
      "description": 字符串; 简述代币所代表的资产
      "imageSmall": 指向图像资源大小为150 x 150的URI
      "imageLarge": 指向图像资源大小为1024x1024的URI
      "details": 渲染详细内容的键值对，例如{"strength": 5}
      // 可选字段
      "authenticityImage":指向表示真伪性证书的mime类型图像资源的URI
    }]


类型: 门票
------------

    [{
      // 必选字段
      "lang": 字符串; ISO 639-1语言标准的两字母编码
      "name": 字符串; 标识该门票是用于哪个活动
      "description": 字符串; 简述该门票代表的活动内容
      "location": 字符串; 活动被举办的地理位置
      "address": 字符串; 活动被举办的地址
      "date": Unix时间; 活动开始的日期
      "time": Unix时间; 活动开始的时间
      "imageSmall": 指向图像资源大小为150 x 150的URI
      "imageLarge": 指向图像资源大小为1024x1024的URI
      "details": 渲染详细内容的键值对，例如{"openingAct": "Nickelback"}
      // 可选字段
      "authenticityImage": 指向表示真伪性证书的mime类型图像资源的URI
      "duration": 字符串; l活动持续的时间
      "row": 字符串; 座位位置的行数
      "seat": 字符串; 座位的号码或者代表随意位置的GA
    }]

类型: 艺术
---------

    [{
      // 必选字段
      "lang": 字符串; ISO 639-1语言标准的两字母编码
      "name": 字符串; 标识代币所代表的资产
      "description": 字符串; 简述代币所代表的资产
      "creator": 字符串; 艺术品的创作者
      "imageSmall": 指向图像资源大小为150 x 150的URI
      "imageLarge": 指向图像资源大小为1024x1024的URI
      "date": unix时间; 艺术品被创作的时间
      "details": 渲染详细内容的键值对，例如  {"materials": ["oil", "wood"], "location": "Norton Simon Museum"}
      // 可选字段
      "authenticityImage": 指向表示真伪性证书的mime类型图像资源的URI
    }]

类型: 珠宝
-------------

    [{
      // 必选字段
      "lang": 字符串; ISO 639-1语言标准的两字母编码
      "name": 字符串; 标识代币所代表的珠宝
      "description": 字符串; 简述代币所代表的珠宝
      "imageSmall": 指向图像资源大小为150 x 150的URI
      "imageLarge": 指向图像资源大小为1024x1024的URI
      "image360": 指向图像资源为360图像的URI
      "manufactureDate": Unix时间; 珠宝被生产的时间
      "manufacturePlace": 字符串; 珠宝所被生产的地点
      "details": 渲染详细内容的键值对，例如 {"caret": 1}
      // 可选字段
      "authenticityImage": 指向表示真伪性证书的mime类型图像资源的URI
    }]

定义
===========

-   **内存 (RAM)** - 指 EOS 区块链上的永久存储空间。必须通过发送 EOS 在 EOS 区块链上进行购买，并且可以在不使用的时候进行出售。

-   **账户 (Account)** - EOS 账户最多 12 个字符，包含字母 a-z，数字 1-5 和英文 句号 (如果使用账户服务)

-   **符号 (Symbol)** - 定义为最多仅由 7 个大写字母组成

-   **同质化 (Fungible)** - 在这里指的是像 EOS 一样的可以进行互换的代币，如果代币是同质化的，那转账的代币都是同等的。

-   **半同质化 (Semi-fungible)** - 技术上唯一的代币，他们拥有唯一的标识，但是可能和其他代币共享相同或相似的数据。许多的实体物品都属于这一类别，因为他们除了唯一的序列号外，在其他方面无法区分。

-   **非同质化 (Non-fungible)** - 任何一个代币都是唯一的
















