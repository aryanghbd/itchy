#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory_resource>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

#include "itch_messages.hpp"

class Order {
    // instruction to buy or sell a stock, some given price or quantity.
private:
    uint64_t m_orderReferenceNumber;
    uint16_t m_stockLocate;
    char m_buySellIndicator;
    uint32_t m_price; // big endian with 4 implied decimal places
    uint32_t m_shares;
    Order* m_previousOrder = nullptr;
    Order* m_nextOrder = nullptr;

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
    Order* previousOrder() const { return m_previousOrder; }
    Order* nextOrder() const { return m_nextOrder; }

    void setPreviousOrder(Order* order) { m_previousOrder = order; }
    void setNextOrder(Order* order) { m_nextOrder = order; }

    void reduceShares(uint32_t amount) {
        if (amount > m_shares) {
            throw std::runtime_error("Cannot reduce shares below zero");
        }
        m_shares -= amount;
    }
};

struct PriceLevel {
    uint32_t totalQuantity = 0;
    Order* head = nullptr;
    Order* tail = nullptr;
};

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

    std::pair<std::vector<std::pair<uint32_t, uint32_t>>, std::vector<std::pair<uint32_t, uint32_t>>> top(int n) const {
        // return the top N price levels for both bids and asks

        std::vector<std::pair<uint32_t, uint32_t>> bidResult;
        std::vector<std::pair<uint32_t, uint32_t>> askResult;
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
        std::pmr::unsynchronized_pool_resource m_OrderPool;
        std::pmr::unordered_map<uint64_t, Order> m_orderIndex{&m_OrderPool};
        std::unordered_map<uint16_t, Book> m_books;
        uint64_t m_addOrderCalls = 0;
        uint64_t m_removeOrderCalls = 0;

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
        const std::unordered_map<uint16_t, Book>& books() const {
            return m_books;
        }
        uint64_t addOrderCalls() const { return m_addOrderCalls; }
        uint64_t removeOrderCalls() const { return m_removeOrderCalls; }
        // uint64_t hitCount() const { return hitCounter; }
        // uint64_t missCount() const { return missCounter; }

        void addOrder(const Order& order) {
            ++m_addOrderCalls;
            if (order.buySellIndicator() != 'B' && order.buySellIndicator() != 'S') {
                throw std::runtime_error("Invalid buy/sell indicator");
            }

            auto [orderIt, inserted] =
                m_orderIndex.try_emplace(order.orderReferenceNumber(), order);
            if (!inserted) {
                throw std::runtime_error("Duplicate order reference number");
            }

            Order& storedOrder = orderIt->second;
            Book& book = m_books[storedOrder.stockLocate()];
            PriceLevel& level = storedOrder.buySellIndicator() == 'B'
                ? book.bids[storedOrder.price()]
                : book.asks[storedOrder.price()];

            storedOrder.setPreviousOrder(level.tail);
            if (level.tail != nullptr) {
                level.tail->setNextOrder(&storedOrder);
            } else {
                level.head = &storedOrder;
            }
            level.tail = &storedOrder;
            level.totalQuantity += storedOrder.shares();
        }

        void removeOrder(uint64_t orderReferenceNumber) {
            ++m_removeOrderCalls;
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
                        PriceLevel& level = levelIt->second;
                        if (order.previousOrder() != nullptr) {
                            order.previousOrder()->setNextOrder(order.nextOrder());
                        } else {
                            level.head = order.nextOrder();
                        }
                        if (order.nextOrder() != nullptr) {
                            order.nextOrder()->setPreviousOrder(order.previousOrder());
                        } else {
                            level.tail = order.previousOrder();
                        }
                        level.totalQuantity -= order.shares();
                        if (level.head == nullptr) {
                            book.bids.erase(levelIt);
                        }
                    }
                } else if (order.buySellIndicator() == 'S') {
                    auto levelIt = book.asks.find(order.price());
                    if (levelIt != book.asks.end()) {
                        PriceLevel& level = levelIt->second;
                        if (order.previousOrder() != nullptr) {
                            order.previousOrder()->setNextOrder(order.nextOrder());
                        } else {
                            level.head = order.nextOrder();
                        }
                        if (order.nextOrder() != nullptr) {
                            order.nextOrder()->setPreviousOrder(order.previousOrder());
                        } else {
                            level.tail = order.previousOrder();
                        }
                        level.totalQuantity -= order.shares();
                        if (level.head == nullptr) {
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
                throw std::runtime_error("Order not found for execution");
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
                throw std::runtime_error("Order not found for execution");
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
                throw std::runtime_error("Order not found for cancellation");
            }
        }
        void apply(const OrderDelete& message) {
            // remove the order from the book
            removeOrder(message.orderReferenceNumber);
        }
        void apply(const OrderReplace& message) {

            Order* originalOrder = getOrder(message.originalOrderReferenceNumber);
            if (!originalOrder) {
                throw std::runtime_error("Original order not found for replacement");
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
        std::optional<uint32_t> bestBid(uint16_t stockLocate) const {
            auto BookIt = m_books.find(stockLocate);
            if (BookIt != m_books.end()) {
                return BookIt->second.bestBid();
            }
            return std::nullopt;
        }

        std::optional<uint32_t> bestAsk(uint16_t stockLocate) const {
            auto BookIt = m_books.find(stockLocate);
            if (BookIt != m_books.end()) {
                return BookIt->second.bestAsk();
            }
            return std::nullopt;
        }

        std::optional<std::pair<std::vector<std::pair<uint32_t, uint32_t>>, std::vector<std::pair<uint32_t, uint32_t>>>> top(uint16_t stockLocate, int n) const {
            auto BookIt = m_books.find(stockLocate);
            if (BookIt != m_books.end()) {
                return BookIt->second.top(n);
            }
            return std::nullopt;
        }
};
