# itchy

ITCH 5.0 decoder and L3 order book in C++20. Why'd I build it? Because I was bored. The matching engine shall be known as `scratchy` (get it?).

## the short version

`itchy` is a C++20 NASDAQ TotalView-ITCH 5.0 replay engine. It streams a gzipped binary feed, performs bounds-checked big-endian decoding into typed `std::variant` messages, and applies add, execute, cancel, delete, and replace events to reconstruct full-depth L3 order books for every symbol in the feed.

The book is built around the hot path: orders live in a pool-backed `std::pmr::unordered_map` for expected O(1) lookup, while intrusive previous/next pointers preserve FIFO priority without a second container allocation. Ordered bid/ask maps provide O(log P) price-level access, O(1) order linking and unlinking, O(1) best bid/ask access, and O(N) top-of-book traversal.

On a 268,744,780-message replay, the current implementation processed 2.54 million messages/second in 106 seconds—including gzip decompression, decoding, and book updates. That is 5.9× faster than the initial build and 1.9× faster than the plain `-O2` baseline. Build with `make`, replay with `./build/itch-lob <feed.gz> --symbol=AAPL`, and see [PERFORMANCE.md](PERFORMANCE.md) for the tuning log and failed experiments.

## what it does

- streams a gzipped NASDAQ TotalView-ITCH 5.0 feed;
- decodes ITCH binary streams into custom typed messages;
- maintains per-symbol bids, asks, price levels, and individual orders;
- applies adds, executions, cancels, deletes, and replacements;
- Constructs FIFO price-level megastructure per stock locate (or stock).
- returns best bid/ask and top-N depth.

I tried to exercise more of the modern C++ features to build it rather than remain a luddite, so parsed message structs are wrapped with `std::variant` for example as a safer tagged union with active type monitoring. We parse every message, but the ones we actually care about for the book are:

| Type | Message |
| --- | --- |
| `A` | Add Order |
| `F` | Add Order with MPID Attribution |
| `E` | Order Executed |
| `C` | Order Executed with Price |
| `X` | Order Cancel |
| `D` | Order Delete |
| `U` | Order Replace |

For replay purposes, we maintain the ordering of administrative, directory, trade, auction, and regulatory messages. I'll explain what the letters mean further down if you're bamboozled.

## build

Requires a C++20 compiler, `make`, and zlib.

```bash
make
```

This builds `build/itch-lob` with `-O2`.

## data

The feed is not included. Pass the path to any gzipped ITCH 5.0 feed as the
first argument. For example, to download the December 30, 2019 feed:

```bash
mkdir -p data
curl -L \
  'https://emi.nasdaq.com/ITCH/Nasdaq%20ITCH/12302019.NASDAQ_ITCH50.gz' \
  -o data/12302019.NASDAQ_ITCH50.gz
```

The download is about 3.5 GB. When you decompress the thing it can run ~10GB. I'm not boujie like that, so I don't have a premium on laptop storage, so I only tested with a couple days worth of trading events, however this will work with other ITCH files from the repository to construct order books.

## run

```bash
./build/itch-lob <feed.gz> --symbol=AAPL [--depth=5] [--limit=N] [--verbose]
```

```bash
# full replay
./build/itch-lob data/12302019.NASDAQ_ITCH50.gz --symbol=AAPL

# intraday snapshot
./build/itch-lob data/12302019.NASDAQ_ITCH50.gz --symbol=AAPL --depth=10 --limit=N

# print applied book events; probably use a small limit
./build/itch-lob data/12302019.NASDAQ_ITCH50.gz --symbol=AAPL --limit=N --verbose
```

`--symbol` only controls the final query. The full multi-symbol book is still reconstructed.

Prices are stored as integers with four implied decimal places. `123450` means `$12.3450`.

## whats an order book?

An order book is a data structure, a tableaux if you will, containing a live list of all active buy and sell orders for a specific asset. For a specific asset (stock, commodity, etc), or 'book' in my code, it divides into 'bids' - a list of buy orders, or how how much people are willing to pay and the quantity they wanna buy, and 'asks' - a list of sell orders, or what price people are selling it for and how many they'll going to offload. Offer per share and request per share if you will.

## book layout

We can lay out each stock's `Book` by mapping its  stock-locate code to two ordered maps for `bids` and `asks`.

Orders live in a pool-allocated hash table keyed by order-reference number. Each `Order` carries its own previous/next pointers, creating a makeshift intrusive FIFO at its price level. We'll then key in that order's information into the referenced stock locate's asks or bids `PriceLevel` index. I'll ramble a bit more in `PERFORMANCE.md` about how it ended up being structured this way, but for now here are some more specific performance deets about the final implementation of registering, acking orders and market actions:

| Operation | Structure | Complexity |
| --- | --- | --- |
| Find order | `std::pmr::unordered_map` | Expected O(1) |
| Find/create price level | `std::map` | O(log P) |
| Append/unlink order | Intrusive FIFO | O(1) |
| Best bid/ask | First map entry | O(1) |
| Top N levels | Ordered iteration | O(N) |

`P` is the number of active price levels on one side of a book.

tl;dr: Accumulate and reflect new orders and form price levels where price maps ~ [total quantity, orderIds]. There can be many price levels for one asset of either `bid` or `ask`, accumulated separately, both forming the `Book`. We accumulate books over the course of the days' events to form the `OrderBook`.

## whats itch

ITCH is a binary protocol used by NASDAQ to transmit stock market updates. Why binary? Because it's cooler, and also because it makes the data transfer fast and lightweight. People are relying on making very fast decisions based on the data received from this; keeping it binary makes it portable.


## what do the messages look like?

Once you've grabbed a file, you can read in the bytes of the file to `stdout`, and you'll probably see sequences looking like this.

```
00 24 41 01 F0 00 00 0D 18 C2 ED 8D A2 00 00 00 00 00 00 22 B1 42 00 00 07 D0 41 52 47 58 20 20 20 20 00 18 80 A8
```

Very poetic. What does it mean? 

Bytes 0-1 - Length of message: `00 24` in big-endian (i.e left to right) == 36 bytes of payload that follow. Each message type has a different length of payload, delightful.

The next 11 bytes form a common message header and are shared structurally between the messages:

Byte 2 - Message type: This byte marks what kind of message we're reading. This matters because we'll decode the bytes following the header differently depending on the type. `41` maps to ASCII `A`, which is an Add Order (no MPID) message. Exciting.

Bytes 3-4 - Stock locate: Numeric representation of a specific stock. It makes for a quicker lookup than the ticker. Delightfully, this changes by the day. In this case, `01 F0` maps to 496, which happens to be `ARGX` on this particular day.

Bytes 5-6 - Tracking number: A two-byte internal tracking number assigned by NASDAQ. In this message it is `00 00`. Just some feed metadata.

Bytes 7-12 - Timestamp: number of nanoseconds since midnight. Here, `0D 18 C2 ED 8D A2` is 14,400,000,724,386 nanoseconds, or `04:00:00.000724386`.

Everything else that follows is specific to the message type. Because this is an `A` message, the remaining 25 bytes form this payload:

Bytes 13-20 - Order reference number: `00 00 00 00 00 00 22 B1` maps to 8,881. This is the unique identifier later execution, cancellation, deletion, and replacement messages use to find the order.

Byte 21 - Buy/sell indicator: `42` maps to ASCII `B`, so this is a buy order. A sell order would contain ASCII `S`, or `53` in hex.

Bytes 22-25 - Shares: `00 00 07 D0` maps to 2,000 shares, what a baller.

Bytes 26-33 - Stock: `41 52 47 58 20 20 20 20` is the ASCII symbol `ARGX`, padded to eight bytes with spaces.

Bytes 34-37 - Price: `00 18 80 A8` maps to 1,605,800. ITCH prices have four implied decimal places, making the actual limit price `$160.5800`.

In plain English, this message means: At `04:00:00.000724386`, NASDAQ added buy order 8,881 for 2,000 shares of `ARGX` at `$160.5800`. 

Ok so now that we know what a message looks like, what are the other types of message?

The common 11-byte header is omitted below. The field list follows the official [Nasdaq TotalView-ITCH 5.0 specification](https://classic.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/NQTVITCHSpecification.pdf).

| Type | Message | Fields after the common header |
| --- | --- | --- |
| `S` | System Event | Event code |
| `R` | Stock Directory | Stock, market category, financial status, round-lot size/rules, issue classification/subtype, authenticity, short-sale threshold, IPO flag, LULD tier, ETP attributes |
| `H` | Stock Trading Action | Stock, trading state, reserved byte, reason |
| `Y` | Reg SHO Short-Sale Price Test | Stock, Reg SHO action |
| `L` | Market Participant Position | MPID, stock, primary-market-maker flag, market-maker mode, participant state |
| `V` | Market-Wide Circuit Breaker Decline Levels | Level 1, Level 2, and Level 3 prices |
| `W` | Market-Wide Circuit Breaker Status | Breached level |
| `K` | IPO Quoting Period Update | Stock, quotation release time, release qualifier, IPO price | 
| `J` | LULD Auction Collar | Stock, reference price, upper/lower collar prices, auction extensions |
| `A` | Add Order | Order reference, side, shares, stock, price |
| `F` | Add Order with MPID Attribution | Add Order fields plus MPID attribution | 
| `E` | Order Executed | Order reference, executed shares, match number |
| `C` | Order Executed with Price | Order reference, executed shares, match number, printable flag, execution price |
| `X` | Order Cancel | Order reference, canceled shares | 
| `D` | Order Delete | Order reference | 
| `U` | Order Replace | Original/new order references, new shares, new price |
| `P` | Non-Cross Trade | Order reference, side, shares, stock, price, match number | 
| `Q` | Cross Trade | Shares, stock, cross price, match number, cross type | 
| `B` | Broken Trade | Match number |
| `I` | Net Order Imbalance Indicator | Paired/imbalance shares, direction, stock, far/near/current-reference prices, cross type, price-variation indicator | 
| `N` | Retail Price Improvement Indicator | Stock, retail-interest indicator |


## tl;dr performance optimization

Full replay: 268,744,780 messages. Wall time includes gzip decompression, decoding, and book updates.

| Version | Time | Throughput |
| --- | ---: | ---: |
| Initial build | 627.1 s | 0.43M msg/s |
| `-O2` | 201.2 s | 1.34M msg/s |
| custom optimisations (Intrusive FIFO + pooled order index + profiling hot paths/wasteful allocs and replacing) | 106 s | 2.54M msg/s |

Took our hot paths (adding and removing orders) down from ~400ns per op to ~90ns per op. I'm going to keep improving on it.

The longer version, including the profiler output and a deque experiment that went horrendously wrong, is in [PERFORMANCE.md](PERFORMANCE.md).

## files

| File | Job |
| --- | --- |
| `message_reader.hpp` | gzip input and message framing |
| `byte_reader.hpp` | bounds-checked big-endian reads |
| `itch_messages.hpp` | ITCH message structs |
| `itch_parser.hpp` | binary decoding |
| `order_book.hpp` | L3 book and event application |
| `main.cpp` | replay loop and CLI |

## references

- [NASDAQ TotalView-ITCH 5.0 specification](https://assets.ctfassets.net/mx0rke14e5yt/5Uz6MGJxbo4wRPou8KveFs/c511c075a3632b277dbfc47341551634/2-13_TVITCH_5.0.pdf)
- [NASDAQ historical ITCH files](https://emi.nasdaq.com/ITCH/Nasdaq%20ITCH/)
