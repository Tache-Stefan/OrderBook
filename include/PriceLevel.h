#pragma once

#include <cstdint>
#include <deque>
#include "Order.h"

class PriceLevel {
public:
    explicit PriceLevel(uint64_t price);

    void add_order(Order& order);
    void remove_order(Order& order);
    void decrease_quantity(uint64_t qty) noexcept;
    [[nodiscard]] inline bool empty() const noexcept { return m_orders.empty(); }
    [[nodiscard]] inline Order* best_order() const noexcept { return m_orders.empty() ? nullptr : m_orders.front(); }
private:
    uint64_t m_total_quantity = 0;
    uint64_t m_price = 0;
    std::deque<Order*> m_orders;
};
