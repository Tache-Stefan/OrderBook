#pragma once

#include <map>
#include "Order.h"
#include "PriceLevel.h"

class OrderBook {
public:
    explicit OrderBook(double tick_size);

    void add_order(Order& order);

private:
    double m_tick_size;
    std::map<std::uint64_t, PriceLevel, std::greater<>> m_bids;
    std::map<std::uint64_t, PriceLevel> m_asks;
    std::map<std::uint64_t, Order*> m_orders;
};
