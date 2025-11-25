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

bool OrderBook::cancel_order(uint64_t order_id) {
    auto it_order = m_orders.find(order_id);
    if (it_order == m_orders.end()) return false;

    Order* order = it_order->second;
    uint64_t price = order->get_price();
    Side side = order->get_side();

    if (side == Side::BID) {
        auto it_level = m_bids.find(price);
        if (it_level != m_bids.end()) {
            it_level->second.remove_order(*order);
            if (it_level->second.best_order() == nullptr) {
                m_bids.erase(it_level);
            }
        }
    } else {
        auto it_level = m_asks.find(price);
        if (it_level != m_asks.end()) {
            it_level->second.remove_order(*order);
            if (it_level->second.best_order() == nullptr) {
                m_asks.erase(it_level);
            }
        }
    }

    m_orders.erase(it_order);
    return true;
}

void OrderBook::match_order(Order& incoming) {
    auto* opposite = (incoming.get_side() == Side::BID) ? &m_asks : &m_bids;

    while (incoming.get_quantity() > 0 && !opposite->empty()) {
        auto it_level = opposite->begin();
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
            cancel_order(best_opposite->get_order_id());
        }
    }

    // if (incoming.get_quantity() > 0) {
    //     add_order(incoming);
    // }
}

Order* OrderBook::best_bid() const {
    if (m_bids.empty()) return nullptr;
    return m_bids.begin()->second.best_order();
}

Order* OrderBook::best_ask() const {
    if (m_asks.empty()) return nullptr;
    return m_asks.begin()->second.best_order();
}
