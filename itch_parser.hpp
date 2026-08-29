#pragma once

#include <cstdint>
#include <span>
#include <stdexcept>
#include <variant>

#include "byte_reader.hpp"
#include "itch_messages.hpp"

using ParsedMessage = std::variant<SystemEvent, AddOrder, OrderExecutedWithPrice, OrderDelete, OrderExecuted, AddOrderWithMPID, StockTradingAction, NetOrderImbalanceIndicator, LULDAuctionCollar, IPOQuotingPeriodUpdate, MarketParticipantPosition, TradeMessageNonCross, CrossTrade, StockDirectory, OrderReplace, MarketWideCircuitBreakerDeclineLevels, OrderCancel, RegSHOShortSalePriceTest>;

ParsedMessage parseMessage(const Message& message) {
    ByteReader reader(std::span<const uint8_t>(message.data(), message.size()));
    uint8_t messageType = reader.readUInt8();

    switch (messageType) {
        case SystemEvent::type: {
            SystemEvent result{};
            result.header.messageType = messageType;
            result.header.stockLocate = reader.readUInt16BE();
            result.header.trackingNumber = reader.readUInt16BE();
            result.header.timestamp = reader.readUInt48BE();
            result.eventCode = static_cast<char>(reader.readUInt8());
            return result;
        }
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
            throw std::runtime_error("Unknown message type");
    }
}
