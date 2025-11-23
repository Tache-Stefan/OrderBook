#pragma once

#include <cstdint>
#include <deque>
#include "Order.h"

class PriceLevel {
public:
    explicit PriceLevel(std::uint64_t price);

    void add_order(Order& order);
    void remove_order(Order& order);

    inline Order* best_order() const { return m_orders.empty() ? nullptr : m_orders.front(); }

private:
    std::uint64_t m_total_quantity = 0;
    std::uint64_t m_price;
    std::deque<Order*> m_orders;
};
