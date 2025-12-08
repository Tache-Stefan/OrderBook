#pragma once

#include <unordered_map>
#include <map>
#include "Order.h"
#include "PriceLevel.h"

class OrderBook {
public:
    explicit OrderBook(double tick_size);

    void submit_order(uint64_t order_id,
                      double raw_price,
                      uint64_t quantity,
                      uint64_t timestamp,
                      Side side);

    [[nodiscard]] bool cancel_order(uint64_t order_id);
    
    [[nodiscard]] Order* best_bid() const noexcept;
    [[nodiscard]] Order* best_ask() const noexcept;

private:
    double m_tick_size;
    std::unordered_map<uint64_t, Order> m_owned_orders;
    std::map<uint64_t, PriceLevel, std::greater<>> m_bids;
    std::map<uint64_t, PriceLevel, std::less<>> m_asks;
    std::map<uint64_t, Order*> m_orders;

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
            level.remove_order(*best_opposite);
            if (level.empty()) {
                opposite.erase(it_level);
            }
            m_orders.erase(best_opposite->get_order_id());
        }
    }
}
