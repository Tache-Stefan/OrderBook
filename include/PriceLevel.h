#pragma once

#include "Order.h"
#include <cstdint>
#include <list>

class PriceLevel {
public:
    using OrderList = std::list<Order*>;
    using Iterator = OrderList::iterator;

    explicit PriceLevel(uint64_t price);

    void add_order(Order& order);
    void remove_order(Order& order);
    void decrease_quantity(uint64_t qty) noexcept;

    [[nodiscard]] inline bool empty() const noexcept { return m_orders.empty(); }
    [[nodiscard]] inline Order* best_order() const noexcept { return m_orders.empty() ? nullptr : m_orders.front(); }
    [[nodiscard]] inline uint64_t get_total_quantity() const noexcept { return m_total_quantity; }
    [[nodiscard]] inline size_t get_order_count() const noexcept { return m_orders.size(); }
    [[nodiscard]] inline uint64_t get_price() const noexcept { return m_price; }
private:
    OrderList m_orders;
    uint64_t m_total_quantity = 0;
    uint64_t m_price = 0;
};
