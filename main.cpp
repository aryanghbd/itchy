#include <iostream>
#include <zlib.h>
#include <vector>
using namespace std;
#include <array>
#include <set>
#include <map>
#include <stdexcept>

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
    MessageHeader header;
    char eventCode;
};

struct AddOrder {
    // add order message type
    // an order message looks like this: 41 01 F0 00 00 0D 18 C2 ED 8D A2 00 00 00 00 00 00 22 B1 42 00 00 07 D0 41 52 47 58 20 20 20 20 00 18 80 A8
    // 36 bytes long, first 11 header, next 25 the payload. Order Reference takes up 8 bytes, side takes up the next byte (42 = BUY), shares take up the next 4 bytes, stock takes up the next 8 bytes, price takes up the last 4 bytes.
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
    MessageHeader header;
    uint64_t orderReferenceNumber;
};

struct OrderExecuted {
    // distinguished because it lacks a price.. duh
    // eg: 45 1B 36 00 02 0D 18 D4 F6 B6 90 00 00 00 00 00 00 99 F3 00 00 00 03 00 00 00 00 00 00 45 7F 
    // 11 byte header, 8 byte order reference, 4 byte executed shares, 8 byte match number

    MessageHeader header;
    uint64_t orderReferenceNumber;
    uint32_t executedShares;
    uint64_t matchNumber;
};

struct AddOrderWithMPID {
    // distinguished because it has an MPID at the end
    // eg: 49 01 F0 00 00 0D 18 C2 ED 8D A2 00 00 00 00 00 00 22 B1 42 00 00 07 D0 41 52 47 58 20 20 20 20 00 18 80 A8 
    // header + order reference + buy/sell indicator + shares + stock + price + MPID 
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
    MessageHeader header;
    uint64_t orderReferenceNumber;
    uint32_t canceledShares;
};

struct RegSHOShortSalePriceTest {
    // type Y
    // eg: 59 00 01 00 00 0A 53 A2 BB EE 7F 41 20 20 20 20 20 20 20 30
    // "At 03:09:14.328788607, Nasdaq reported that symbol A, stock-locate 1, was not subject to the Reg SHO short-sale price test."

    // header + stock (8) + reg sho action (1)
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

int main() {
    MessageReader reader("data/12302019.NASDAQ_ITCH50.gz");
    // better idea, we should iteratively read messages and grab all their headers to see how many different types there are

    map<uint8_t, Message> messagesByType;
    // so i can get a sense of what each unique message type looks like.
    constexpr size_t expectedMessageTypes = 22;
    Message message;
    while (messagesByType.size() < expectedMessageTypes && reader.readMessage(message)) {
        uint8_t messageType = message.data()[0];
        messagesByType.try_emplace(messageType, message);
    }
    // go through each type and the message
    for (const auto& pair : messagesByType) {
        uint8_t messageType = pair.first;
        const Message& message = pair.second;
        cout << "Message Type: " << static_cast<int>(messageType) << ", Size: " << message.size() << endl;
        reader.printMessage(message);
    }

}
