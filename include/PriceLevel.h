#pragma once

#include <cstdint>
#include <deque>
#include "Order.h"

class PriceLevel {
public:
    explicit PriceLevel(uint64_t price);

    void add_order(Order& order);
    void remove_order(Order& order);
    void decrease_quantity(uint64_t qty);

    inline Order* best_order() const { return m_orders.empty() ? nullptr : m_orders.front(); }

private:
    uint64_t m_total_quantity = 0;
    uint64_t m_price;
    std::deque<Order*> m_orders;
};
