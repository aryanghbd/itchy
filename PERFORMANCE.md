# Performance: Development and optimization.


## Coming up with what we had:


### Parsing

Alright cool, we have messages, bytes and meanings. How do we actually do anything with these? 

Well we probably need a way to read the messages in, so as such I implemented message reading methods. This was very straight forward thanks to the ITCH format, because I'll always know exactly how much to read by reading in the first two bytes, reading it as big-endian (right shift the first byte by 8 bits and then OR it with the second byte to get the value), then we know exactly how many bytes we need to `gzread()` to get the rest of the payload. `gzread()` automatically advances the cursor anyway, so we can just keep doing that per message.

Since the first 11 bytes form a common header, this can be the first detachable struct.

```
struct MessageHeader {
    uint8_t messageType; 
    uint16_t stockLocate;
    uint16_t trackingNumber;
    uint64_t timestamp; 
};
```

Since converting everything to big-endian is going to be a massive bore, being on an M1 MacBook, I decided to just implement a `ByteReader` class that would read in big-endian bytes of various sizes, this saved me a lot of suffering and despair.

Noticing that first byte in the header, it gives us the type, a consistent ASCII. No need to convert it to ASCII directly, it'll always be the same and we can route by that, saves time. I went ahead and made structs for every individual message class based on the ITCH documentation, detailing their unique fields:

```
struct AddOrder {
    // add order message type
    // an order message looks like this: 41 01 F0 00 00 0D 18 C2 ED 8D A2 00 00 00 00 00 00 22 B1 42 00 00 07 D0 41 52 47 58 20 20 20 20 00 18 80 A8
    // 36 bytes long, first 11 header, next 25 the payload. Order Reference takes up 8 bytes, side takes up the next byte (42 = BUY), shares take up the next 4 bytes, stock takes up the next 8 bytes, price takes up the last 4 bytes.
    static constexpr char type = 'A';
    MessageHeader header;
    uint64_t orderReferenceNumber;
    char buySellIndicator;
    uint32_t shares;
    char stock[8];
    uint32_t price; // this is big endian with 4 implied decimal places
};

struct OrderExecutedWithPrice {
    // order executed with price 'C' type
    // 43 1D B3 00 01 1F 00 63 21 70 99 00 00 00 00 00 90 CB CC 00 00 00 64 00 00 00 00 00 00 C5 1C 4E 00 00 A3 48 
    // looks like this header + pl
    // pl looks like: 8 byte order reference, 4 bytes of executed shares, 8 bytes for match number, 1 byte for printable, 4 bytes for execution price
    static constexpr char type = 'C';
    MessageHeader header;
    uint64_t orderReferenceNumber;
    uint32_t executedShares;
    uint64_t matchNumber;
    char printable;
    uint32_t price; 
};
```

Excellent, once we've done that for everything, we need a way to route a generic message to one of these structs once we've inferred the type, how can we route to a struct from a message? Since there is little shared between messages, I didn't wanna force a gestalt class structure. The parser needs one return type that is capable of holding any one of these structs at a time. We could use a `std::union`, but it doesn't maintain which type is active unless we maintained a separate tag. This means we can still accidentally read some of the other message types and run into undefined behaviour. `std::variant` effectively handles this issue for us as we are able to safely access the current active type via `std::get` or `std::visit`, while also taking care of the cleanup of the active object when we're done with it. There's also some compile-time checking which is handy. There is ... some overhead as an expense of convenience, and a `std::union` would definitely be faster, but this is something we can always tackle later in the priority list of optimisations. 

So now we implement switching within the variant to instantiate the specific message type's struct based on the byte read in the header, we can then set up each message object. Cool, we've now just structurally parsed everything in the ITCH feed. Now we have to actually do something with it.

### Books, Levels, Orders

There are plenty of messages but only a few of them actually matter, repeating from `README.md`, we care about these:

| Type | Message |
| --- | --- |
| `A` | Add Order |
| `F` | Add Order with MPID Attribution |
| `E` | Order Executed |
| `C` | Order Executed with Price |
| `X` | Order Cancel |
| `D` | Order Delete |
| `U` | Order Replace |

Given these all pertain to some mutation of an order, we can homologate these into a common format:

```
class Order {
    // instruction to buy or sell a stock, some given price or quantity.
private:
    uint64_t m_orderReferenceNumber;
    uint16_t m_stockLocate;
    char m_buySellIndicator;
    uint32_t m_price; // big endian with 4 implied decimal places
    uint32_t m_shares; 
    std::list<uint64_t>::iterator m_listIterator; // position of this order's ID within its PriceLevel's FIFO list, for O(1) removal

....
```

Then we can easily track these, and apply mutations (such as order executions, cancellations, deletions, replacements) with a simple class method to reduce share quantity when necessary.

Many orders can rest at the same price. The eventual goal of this market making infrastructure is just to marry up the bids and the asks. This can be made easy if we know exactly how many people are willing to buy something for $100, and taking all the shares and people willing to sell it for $100 and joining those together, prioritising the first people to make the call.

```
$100.00 → total quantity: 700
          FIFO: order 12 → order 19 → order 31
```

That would be the role of the matching engine to do quickly and effectively. To construct that apparatus though, we need an easy way to reflect those prices to be able to match them. We should be able to take a number, and figure out exactly how many shares are being bid or asked for at that number and the specific reference numbers so we have receipts. Hence, we need to implement a price level to keep up with that.

```
struct PriceLevel {
    uint32_t totalQuantity = 0;
    std::list<uint64_t> orderIds; // list of order reference numbers at this price level, FIFO order
};
```
Very nice, we use a list here to allow us to `push_back`, but we can also track the iterator to allow us to cancel orders in O(1) time without invalidation.

This way, each stock can have its own `bids` and `asks`, which can be a mapping of each price to its price level.

```
struct Book {
    // each stock is a book, has bids and asks.
    std::map<uint32_t, PriceLevel, std::greater<>> bids; // sorted descending: best bid first
    std::map<uint32_t, PriceLevel> asks; // sorted ascending: best ask first

    // best bid and best ask,
    // 'best' means the highest bid, or price someone is willing to buy
    // 'best' ask means lowest price someone is willin to sell at

    std::optional<uint32_t> bestBid() const {
        if (bids.empty()) {
            return std::nullopt;
        }
        return bids.begin()->first;
    }

    std::optional<uint32_t> bestAsk() const {
        if (asks.empty()) {
            return std::nullopt;
        }
        return asks.begin()->first;
    }
```

Then the `OrderBook` just accumulates `Book`s and mutates them as events come in. Each order type can have an `apply()` method that mutates the relevant information, then with some clever templating, we can infer the order message type and invoke its unique `apply()` call on the book seamlessly.

```
class OrderBook {
    private:
        unordered_map<uint64_t, Order> m_orderIndex;
        unordered_map<uint16_t, Book> m_books;
.....
```

```
std::visit([&](const auto& message) {
    using T = std::decay_t<decltype(message)>;

    if constexpr (std::is_same_v<T, AddOrder>) {
        orderBook.apply(message);
    } else if constexpr (std::is_same_v<T, OrderDelete>) {
        orderBook.apply(message);
    }
}, parsedMessage);
```

Boom, in short, that is an L3 Order Book more or less fleshed out.


## Profiling and Optimization.

## Setup

Dataset: `12302019.NASDAQ_ITCH50.gz`

- 268,744,780 messages;
- all symbols reconstructed;
- gzip decompression included in wall time;
- `--symbol=AAPL` only controls the final depth query.

Benchmark command:

```bash
time ./build/itch-lob --symbol=AAPL
```

## The first run

The first full replay took a while.

```text
real    10m27.127s
user    10m21.424s
sys     0m2.834s
```

That is 627.1 seconds, or about 0.43 million messages per second. I was happy that it worked without any malformed messages or breaks, but this could be much better.

## `-O2` my beloved

The initial build was unoptimized. Rather than reinvent the wheel, we can leverage the compiler to carry a lot of the heavy lifting for us. We have many functions in our implementation that could probably be inlined, maybe loops that could be unrolled, dead or redundant code that could be eliminated.  Enabling `-O2` allows us these aggressive compiler optimizations. When doing this, we brought the replay down to:

```text
real    3m21.230s
user    3m16.960s
sys     0m1.616s
```

201.2 seconds, or 1.34 million messages per second. Easy 3.1x.

The remaining work used this as the useful baseline. Otherwise most of the claimed improvement would just be a compiler flag (as nice as that is).

You can see some of the improvements from the optimization report were a lot of inlining and a lot of unrolling of some of my `ByteReader.readUINTXBE()` methods, which makes sense since these loops execute a fixed number of times without difference for however many bytes we're reading, look:

```
--- !Passed
Pass:            loop-unroll
Name:            FullyUnrolled
DebugLoc:        { File: './byte_reader.hpp', Line: 51, Column: 9 }
Function:        _ZN10ByteReader12readUInt48BEEv
Args:
  - String:          'completely unrolled loop with '
  - UnrollCount:     '6'
  - String:          ' iterations'
...
```

## finding the hot paths

I used the macOS sampling profiler against a running replay:

```bash
./build/itch-lob --symbol=AAPL > /tmp/itch_run.log 2>&1 &
PID=$!

sample "$PID" 20 -file /tmp/itch_sample.txt
wait "$PID"
```

The top application frames were fairly unsurprising:

```text
OrderBook::removeOrder(...)    5645
OrderBook::addOrder(...)       3340
_xzm_free                      1184
unordered_map::emplace          928
```

The full replay called both book operations a lot:

```text
addOrder calls:    140270523
removeOrder calls: 140270523
```

Naturally, we spend a lot of time within a given sampling interval either adding or removing an order. Them's the breaks. Do the math:

![alt text](image.png)

Most of our gains will come from making these methods faster, so what was making it so slow?


## Theory 1: PriceLevel allocation 

We have

```
PriceLevel& level = book.bids[newOrder.price()];
```

in both methods. Where one doesn't exist, we have to allocate a new node and tree traverse. There's probably loads of different prices being sent in very quickly, surely there must be a wide range without much commonality, especially given the granularity of decimal placements. This got me thinking: how often are we 'hitting', i.e accessing an existing price level index, vs 'missing', i.e indexing at a new price not seen before and having to reallocate. I added a counter to see if we were being burnt by this or not:

```
Hits: 84.65%
Misses: 15.35%
```

So on average we were hitting fairly often, so the map traversal is fine for now.

## Theory 2: The verifier was expensive and unnecessary


```
        bool verifyPriceLevel(const PriceLevel& level) const {
            uint32_t total = 0;
            for (uint64_t orderId : level.orderIds) {
                auto it = m_orderIndex.find(orderId);
                if (it != m_orderIndex.end()) {
                    total += it->second.shares();
                }
            }
            return total == level.totalQuantity;
        }
```

...

```
        void addOrder(const Order& order) {
            Order newOrder = order;
            Book& book = m_books[newOrder.stockLocate()];

            if (newOrder.buySellIndicator() == 'B') {
                PriceLevel& level = book.bids[newOrder.price()];
                level.totalQuantity += newOrder.shares();
                level.orderIds.push_back(newOrder.orderReferenceNumber());
                newOrder.setListIterator(std::prev(level.orderIds.end()));
                m_orderIndex.insert_or_assign(newOrder.orderReferenceNumber(), newOrder);
                if(verifyPriceLevel(level) == false) {
                    throw runtime_error("Price level total quantity does not match sum of order shares");
                }
            }
```

During development, every mutation re-summed the orders at a price level and checked the result against its cached quantity. Useful invariant, but probably a bad place to run it. This invariant check loops over all orders EVERY order. This is quite wasteful, and we could either trust the mutations or run more periodic checks, perhaps amortize or distributing the cost over a span of a few million messages. I decided to just remove it.


Doing this got us down to 125–131 seconds, or 2.05–2.15 million messages per second.

```
- addOrder: 1,937 / 16,002 = 12.10% → roughly 108 ns/call
- removeOrder: 4,880 / 16,002 = 30.50% → roughly 272 ns/call
```

The check should come back as a test/debug option rather than running after every production update.

## Theory 3: The FIFO was not free

```
level.orderIds.push_back(newOrder.orderReferenceNumber());
```

The first FIFO used `std::list<uint64_t>`. Removal was constant-time because the order index stored the relevant iterator, but each resting order still needed its own list-node allocation. Working at latencies this low you can't just hand wave it off and say O(1) is ok, because even the constant can be bad.

Each alloc entails a:

```
order ID + previous pointer + next pointer + allocator metadata
```

Not only that constant cost, but the fact is that `std::list` is a doubly linked list with heap allocations. When you traverse across it there's going to be a lot of cache misses because we're just pointing to random locations in the heap and resolving it. We need to improve on this.

A diagnostic version that temporarily replaced the FIFO with a simple count ran in about 108 seconds versus 131 seconds with the list. It was not a valid book implementation, but it made the allocation cost fairly obvious.

### The deque idea was bad

I tried a deque with lazy deletion to get chunked storage without losing cheap front/back operations. My idea being that a `std::deque` was gonna have contiguous chunks so that we would in essence be able to pre-fetch some of the following items and save time. It did not save time.

It ran in 141 seconds and memory grew from roughly 250 MB to 3.1 GB. Stale order IDs accumulated faster than they were cleaned up, so that version went away.

### Improved layout

`try_emplace` and `.erase` from mOrderIndex is now our bottleneck.

The current version keeps each `Order` directly inside the order index and stores `previous`/`next` pointers on the order itself.

Each price level only needs a head, tail, and cached total quantity. Once an order is found in the hash table, it can be unlinked without scanning the level or allocating a second container node. A flat address mapped hashing hashing structure with a pool allocator allows us to, instead of just using buckets via pointers, have a flat section of memory that is claimed and rescinded.

The order index is a `std::pmr::unordered_map` backed by `std::pmr::unsynchronized_pool_resource`. Erased node storage can be reused by later inserts instead of going back through the general-purpose allocator every time. Pool allocation

That version ran in 106 seconds: about 2.54 million messages per second.

## results

| Version | Wall time | Throughput | Notes |
| --- | ---: | ---: | --- |
| Initial build | 627.1 s | 0.43M msg/s | Unoptimized baseline |
| `-O2` | 201.2 s | 1.34M msg/s | Compiler optimization |
| No per-mutation level scan | 125–131 s | 2.05–2.15M msg/s | Invariant removed from hot path |
| Deque + lazy deletion | 141 s | 1.91M msg/s | Rejected; 3.1 GB memory |
| Intrusive FIFO + pooled index | 106 s | 2.54M msg/s | Current version |

Overall: 5.9x faster than the first run and 1.9x faster than the `-O2` baseline. 

## Caveats

- These are end-to-end throughput numbers, not per-message latency percentiles.
- The runs include gzip decompression, parsing, book mutation, and final reporting.
- Most entries are individual representative runs rather than repeated-run medians.
- Exact hardware, compiler, and peak memory for the final version still need recording.
- This is offline replay performance, not a claim about production feed-handler latency.

## Next measurements

- report the median of several identical runs;
- record the exact machine, compiler, and peak resident memory;
- benchmark decoding separately from book mutation;
- add a debug build that periodically checks book invariants.
