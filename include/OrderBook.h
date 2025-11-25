#pragma once

#include <map>
#include "Order.h"
#include "PriceLevel.h"

class OrderBook {
public:
    explicit OrderBook(double tick_size);

    void add_order(Order& order);
    bool cancel_order(uint64_t order_id);
    void match_order(Order& incoming);

private:
    double m_tick_size;
    std::map<uint64_t, PriceLevel, std::greater<>> m_bids;
    std::map<uint64_t, PriceLevel, std::less<>> m_asks;
    std::map<uint64_t, Order*> m_orders;

    Order* best_bid() const;
    Order* best_ask() const;
};
