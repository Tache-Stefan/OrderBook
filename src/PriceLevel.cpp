#include "PriceLevel.h"

PriceLevel::PriceLevel(uint64_t price)
    : m_price(price) {}

void PriceLevel::add_order(Order& order) {
    m_orders.push_back(&order);
    order.set_iterator_in_level(std::prev(m_orders.end()));
    m_total_quantity += order.get_quantity();
}

void PriceLevel::remove_order(Order& order) {
    m_total_quantity -= order.get_quantity();
    m_orders.erase(order.get_iterator_in_level());
    order.set_iterator_in_level(Order::LevelIterator{});
}

void PriceLevel::decrease_quantity(uint64_t qty) noexcept {
    m_total_quantity = (qty >= m_total_quantity) ? 0 : (m_total_quantity - qty);
}
