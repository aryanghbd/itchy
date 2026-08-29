#include <iostream>
#include <zlib.h>
#include <vector>
using namespace std;
#include <array>
#include <set>
#include <map>
#include <stdexcept>
#include <variant>
#include <list>
#include <optional> 
// methods to read to big endian

#include <span>

class ByteReader {
private:
    std::span<const uint8_t> m_bytes;
    size_t m_position = 0;

    void require(size_t count) const {
        if (count > m_bytes.size() - m_position) {
            throw runtime_error("Unexpected end of message");
        }
    }

public:
    explicit ByteReader(std::span<const uint8_t> bytes)
        : m_bytes(bytes) {}

    uint8_t readUInt8() {
        require(1);
        return m_bytes[m_position++];
    }

    uint16_t readUInt16BE() {
        require(2);
        uint16_t value =
            (static_cast<uint16_t>(m_bytes[m_position]) << 8) |
            static_cast<uint16_t>(m_bytes[m_position + 1]);
        m_position += 2;
        return value;
    }

    uint32_t readUInt32BE() {
        require(4);
        uint32_t value =
            (static_cast<uint32_t>(m_bytes[m_position]) << 24) |
            (static_cast<uint32_t>(m_bytes[m_position + 1]) << 16) |
            (static_cast<uint32_t>(m_bytes[m_position + 2]) << 8) |
            static_cast<uint32_t>(m_bytes[m_position + 3]);
        m_position += 4;
        return value;
    }

    uint64_t readUInt48BE() {
        require(6);
        uint64_t value = 0;
        for (size_t i = 0; i < 6; ++i) {
            value = (value << 8) | m_bytes[m_position + i];
        }
        m_position += 6;
        return value;
    }

    uint64_t readUInt64BE() {
        require(8);
        uint64_t value = 0;
        for (size_t i = 0; i < 8; ++i) {
            value = (value << 8) | m_bytes[m_position + i];
        }
        m_position += 8;
        return value;
    }

    void readChars(char* destination, size_t count) {
        require(count);
        for (size_t i = 0; i < count; ++i) {
            destination[i] = static_cast<char>(m_bytes[m_position + i]);
        }
        m_position += count;
    }
};
struct MessageHeader {
    // most itch message types share the first 11 payload bytes.
    uint8_t messageType; 
    uint16_t stockLocate;
    uint16_t trackingNumber;
    uint64_t timestamp; 
};

struct SystemEvent {
    // system event message type
    // a System Event will look like this 53 00 00 00 00 0a 11 ea 0e 8c 43 4f
    // first 11 bytes being the header, the last byte is the event code, this case is 4f or 'O', start of messages.
    static constexpr char type = 'S';
    MessageHeader header;
    char eventCode;
};

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

struct OrderDelete {
    // eg: 44 19 F4 00 00 0D 18 C4 05 8F A2 00 00 00 00 00 00 03 D7
    // 11 byte header, 8 byte order reference
    static constexpr char type = 'D';
    MessageHeader header;
    uint64_t orderReferenceNumber;
};

struct OrderExecuted {
    // distinguished because it lacks a price.. duh
    // eg: 45 1B 36 00 02 0D 18 D4 F6 B6 90 00 00 00 00 00 00 99 F3 00 00 00 03 00 00 00 00 00 00 45 7F 
    // 11 byte header, 8 byte order reference, 4 byte executed shares, 8 byte match number

    static constexpr char type = 'E';
    MessageHeader header;
    uint64_t orderReferenceNumber;
    uint32_t executedShares;
    uint64_t matchNumber;
};

struct AddOrderWithMPID {
    // distinguished because it has an MPID at the end
    // eg: 49 01 F0 00 00 0D 18 C2 ED 8D A2 00 00 00 00 00 00 22 B1 42 00 00 07 D0 41 52 47 58 20 20 20 20 00 18 80 A8 
    // header + order reference + buy/sell indicator + shares + stock + price + MPID 
    static constexpr char type = 'F';
    MessageHeader header;
    uint64_t orderReferenceNumber;
    char buySellIndicator;
    uint32_t shares;
    char stock[8];
    uint32_t price; // this is big endian with 4 implied decimal places
    char mpid[4]; //mpid is market participant identifier, 4 bytes long, usually 4 ascii characters, tied to some market maker
};

struct StockTradingAction {
    // type H
    // eg: 48 00 01 00 00 0A 53 A2 BA 7F 6D 41 20 20 20 20 20 20 20 54 20 20 20 20 20 
    // header + stock (8) + trading state (1) + reserved (1) + reason (4)
    static constexpr char type = 'H';
    MessageHeader header;
    char stock[8];
    char tradingState;
    char reserved;
    char reason[4];
};

struct NetOrderImbalanceIndicator {
    //eg: 49 11 D9 00 00 1E FE DE 53 1C 26 00 00 00 00 00 00 09 81 00 00 00 00 00 00 00 00 4E 4B 45 51 55 20 20 20 20 00 02 1C A0 00 02 1C A0 00 02 1C A0 4F 4C
    // header + paired shares (8) + imbalance shares (8) + direction (1) + stock (8) + far price (4) + near price (4) + cross type (1) + price variation (1)

    // they calculate the price that would maximise paired shares in the opening cross.
    static constexpr char type = 'I';
    MessageHeader header;
    uint64_t pairedShares;
    uint64_t imbalanceShares;
    char direction;
    char stock[8];
    uint32_t farPrice;
    uint32_t nearPrice;
    char crossType;
    char priceVariation;
};

struct LULDAuctionCollar {
    // type J
    // eg: 4A 21 A7 00 00 20 4D EB 9C 32 87 57 4B 45 59 20 20 20 20 00 02 DA 78 00 02 FF 30 00 02 55 A8 00 00 00 00
    // header + stock (8) + collar reference price (4) + upper auction collar price (4) + lower auction collar price (4) + auction extensions (4)
    static constexpr char type = 'J';
    MessageHeader header;
    char stock[8];
    uint32_t collarReferencePrice;
    uint32_t upperAuctionCollarPrice;
    uint32_t lowerAuctionCollarPrice;
    uint32_t auctionExtensions;
};

struct IPOQuotingPeriodUpdate {
    // type K
    // eg: 4B 00 00 00 00 18 C8 55 F3 ED D6 4D 4B 44 20 20 20 20 20 00 00 94 D4 41 00 00 D2 28
    // header + stock(8) + quotation release time (4) + release qualifier (1) + ipo price (4)
    static constexpr char type = 'K';
    MessageHeader header;
    char stock[8];
    uint32_t quotationReleaseTime;
    char releaseQualifier;
    uint32_t ipoPrice;
};

struct MarketParticipantPosition {
    // type L
    // eg: 4C 12 69 00 00 0A 56 30 98 E7 08 43 44 52 47 4C 41 5A 20 20 20 20 20 59 4E 41
    // header + mpid (4) + stock (8) + primary market maker (1) + market maker mode (1) + market participant state (1)
    static constexpr char type = 'L';
    MessageHeader header;
    char mpid[4];
    char stock[8];
    char primaryMarketMaker;
    char marketMakerMode;
    char marketParticipantState;
};

struct TradeMessageNonCross {
    // type P
    // eg: 50 0A CC 00 02 0D 18 F8 90 10 B2 00 00 00 00 00 00 00 00 42 00 00 00 0A 46 43 45 4C 20 20 20 20 00 00 33 90 00 00 00 00 00 00 45 84
    // "At 04:00:00.900567218, ten shares of FCEL traded at $1.32. Nasdaq assigned the execution match number 17,796"
    // header + order reference (8) + buy/sell indicator (1) + shares (4) + stock (8) + price (4) + match number (8)
    static constexpr char type = 'P';
    MessageHeader header;
    uint64_t orderReferenceNumber;
    char buySellIndicator;
    uint32_t shares;
    char stock[8];
    uint32_t price;
    uint64_t matchNumber; 
};

struct CrossTrade {
    // type Q
    // eg: 51 0E 58 00 02 1F 1A CE E9 1E FC 00 00 00 00 00 00 18 29 48 44 53 20 20 20 20 20 00 06 2B 4C 00 00 00 00 00 00 D0 F9 4F 
    // "At 09:30:00.000995068, Nasdaq completed HDS’s opening cross. A total of 6,185 shares matched at $40.43, under match number 53,497"
    // header + shares (8) + stock (8) + cross price (4) + match number (8) + cross type (1)
    static constexpr char type = 'Q';
    MessageHeader header;
    uint64_t shares;
    char stock[8];
    uint32_t crossPrice;
    uint64_t matchNumber;
    char crossType;
};

struct StockDirectory {
    // type R
    // eg: 52 00 01 00 00 0A 53 A2 88 70 58 41 20 20 20 20 20 20 20 4E 20 00 00 00 64 4E 43 5A 20 50 4E 20 31 4E 00 00 00 00 4E
    // "At 03:09:14.325413976, Nasdaq published the daily directory entry for symbol A. It is a live NYSE-listed common stock with stock-locate code 1 and a round-lot size of 100 shares. Odd lots are permitted, it is a Tier 1 LULD security, and it is neither an ETP nor inverse."
    // header + stock (8) + market category (1) + financial status indicator (1) + round lot size (4) + round lots only (1) + issue clarification (1) + issue subtype (2) + authenticity (1) + short-sale threshold (1) + ipo flag (1) + luld tier (1) + etp flag (1) + etp leverage factor (4) + inverse indicator (1)
    static constexpr char type = 'R';
    MessageHeader header;
    char stock[8];
    char marketCategory;
    char financialStatusIndicator;
    uint32_t roundLotSize;
    char roundLotsOnly;
    char issueClarification;
    char issueSubtype[2];
    char authenticity;
    char shortSaleThreshold;
    char ipoFlag;
    char luldTier;
    char etpFlag;
    uint32_t etpLeverageFactor;
    char inverseIndicator;
};

struct OrderReplace {
    // type U
    // eg: 55 0E E8 00 00 0D 18 C4 C1 03 F3 00 00 00 00 00 00 00 46 00 00 00 00 00 00 34 BE 00 00 01 F4 00 05 F8 E8
    // "At 04:00:00.031359987, order 70 was replaced by order 13,502, containing 500 shares at $39.1400."
    // operates as an atomic cancel and add
    // header + original order reference (8) + new order reference (8) + new share quantity (4) + new price (4)
    static constexpr char type = 'U';
    MessageHeader header;
    uint64_t originalOrderReferenceNumber;
    uint64_t newOrderReferenceNumber;
    uint32_t newShares;
    uint32_t newPrice;
};

struct MarketWideCircuitBreakerDeclineLevels {
    // type V
    // eg: 56 00 00 00 00 16 EC BC 77 60 D2 00 00 00 46 28 21 94 40 00 00 00 41 A1 6A B8 40 00 00 00 3C 59 95 62 40
    // "At 07:00:06.030033106, Nasdaq published that the market-wide circuit-breaker thresholds were 3013.21 for Level 1, 2818.81 for Level 2, and 2592.01 for Level 3."

    // header + level 1 (8) + level 2 (8) + level 3 (8)
    static constexpr char type = 'V';
    MessageHeader header;
    uint64_t level1;
    uint64_t level2;
    uint64_t level3;
};

struct OrderCancel {
    // type X
    // eg: 58 14 B5 00 00 0D 18 C4 DB AF CC 00 00 00 00 00 00 21 33 00 00 01 F4 
    // At 04:00:00.033107916, 500 shares were canceled from order 8,499, associated with stock-locate code 5,301.
    // header + order reference (8) + canceled shares (4)
    static constexpr char type = 'X';
    MessageHeader header;
    uint64_t orderReferenceNumber;
    uint32_t canceledShares;
};

struct RegSHOShortSalePriceTest {
    // type Y
    // eg: 59 00 01 00 00 0A 53 A2 BB EE 7F 41 20 20 20 20 20 20 20 30
    // "At 03:09:14.328788607, Nasdaq reported that symbol A, stock-locate 1, was not subject to the Reg SHO short-sale price test."

    // header + stock (8) + reg sho action (1)
    static constexpr char type = 'Y';
    MessageHeader header;
    char stock[8];
    char regSHOAction;
};



class Message {
    // class for a single ITCH message from the stream.
    private:
        vector<uint8_t> m_buffer;
    public:
        // constructor to create an empty message
        Message() = default;

        // constructor to create a message of a given size
        Message(size_t size) {
            m_buffer.resize(size);
        }

        // method to get the size

        size_t size() const {
            return m_buffer.size();
        }

        // method to get the data itself
        const uint8_t* data() const {
            return m_buffer.data();
        }

        // get data (mutable since it returns the pointer.)
        uint8_t* data() {
            return m_buffer.data();
        }
};

class MessageReader {
    private:
        gzFile m_file;

        bool readExact(uint8_t* destination, size_t bytesToRead) {
            size_t totalRead = 0;
            while (totalRead < bytesToRead) {
                int bytesRead = gzread(
                    m_file,
                    destination + totalRead,
                    static_cast<unsigned int>(bytesToRead - totalRead)
                );

                if (bytesRead < 0) {
                    int errorNumber = 0;
                    const char* errorMessage = gzerror(m_file, &errorNumber);
                    throw runtime_error(errorMessage);
                }

                if (bytesRead == 0) {
                    if (totalRead == 0) {
                        return false;
                    }
                    throw runtime_error("Unexpected end of file");
                }

                totalRead += static_cast<size_t>(bytesRead);
            }

            return true;
        }

    public:
        // constructor to open the file
        MessageReader(const char* filename) {
            m_file = gzopen(filename, "rb");
            if (!m_file) {
                throw runtime_error("Failed to open gzip file");
            }
        }

        // destructor to close the file
        ~MessageReader() {
            if (m_file) {
                gzclose(m_file);
            }
        }

        // read a message from the file
        bool readMessage(Message& message) {
            // read the size of the message from the stream (first 2 bytes indicate size to read in bytes)
            uint16_t size; // size of the message in bytes from first 2 bytes of the stream

            // read first 2 bytes to get the size of the message

            // funky caveat: ITCH messages are big endian
            // so the first two bytes being 00 27 for example would be read as 0x2700 = 9984
            // no bueno, we wanna read this as 0x0027, so gotta do some funky shit to swap the bytes around

            
            uint8_t sizeBytes[2];
            if (!readExact(sizeBytes, 2)) {
                return false;
            }
            // just right shift the first byte by 8 bits then OR with the second byte.
            size = (sizeBytes[0] << 8) | sizeBytes[1];

            if (size == 0 || size > 50) {
                throw runtime_error("Invalid ITCH message size");
            }

            // now we can read the rest of the message exactly.
            message = Message(size);
            if (!readExact(message.data(), size)) {
                throw runtime_error("Unexpected end of file while reading message");
            }
            return true;
        }

        void printMessage(const Message& message) {
            // print the message in hex format
            for (size_t i = 0; i < message.size(); ++i) {
                printf("%02X ", message.data()[i]);
            }
            printf("\n");
        }
};

class Order {
    // instruction to buy or sell a stock, some given price or quantity.
private:
    uint64_t m_orderReferenceNumber;
    uint16_t m_stockLocate;
    char m_buySellIndicator;
    uint32_t m_price; // big endian with 4 implied decimal places
    uint32_t m_shares; 
    std::list<uint64_t>::iterator m_listIterator; // position of this order's ID within its PriceLevel's FIFO list, for O(1) removal

public:
    Order(
        uint64_t orderReferenceNumber,
        uint16_t stockLocate,
        char buySellIndicator,
        uint32_t price,
        uint32_t shares

    ) : m_orderReferenceNumber(orderReferenceNumber),
        m_stockLocate(stockLocate),
        m_buySellIndicator(buySellIndicator),
        m_price(price),
        m_shares(shares) {}
        
    // getters
    uint64_t orderReferenceNumber() const { return m_orderReferenceNumber; }
    uint16_t stockLocate() const { return m_stockLocate; }
    char buySellIndicator() const { return m_buySellIndicator; }
    uint32_t price() const { return m_price; }
    uint32_t shares() const { return m_shares; }
    std::list<uint64_t>::iterator listIterator() const { return m_listIterator; }

    // setters
    void setListIterator(std::list<uint64_t>::iterator it) { m_listIterator = it; }

    void reduceShares(uint32_t amount) {
        if (amount > m_shares) {
            throw runtime_error("Cannot reduce shares below zero");
        }
        m_shares -= amount;
    }
};

struct PriceLevel {
    uint32_t totalQuantity = 0;
    std::list<uint64_t> orderIds; // list of order reference numbers at this price level, FIFO order
};

struct Book {
    // each stock is a book, has bids and asks.
    map<uint32_t, PriceLevel, std::greater<>> bids; // sorted descending: best bid first
    map<uint32_t, PriceLevel> asks; // sorted ascending: best ask first

    // best bid and best ask,
    // 'best' means the highest bid, or price someone is willing to buy
    // 'best' ask means lowest price someone is willin to sell at

    optional<uint32_t> bestBid() const {
        if (bids.empty()) {
            return nullopt;
        }
        return bids.begin()->first;
    }

    optional<uint32_t> bestAsk() const {
        if (asks.empty()) {
            return nullopt;
        }
        return asks.begin()->first;
    }

    pair<vector<pair<uint32_t, uint32_t>>, vector<pair<uint32_t, uint32_t>>> top(int n) const {
        // return the top N price levels for both bids and asks

        vector<pair<uint32_t, uint32_t>> bidResult;
        vector<pair<uint32_t, uint32_t>> askResult;
        auto bidIt = bids.begin();
        auto askIt = asks.begin();
        for (int i = 0; i < n; ++i) {
            PriceLevel bidLevel;
            PriceLevel askLevel;
            if (bidIt != bids.end()) {
                bidLevel = bidIt->second;
                // push here
                bidResult.push_back({bidIt->first, bidLevel.totalQuantity});
                ++bidIt;
            }
            if (askIt != asks.end()) {
                askLevel = askIt->second;
                askResult.push_back({askIt->first, askLevel.totalQuantity});
                ++askIt;
            }
        }
        return {bidResult, askResult};
    }
};

class OrderBook {
    private:
        unordered_map<uint64_t, Order> m_orderIndex;
        unordered_map<uint16_t, Book> m_books;

        // find the price level an order sits in, and subtract 'amount' from its cached total.
        // does not touch the FIFO list; used for partial fills/cancels where the order stays resting.
        void reduceLevelQuantity(const Order& order, uint32_t amount) {
            auto bookIt = m_books.find(order.stockLocate());
            if (bookIt == m_books.end()) {
                return;
            }
            Book& book = bookIt->second;
            if (order.buySellIndicator() == 'B') {
                auto levelIt = book.bids.find(order.price());
                if (levelIt != book.bids.end()) {
                    levelIt->second.totalQuantity -= amount;
                }
            } else if (order.buySellIndicator() == 'S') {
                auto levelIt = book.asks.find(order.price());
                if (levelIt != book.asks.end()) {
                    levelIt->second.totalQuantity -= amount;
                }
            }
        }

    public:
        int size() const {
            return m_orderIndex.size();
        }
        const unordered_map<uint16_t, Book>& books() const {
            return m_books;
        }

        void addOrder(const Order& order) {
            Order newOrder = order;
            Book& book = m_books[newOrder.stockLocate()];

            if (newOrder.buySellIndicator() == 'B') {
                PriceLevel& level = book.bids[newOrder.price()];
                level.totalQuantity += newOrder.shares();
                level.orderIds.push_back(newOrder.orderReferenceNumber());
                newOrder.setListIterator(std::prev(level.orderIds.end()));
            }
            else if (newOrder.buySellIndicator() == 'S') {
                PriceLevel& level = book.asks[newOrder.price()];
                level.totalQuantity += newOrder.shares();
                level.orderIds.push_back(newOrder.orderReferenceNumber());
                newOrder.setListIterator(std::prev(level.orderIds.end()));
            }
            else {
                throw runtime_error("Invalid buy/sell indicator");
            }

            m_orderIndex.insert_or_assign(newOrder.orderReferenceNumber(), newOrder);
        }

        void removeOrder(uint64_t orderReferenceNumber) {
            auto orderIt = m_orderIndex.find(orderReferenceNumber);
            if (orderIt == m_orderIndex.end()) {
                return;
            }
            Order& order = orderIt->second;

            auto bookIt = m_books.find(order.stockLocate());
            if (bookIt != m_books.end()) {
                Book& book = bookIt->second;
                if (order.buySellIndicator() == 'B') {
                    auto levelIt = book.bids.find(order.price());
                    if (levelIt != book.bids.end()) {
                        levelIt->second.orderIds.erase(order.listIterator());
                        levelIt->second.totalQuantity -= order.shares();
                        if (levelIt->second.orderIds.empty()) {
                            book.bids.erase(levelIt);
                        }
                    }
                } else if (order.buySellIndicator() == 'S') {
                    auto levelIt = book.asks.find(order.price());
                    if (levelIt != book.asks.end()) {
                        levelIt->second.orderIds.erase(order.listIterator());
                        levelIt->second.totalQuantity -= order.shares();
                        if (levelIt->second.orderIds.empty()) {
                            book.asks.erase(levelIt);
                        }
                    }
                }
            }

            m_orderIndex.erase(orderIt);
        }

        Order* getOrder(uint64_t orderReferenceNumber) {
            auto it = m_orderIndex.find(orderReferenceNumber);
            if (it != m_orderIndex.end()) {
                return &it->second;
            }
            return nullptr;
        }

        void apply(const AddOrder& message) {
            Order order(
                message.orderReferenceNumber,
                message.header.stockLocate,
                message.buySellIndicator,
                message.price,
                message.shares
            );
            addOrder(order);
        }
        void apply(const AddOrderWithMPID& message) {
            Order order(
                message.orderReferenceNumber,
                message.header.stockLocate,
                message.buySellIndicator,
                message.price,
                message.shares
            );
            addOrder(order);
        }
        void apply(const OrderExecuted& message) {
            // reduce the shares of the order by the executed shares

            // NOTE: per NASDAQ spec if order is executed completely (i.e reduces shares to 0) it is deleted without an explicit deletion event following
            Order* order = getOrder(message.orderReferenceNumber);
            if (order) {
                reduceLevelQuantity(*order, message.executedShares);
                order->reduceShares(message.executedShares);
                if (order->shares() == 0) {
                    removeOrder(message.orderReferenceNumber);
                }
            }
            else {
                throw runtime_error("Order not found for execution");
            }
        }
        void apply(const OrderExecutedWithPrice& message) {
            Order* order = getOrder(message.orderReferenceNumber);
            if (order) {
                reduceLevelQuantity(*order, message.executedShares);
                order->reduceShares(message.executedShares);
                if (order->shares() == 0) {
                    removeOrder(message.orderReferenceNumber);
                }
            }
            else {
                throw runtime_error("Order not found for execution");
            }
        }
        void apply(const OrderCancel& message) {
            // reduce remaining shares but don't delete the order, since it may be partially filled and still exist in the book
            Order* order = getOrder(message.orderReferenceNumber);
            if (order) {
                reduceLevelQuantity(*order, message.canceledShares);
                order->reduceShares(message.canceledShares);
                if (order->shares() == 0) {
                    removeOrder(message.orderReferenceNumber);
                }
            }
            else {
                throw runtime_error("Order not found for cancellation");
            }
        }
        void apply(const OrderDelete& message) {
            // remove the order from the book
            removeOrder(message.orderReferenceNumber);
        }
        void apply(const OrderReplace& message) {

            Order* originalOrder = getOrder(message.originalOrderReferenceNumber);
            if (!originalOrder) {
                throw runtime_error("Original order not found for replacement");
            }

            // get stock locate and buy/sell indicator from the original order
            uint16_t stockLocate = originalOrder->stockLocate();
            char buySellIndicator = originalOrder->buySellIndicator();
            removeOrder(message.originalOrderReferenceNumber);
            Order newOrder(
                message.newOrderReferenceNumber,
                stockLocate,
                buySellIndicator,
                message.newPrice,
                message.newShares
            );
            addOrder(newOrder);
        }

        // locate book from stocklocate and return its best bid and ask
        optional<uint32_t> bestBid(uint16_t stockLocate) const {
            auto BookIt = m_books.find(stockLocate);
            if (BookIt != m_books.end()) {
                return BookIt->second.bestBid();
            }
            return nullopt;
        }

        optional<uint32_t> bestAsk(uint16_t stockLocate) const {
            auto BookIt = m_books.find(stockLocate);
            if (BookIt != m_books.end()) {
                return BookIt->second.bestAsk();
            }
            return nullopt;
        }

        optional<pair<vector<pair<uint32_t, uint32_t>>, vector<pair<uint32_t, uint32_t>>>> top(uint16_t stockLocate, int n) const {
            auto BookIt = m_books.find(stockLocate);
            if (BookIt != m_books.end()) {
                return BookIt->second.top(n);
            }
            return nullopt;
        }
};
using ParsedMessage = std::variant<SystemEvent, AddOrder, OrderExecutedWithPrice, OrderDelete, OrderExecuted, AddOrderWithMPID, StockTradingAction, NetOrderImbalanceIndicator, LULDAuctionCollar, IPOQuotingPeriodUpdate, MarketParticipantPosition, TradeMessageNonCross, CrossTrade, StockDirectory, OrderReplace, MarketWideCircuitBreakerDeclineLevels, OrderCancel, RegSHOShortSalePriceTest>;

ParsedMessage parseMessage(const Message& message) {
    ByteReader reader(std::span<const uint8_t>(message.data(), message.size()));
    uint8_t messageType = reader.readUInt8();

    switch (messageType) {
        case SystemEvent::type:
            return *reinterpret_cast<const SystemEvent*>(message.data());
        case AddOrder::type: {
            AddOrder result{};

            result.header.messageType = messageType;
            result.header.stockLocate = reader.readUInt16BE();
            result.header.trackingNumber = reader.readUInt16BE();
            result.header.timestamp = reader.readUInt48BE();

            result.orderReferenceNumber = reader.readUInt64BE();
            result.buySellIndicator = static_cast<char>(reader.readUInt8());
            result.shares = reader.readUInt32BE();
            reader.readChars(result.stock, 8);
            result.price = reader.readUInt32BE();
            return result;
        }
        case OrderExecutedWithPrice::type: {
            OrderExecutedWithPrice result{};
            result.header.messageType = messageType;
            result.header.stockLocate = reader.readUInt16BE();
            result.header.trackingNumber = reader.readUInt16BE();
            result.header.timestamp = reader.readUInt48BE();

            result.orderReferenceNumber = reader.readUInt64BE();
            result.executedShares = reader.readUInt32BE();
            result.matchNumber = reader.readUInt64BE();
            result.printable = static_cast<char>(reader.readUInt8());
            result.price = reader.readUInt32BE();
            return result;
        }
        case OrderDelete::type: {
            OrderDelete result{};
            result.header.messageType = messageType;
            result.header.stockLocate = reader.readUInt16BE();
            result.header.trackingNumber = reader.readUInt16BE();
            result.header.timestamp = reader.readUInt48BE();

            result.orderReferenceNumber = reader.readUInt64BE();
            return result;
        }
        case OrderExecuted::type: {
            OrderExecuted result{};
            result.header.messageType = messageType;
            result.header.stockLocate = reader.readUInt16BE();
            result.header.trackingNumber = reader.readUInt16BE();
            result.header.timestamp = reader.readUInt48BE();

            result.orderReferenceNumber = reader.readUInt64BE();
            result.executedShares = reader.readUInt32BE();
            result.matchNumber = reader.readUInt64BE();
            return result;
        }
        case AddOrderWithMPID::type: {
            AddOrderWithMPID result{};
            result.header.messageType = messageType;
            result.header.stockLocate = reader.readUInt16BE();
            result.header.trackingNumber = reader.readUInt16BE();
            result.header.timestamp = reader.readUInt48BE();

            result.orderReferenceNumber = reader.readUInt64BE();
            result.buySellIndicator = static_cast<char>(reader.readUInt8());
            result.shares = reader.readUInt32BE();
            reader.readChars(result.stock, 8);
            result.price = reader.readUInt32BE();
            reader.readChars(result.mpid, 4);
            return result;
        }
        case StockTradingAction::type: {
            StockTradingAction result{};
            result.header.messageType = messageType;
            result.header.stockLocate = reader.readUInt16BE();
            result.header.trackingNumber = reader.readUInt16BE();
            result.header.timestamp = reader.readUInt48BE();
            reader.readChars(result.stock, 8);
            result.tradingState = static_cast<char>(reader.readUInt8());
            result.reserved = static_cast<char>(reader.readUInt8());
            reader.readChars(result.reason, 4);
            return result;
        }

        case NetOrderImbalanceIndicator::type: {
            NetOrderImbalanceIndicator result{};
            result.header.messageType = messageType;
            result.header.stockLocate = reader.readUInt16BE();
            result.header.trackingNumber = reader.readUInt16BE();
            result.header.timestamp = reader.readUInt48BE();
            result.pairedShares = reader.readUInt64BE();
            result.imbalanceShares = reader.readUInt64BE();
            result.direction = static_cast<char>(reader.readUInt8());
            reader.readChars(result.stock, 8);
            result.farPrice = reader.readUInt32BE();
            result.nearPrice = reader.readUInt32BE();
            result.crossType = static_cast<char>(reader.readUInt8());
            result.priceVariation = static_cast<char>(reader.readUInt8());
            return result;
        }

        case LULDAuctionCollar::type: {
            LULDAuctionCollar result{};
            result.header.messageType = messageType;
            result.header.stockLocate = reader.readUInt16BE();
            result.header.trackingNumber = reader.readUInt16BE();
            result.header.timestamp = reader.readUInt48BE();
            reader.readChars(result.stock, 8);
            result.collarReferencePrice = reader.readUInt32BE();
            result.upperAuctionCollarPrice = reader.readUInt32BE();
            result.lowerAuctionCollarPrice = reader.readUInt32BE();
            result.auctionExtensions = reader.readUInt32BE();
            return result;
        }

        case IPOQuotingPeriodUpdate::type: {
            IPOQuotingPeriodUpdate result{};
            result.header.messageType = messageType;
            result.header.stockLocate = reader.readUInt16BE();
            result.header.trackingNumber = reader.readUInt16BE();
            result.header.timestamp = reader.readUInt48BE();
            reader.readChars(result.stock, 8);
            result.quotationReleaseTime = reader.readUInt32BE();
            result.releaseQualifier = static_cast<char>(reader.readUInt8());
            result.ipoPrice = reader.readUInt32BE();
            return result;
        }

        case MarketParticipantPosition::type: {
            MarketParticipantPosition result{};
            result.header.messageType = messageType;
            result.header.stockLocate = reader.readUInt16BE();
            result.header.trackingNumber = reader.readUInt16BE();
            result.header.timestamp = reader.readUInt48BE();
            reader.readChars(result.mpid, 4);
            reader.readChars(result.stock, 8);
            result.primaryMarketMaker = static_cast<char>(reader.readUInt8());
            result.marketMakerMode = static_cast<char>(reader.readUInt8());
            result.marketParticipantState = static_cast<char>(reader.readUInt8());
            return result;
        }

        case TradeMessageNonCross::type: {
            TradeMessageNonCross result{};
            result.header.messageType = messageType;
            result.header.stockLocate = reader.readUInt16BE();
            result.header.trackingNumber = reader.readUInt16BE();
            result.header.timestamp = reader.readUInt48BE();
            result.orderReferenceNumber = reader.readUInt64BE();
            result.buySellIndicator = static_cast<char>(reader.readUInt8());
            result.shares = reader.readUInt32BE();
            reader.readChars(result.stock, 8);
            result.price = reader.readUInt32BE();
            result.matchNumber = reader.readUInt64BE();
            return result;
        }

        case CrossTrade::type: {
            CrossTrade result{};
            result.header.messageType = messageType;
            result.header.stockLocate = reader.readUInt16BE();
            result.header.trackingNumber = reader.readUInt16BE();
            result.header.timestamp = reader.readUInt48BE();
            result.shares = reader.readUInt64BE();
            reader.readChars(result.stock, 8);
            result.crossPrice = reader.readUInt32BE();
            result.matchNumber = reader.readUInt64BE();
            result.crossType = static_cast<char>(reader.readUInt8());
            return result;
        }

        case StockDirectory::type: {
            StockDirectory result{};
            result.header.messageType = messageType;
            result.header.stockLocate = reader.readUInt16BE();
            result.header.trackingNumber = reader.readUInt16BE();
            result.header.timestamp = reader.readUInt48BE();
            reader.readChars(result.stock, 8);
            result.marketCategory = static_cast<char>(reader.readUInt8());
            result.financialStatusIndicator = static_cast<char>(reader.readUInt8());
            result.roundLotSize = reader.readUInt32BE();
            result.roundLotsOnly = static_cast<char>(reader.readUInt8());
            result.issueClarification = static_cast<char>(reader.readUInt8());
            reader.readChars(result.issueSubtype, 2);
            result.authenticity = static_cast<char>(reader.readUInt8());
            result.shortSaleThreshold = static_cast<char>(reader.readUInt8());
            result.ipoFlag = static_cast<char>(reader.readUInt8());
            result.luldTier = static_cast<char>(reader.readUInt8());
            result.etpFlag = static_cast<char>(reader.readUInt8());
            result.etpLeverageFactor = reader.readUInt32BE();
            result.inverseIndicator = static_cast<char>(reader.readUInt8());
            return result;
        }

        case OrderReplace::type: {
            OrderReplace result{};
            result.header.messageType = messageType;
            result.header.stockLocate = reader.readUInt16BE();
            result.header.trackingNumber = reader.readUInt16BE();
            result.header.timestamp = reader.readUInt48BE();

            result.originalOrderReferenceNumber = reader.readUInt64BE();
            result.newOrderReferenceNumber = reader.readUInt64BE();
            result.newShares = reader.readUInt32BE();
            result.newPrice = reader.readUInt32BE();
            return result;
        }
        case MarketWideCircuitBreakerDeclineLevels::type: {
            MarketWideCircuitBreakerDeclineLevels result{};
            result.header.messageType = messageType;
            result.header.stockLocate = reader.readUInt16BE();
            result.header.trackingNumber = reader.readUInt16BE();
            result.header.timestamp = reader.readUInt48BE();
            result.level1 = reader.readUInt64BE();
            result.level2 = reader.readUInt64BE();
            result.level3 = reader.readUInt64BE();
            return result;
        }

        case OrderCancel::type: {
            OrderCancel result{};
            result.header.messageType = messageType;
            result.header.stockLocate = reader.readUInt16BE();
            result.header.trackingNumber = reader.readUInt16BE();
            result.header.timestamp = reader.readUInt48BE();
            result.orderReferenceNumber = reader.readUInt64BE();
            result.canceledShares = reader.readUInt32BE();
            return result;
        }

        case RegSHOShortSalePriceTest::type: {
            RegSHOShortSalePriceTest result{};
            result.header.messageType = messageType;
            result.header.stockLocate = reader.readUInt16BE();
            result.header.trackingNumber = reader.readUInt16BE();
            result.header.timestamp = reader.readUInt48BE();
            reader.readChars(result.stock, 8);
            result.regSHOAction = static_cast<char>(reader.readUInt8());
            return result;
        }

        default:
            throw runtime_error("Unknown message type");
    }
}
int main() {
    MessageReader reader("data/12302019.NASDAQ_ITCH50.gz");
    // better idea, we should iteratively read messages and grab all their headers to see how many different types there are

    Message message;
    ParsedMessage parsedMessage;
    OrderBook orderBook;
    int messageCount = 0;
    int unknownMessageCount = 0;
    int malformedMessageCount = 0;

    // just go through first 1000 messages of the order book for now.
    while(orderBook.size() < 1000 && reader.readMessage(message)) {
        try {
            parsedMessage = parseMessage(message);
            messageCount++;
            // list of message types we want to handle [AddOrder, AddOrderWithMPID, OrderExecuted, OrderExecutedWithPrice, OrderCancel, OrderDelete, OrderReplace]

            std::visit([&orderBook, &unknownMessageCount](auto&& msg) {
                using T = std::decay_t<decltype(msg)>;
                if constexpr (std::is_same_v<T, AddOrder>) {
                    cout << "AddOrder message: OrderRef=" << msg.orderReferenceNumber << ", Shares=" << msg.shares << ", Price=" << msg.price << endl;
                    orderBook.apply(msg);
                } else if constexpr (std::is_same_v<T, AddOrderWithMPID>) {
                    cout << "AddOrderWithMPID message: OrderRef=" << msg.orderReferenceNumber << ", Shares=" << msg.shares << ", Price=" << msg.price << endl;
                    orderBook.apply(msg);
                } else if constexpr (std::is_same_v<T, OrderExecuted>) {
                    cout << "OrderExecuted message: OrderRef=" << msg.orderReferenceNumber << ", ExecutedShares=" << msg.executedShares << endl;
                    orderBook.apply(msg);
                } else if constexpr (std::is_same_v<T, OrderExecutedWithPrice>) {
                    cout << "OrderExecutedWithPrice message: OrderRef=" << msg.orderReferenceNumber << ", ExecutedShares=" << msg.executedShares << ", Price=" << msg.price << endl;
                    orderBook.apply(msg);
                } else if constexpr (std::is_same_v<T, OrderCancel>) {
                    cout << "OrderCancel message: OrderRef=" << msg.orderReferenceNumber << ", CanceledShares=" << msg.canceledShares << endl;
                    orderBook.apply(msg);
                } else if constexpr (std::is_same_v<T, OrderDelete>) {
                    cout << "OrderDelete message: OrderRef=" << msg.orderReferenceNumber << endl;
                    orderBook.apply(msg);
                } else if constexpr (std::is_same_v<T, OrderReplace>) {
                    cout << "OrderReplace message: OriginalOrderRef=" << msg.originalOrderReferenceNumber << ", NewOrderRef=" << msg.newOrderReferenceNumber << ", NewShares=" << msg.newShares << ", NewPrice=" << msg.newPrice << endl;
                    orderBook.apply(msg);
                } else {
                    unknownMessageCount++;
                }
            }, parsedMessage);
            
        } catch (const std::exception& e) {
            cerr << "Error parsing message: " << e.what() << endl;
            malformedMessageCount++;
        }
    }

    cout << "The order book has " << orderBook.size() << " orders after processing the ITCH feed." << endl;
    cout << "Processed " << messageCount << " messages." << endl;
    cout << "Encountered " << unknownMessageCount << " unknown messages." << endl;
    cout << "Encountered " << malformedMessageCount << " malformed messages." << endl;

}
