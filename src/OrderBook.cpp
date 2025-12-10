#include "OrderBook.h"
#include <cassert>
#include <cmath>

OrderBook::OrderBook(double tick_size)
    : m_tick_size(tick_size) {}

uint64_t OrderBook::submit_order(double raw_price,
                                 uint64_t quantity,
                                 uint64_t timestamp,
                                 Side side) {
    const uint64_t order_id = m_next_order_id++;
    const double ticks = raw_price / m_tick_size;
    assert(std::fabs(ticks - std::round(ticks)) < 1e-9 && "Price is not aligned to tick size");
    
    const uint64_t price_in_ticks = static_cast<uint64_t>(std::llround(ticks));

    auto [it, inserted] = m_owned_orders.emplace(order_id,
                          Order(order_id, price_in_ticks, quantity, timestamp, side));
    
    Order& order = it->second;
    match_order(order);
    
    if (order.get_quantity() > 0) {
        add_order(order);
    } else {
        m_owned_orders.erase(it);
    }

    return order_id;
}

void OrderBook::add_order(Order& order) {
    uint64_t order_price = order.get_price();

    if (order.get_side() == Side::BID) {
        auto it = m_bids.try_emplace(order_price, PriceLevel(order_price)).first;
        it->second.add_order(order);
    } else {
        auto it = m_asks.try_emplace(order_price, PriceLevel(order_price)).first;
        it->second.add_order(order);
    }
}

bool OrderBook::cancel_order(uint64_t order_id) {
    auto it_order = m_owned_orders.find(order_id);
    if (it_order == m_owned_orders.end()) return false;

    Order& order = it_order->second;
    uint64_t price = order.get_price();
    Side side = order.get_side();

    if (side == Side::BID) {
        auto it_level = m_bids.find(price);
        if (it_level != m_bids.end()) {
            it_level->second.remove_order(order);
            if (it_level->second.best_order() == nullptr) {
                m_bids.erase(it_level);
            }
        }
    } else {
        auto it_level = m_asks.find(price);
        if (it_level != m_asks.end()) {
            it_level->second.remove_order(order);
            if (it_level->second.best_order() == nullptr) {
                m_asks.erase(it_level);
            }
        }
    }

    m_owned_orders.erase(it_order);
    return true;
}

void OrderBook::match_order(Order& incoming) {
    if (incoming.get_side() == Side::BID) {
        match_against(incoming, m_asks);
    } else {
        match_against(incoming, m_bids);
    }
}

Order* OrderBook::best_bid() const noexcept {
    if (m_bids.empty()) return nullptr;
    return m_bids.begin()->second.best_order();
}

Order* OrderBook::best_ask() const noexcept {
    if (m_asks.empty()) return nullptr;
    return m_asks.begin()->second.best_order();
}
