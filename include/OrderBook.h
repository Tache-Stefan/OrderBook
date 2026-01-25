#pragma once

#include <cstdint>
#include <unordered_map>
#include <map>
#include <vector>
#include <functional>
#include "Order.h"
#include "PriceLevel.h"
#include "Trade.h"

struct DepthLevel {
    double price;
    uint64_t quantity;
    size_t order_count;
};

class OrderBook {
public:
    using TradeCallback = std::function<void(const Trade&)>;

    explicit OrderBook(double tick_size);

    uint64_t submit_order(double raw_price,
                          uint64_t quantity,
                          uint64_t timestamp,
                          Side side,
                          OrderType order_type = OrderType::LIMIT);
    
    uint64_t submit_market_order(uint64_t quantity,
                                 uint64_t timestamp,
                                 Side side);

    [[nodiscard]] bool cancel_order(uint64_t order_id);
    [[nodiscard]] bool modify_order(uint64_t order_id, uint64_t new_quantity, uint64_t current_timestamp);
    
    [[nodiscard]] Order* best_bid() const noexcept;
    [[nodiscard]] Order* best_ask() const noexcept;

    [[nodiscard]] std::vector<DepthLevel> get_depth(Side side, size_t max_levels = 10) const;
    [[nodiscard]] double get_spread() const;
    [[nodiscard]] double get_mid_price() const;
    [[nodiscard]] size_t get_order_count() const noexcept;
    [[nodiscard]] size_t get_bid_levels() const noexcept;
    [[nodiscard]] size_t get_ask_levels() const noexcept;

    void set_trade_callback(TradeCallback callback);
    [[nodiscard]] const std::vector<Trade>& get_trades() const noexcept;
    void clear_trades() noexcept;

private:
    uint64_t m_next_order_id = 1;
    uint64_t m_next_trade_id = 1;
    double m_tick_size = 0.01;
    std::unordered_map<uint64_t, Order> m_owned_orders;
    std::map<uint64_t, PriceLevel, std::greater<>> m_bids;
    std::map<uint64_t, PriceLevel, std::less<>> m_asks;

    std::vector<Trade> m_trades;
    TradeCallback m_trade_callback = nullptr;

    [[nodiscard]] uint64_t normalize_price(double raw_price) const noexcept;
    [[nodiscard]] bool can_fill_entirely(uint64_t price, uint64_t quantity, Side side) const;

    template<typename OppositeMap>
    void match_against(Order& incoming, OppositeMap& opposite);
    template<typename OppositeMap>
    void match_against_market(Order& incoming, OppositeMap& opposite);

    void add_order(Order& order);
    void match_order(Order& incoming);
    void record_trade(uint64_t buyer_order_id,
                      uint64_t seller_order_id,
                      uint64_t price,
                      uint64_t quantity,
                      uint64_t timestamp);
};

template<typename OppositeMap>
void OrderBook::match_against(Order& incoming, OppositeMap& opposite) {
    while (incoming.get_quantity() > 0 && !opposite.empty()) {
        auto it_level = opposite.begin();
        PriceLevel& level = it_level->second;
        Order* best_opposite = level.best_order();

        if ((incoming.get_side() == Side::BID && incoming.get_price() < best_opposite->get_price()) ||
            (incoming.get_side() == Side::ASK && incoming.get_price() > best_opposite->get_price())) {
            break;
        }

        uint64_t trade_qty = std::min(incoming.get_quantity(), best_opposite->get_quantity());
        uint64_t trade_price = best_opposite->get_price();

        uint64_t buyer_id = (incoming.get_side() == Side::BID) ? incoming.get_order_id() : best_opposite->get_order_id();
        uint64_t seller_id = (incoming.get_side() == Side::ASK) ? incoming.get_order_id() : best_opposite->get_order_id();
        record_trade(buyer_id, seller_id, trade_price, trade_qty, incoming.get_timestamp());

        incoming.decrease_quantity(trade_qty);
        best_opposite->decrease_quantity(trade_qty);
        level.decrease_quantity(trade_qty);

        if (best_opposite->get_quantity() == 0) {
            uint64_t order_id_to_remove = best_opposite->get_order_id();
            level.remove_order(*best_opposite);
            if (level.empty()) {
                opposite.erase(it_level);
            }
            m_owned_orders.erase(order_id_to_remove);
        }
    }
}

template<typename OppositeMap>
void OrderBook::match_against_market(Order& incoming, OppositeMap& opposite) {
    while (incoming.get_quantity() > 0 && !opposite.empty()) {
        auto it_level = opposite.begin();
        PriceLevel& level = it_level->second;
        Order* best_opposite = level.best_order();

        uint64_t trade_qty = std::min(incoming.get_quantity(), best_opposite->get_quantity());
        uint64_t trade_price = best_opposite->get_price();

        uint64_t buyer_id = (incoming.get_side() == Side::BID) ? incoming.get_order_id() : best_opposite->get_order_id();
        uint64_t seller_id = (incoming.get_side() == Side::ASK) ? incoming.get_order_id() : best_opposite->get_order_id();
        record_trade(buyer_id, seller_id, trade_price, trade_qty, incoming.get_timestamp());

        incoming.decrease_quantity(trade_qty);
        best_opposite->decrease_quantity(trade_qty);
        level.decrease_quantity(trade_qty);

        if (best_opposite->get_quantity() == 0) {
            uint64_t order_id_to_remove = best_opposite->get_order_id();
            level.remove_order(*best_opposite);
            if (level.empty()) {
                opposite.erase(it_level);
            }
            m_owned_orders.erase(order_id_to_remove);
        }
    }
}
