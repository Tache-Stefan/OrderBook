#include "OrderBook.h"
#include <cassert>

OrderBook::OrderBook(double tick_size)
    : m_tick_size(tick_size) {}

void OrderBook::add_order(Order& order) {
    uint64_t order_price = order.get_price();

    if (order.get_side() == Side::BID) {
        auto it = m_bids.try_emplace(order_price, PriceLevel(order_price)).first;
        it->second.add_order(order);
    } else {
        auto it = m_asks.try_emplace(order_price, PriceLevel(order_price)).first;
        it->second.add_order(order);
    }

    assert(m_orders.find(order.get_order_id()) == m_orders.end());
    m_orders[order.get_order_id()] = &order;
}
