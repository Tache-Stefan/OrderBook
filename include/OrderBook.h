#pragma once

#include <cstdint>
#include <unordered_map>
#include <map>
#include "Order.h"
#include "PriceLevel.h"

class OrderBook {
public:
    explicit OrderBook(double tick_size);

    uint64_t submit_order(double raw_price,
                          uint64_t quantity,
                          uint64_t timestamp,
                          Side side);

    [[nodiscard]] bool cancel_order(uint64_t order_id);
    
    [[nodiscard]] Order* best_bid() const noexcept;
    [[nodiscard]] Order* best_ask() const noexcept;

private:
    uint64_t m_next_order_id = 1;
    double m_tick_size = 0.01;
    std::unordered_map<uint64_t, Order> m_owned_orders;
    std::map<uint64_t, PriceLevel, std::greater<>> m_bids;
    std::map<uint64_t, PriceLevel, std::less<>> m_asks;

    template<typename OppositeMap>
    void match_against(Order& incoming, OppositeMap& opposite);
    void add_order(Order& order);
    void match_order(Order& incoming);
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
