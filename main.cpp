#include <cstdint>
#include <exception>
#include <iostream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <variant>

#include "itch_parser.hpp"
#include "message_reader.hpp"
#include "order_book.hpp"

using namespace std;

int main(int argc, char* argv[]) {
    // Parse the feed path and options independently of their order.
    string feedPath;
    string symbol;
    int depth = 5; // default depth
    int limit = -1; // -1 means no limit, read the whole feed
    bool verbose = false; // if true, print a line per applied message
    for (int i = 1; i < argc; i++) {
        string arg(argv[i]);
        if (arg.rfind("--symbol=", 0) == 0) {
            symbol = arg.substr(9);
        } else if (arg.rfind("--depth=", 0) == 0) {
            depth = stoi(arg.substr(8));
        } else if (arg.rfind("--limit=", 0) == 0) {
            limit = stoi(arg.substr(8));
        } else if (arg.rfind("--verbose", 0) == 0) {
            verbose = true;
        } else if (!arg.empty() && arg[0] != '-' && feedPath.empty()) {
            feedPath = arg;
        } else {
            cerr << "Unknown argument: " << arg << endl;
            return 1;
        }
    }

    if (feedPath.empty() || symbol.empty()) {
        cerr << "Usage: " << argv[0]
             << " <feed.gz> --symbol=XYZ [--depth=N] [--limit=N] [--verbose]" << endl;
        return 1;
    }

    MessageReader reader(feedPath.c_str());
    Message message;
    ParsedMessage parsedMessage;
    OrderBook orderBook;
    int messageCount = 0;
    unordered_map<char, int> unsupportedMessageCounts;
    unordered_map<uint16_t, string> stockLocateToSymbol;
    int malformedMessageCount = 0;

    while((limit == -1 || messageCount < limit) && reader.readMessage(message)) {
        try {
            parsedMessage = parseMessage(message);
            messageCount++;
            // list of message types we want to handle [AddOrder, AddOrderWithMPID, OrderExecuted, OrderExecutedWithPrice, OrderCancel, OrderDelete, OrderReplace]

            std::visit([&orderBook, &unsupportedMessageCounts, &stockLocateToSymbol, verbose](auto&& msg) {
                using T = std::decay_t<decltype(msg)>;
                if constexpr (std::is_same_v<T, AddOrder>) {
                    if (verbose) cout << "AddOrder message: OrderRef=" << msg.orderReferenceNumber << ", Shares=" << msg.shares << ", Price=" << msg.price << endl;
                    orderBook.apply(msg);
                } else if constexpr (std::is_same_v<T, AddOrderWithMPID>) {
                    if (verbose) cout << "AddOrderWithMPID message: OrderRef=" << msg.orderReferenceNumber << ", Shares=" << msg.shares << ", Price=" << msg.price << endl;
                    orderBook.apply(msg);
                } else if constexpr (std::is_same_v<T, OrderExecuted>) {
                    if (verbose) cout << "OrderExecuted message: OrderRef=" << msg.orderReferenceNumber << ", ExecutedShares=" << msg.executedShares << endl;
                    orderBook.apply(msg);
                } else if constexpr (std::is_same_v<T, OrderExecutedWithPrice>) {
                    if (verbose) cout << "OrderExecutedWithPrice message: OrderRef=" << msg.orderReferenceNumber << ", ExecutedShares=" << msg.executedShares << ", Price=" << msg.price << endl;
                    orderBook.apply(msg);
                } else if constexpr (std::is_same_v<T, OrderCancel>) {
                    if (verbose) cout << "OrderCancel message: OrderRef=" << msg.orderReferenceNumber << ", CanceledShares=" << msg.canceledShares << endl;
                    orderBook.apply(msg);
                } else if constexpr (std::is_same_v<T, OrderDelete>) {
                    if (verbose) cout << "OrderDelete message: OrderRef=" << msg.orderReferenceNumber << endl;
                    orderBook.apply(msg);
                } else if constexpr (std::is_same_v<T, OrderReplace>) {
                    if (verbose) cout << "OrderReplace message: OriginalOrderRef=" << msg.originalOrderReferenceNumber << ", NewOrderRef=" << msg.newOrderReferenceNumber << ", NewShares=" << msg.newShares << ", NewPrice=" << msg.newPrice << endl;
                    orderBook.apply(msg);
                } else if constexpr (std::is_same_v<T, StockDirectory>) {
                    // store stock symbol for later use, trimming trailing space padding
                    string stockSymbol(msg.stock, 8);
                    size_t lastNonSpace = stockSymbol.find_last_not_of(' ');
                    stockSymbol = (lastNonSpace == string::npos) ? "" : stockSymbol.substr(0, lastNonSpace + 1);
                    stockLocateToSymbol[msg.header.stockLocate] = stockSymbol;
                } else {
                    unsupportedMessageCounts[T::type]++;
                }
            }, parsedMessage);
            
        } catch (const std::exception& e) {
            cerr << "Error parsing message: " << e.what() << endl;
            malformedMessageCount++;
        }
    }

    cout << "The order book has " << orderBook.size() << " orders after processing the ITCH feed." << endl;
    cout << "Processed " << messageCount << " messages." << endl;
    cout << "Encountered " << malformedMessageCount << " malformed messages." << endl;
    cout << "addOrder calls: " << orderBook.addOrderCalls() << endl;
    cout << "removeOrder calls: " << orderBook.removeOrderCalls() << endl;

    cout << "Unsupported message counts by type:" << endl;
    for (const auto& [type, count] : unsupportedMessageCounts) {
        cout << "  Type '" << type << "': " << count << endl;
    }

    // find stockLocate for this symbol
    uint16_t stockLocate = 0;
    bool found = false;
    for (const auto& [loc, sym] : stockLocateToSymbol) {
        if (sym == symbol) {
            stockLocate = loc;
            found = true;
            break;
        }
    }
    if (!found) {
        cout << "Symbol " << symbol << " not found in the feed." << endl;
        return 1;
    }

    // get top N bids and asks for this stockLocate
    auto topLevelsOpt = orderBook.top(stockLocate, depth);
    if (topLevelsOpt) {
        auto [bids, asks] = *topLevelsOpt;
        cout << "Top " << depth << " levels for symbol " << symbol << ":" << endl;
        cout << "Bids:" << endl;
        for (const auto& [price, quantity] : bids) {
            cout << "  Price: " << price << ", Quantity: " << quantity << endl;
        }
        cout << "Asks:" << endl;
        for (const auto& [price, quantity] : asks) {
            cout << "  Price: " << price << ", Quantity: " << quantity << endl;
        }
    }
}
