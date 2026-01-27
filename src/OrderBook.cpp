#include "OrderBook.h"
#include <cassert>
#include <cmath>

OrderBook::OrderBook(double tick_size)
    : m_tick_size(tick_size) {}

OrderBook::~OrderBook() {
    for (auto& [order_id, order_ptr] : m_orders) {
        m_order_pool.destroy(order_ptr);
    }
}

uint64_t OrderBook::normalize_price(double raw_price) const noexcept {
    return static_cast<uint64_t>(std::round(raw_price / m_tick_size));
}

bool OrderBook::can_fill_entirely(uint64_t price, uint64_t quantity, Side side) const {
    uint64_t available = 0;

    if (side == Side::BID) {
        for (const auto& [level_price, level] : m_asks) {
            if (level_price > price) break;
            available += level.empty() ? 0 : level.best_order()->get_quantity();
            if (available >= quantity) return true;
        }
    } else {
        for (const auto& [level_price, level] : m_bids) {
            if (level_price < price) break;
            available += level.empty() ? 0 : level.best_order()->get_quantity();
            if (available >= quantity) return true;
        }
    }

    return available >= quantity;
}

uint64_t OrderBook::submit_order(double raw_price,
                                 uint64_t quantity,
                                 uint64_t timestamp,
                                 Side side,
                                 OrderType type) {

    if (quantity == 0) {
        return 0;
    }

    uint64_t price = normalize_price(raw_price);
    uint64_t order_id = m_next_order_id++;

    if (type == OrderType::FOK) {
        if (!can_fill_entirely(price, quantity, side)) {
            return 0;
        }
    }

    Order* order = m_order_pool.construct(order_id, price, quantity, timestamp, side);
    match_order(*order);

    if (order->get_quantity() > 0) {
        switch (type) {
            case OrderType::LIMIT:
                m_orders[order_id] = order;
                add_order(*order);
                break;
            case OrderType::MARKET:
            case OrderType::IOC:
            case OrderType::FOK:
                m_order_pool.destroy(order);
                break;
        }
    } else {
        m_order_pool.destroy(order);
    }

    return order_id;
}

uint64_t OrderBook::submit_market_order(uint64_t quantity,
                                        uint64_t timestamp,
                                        Side side) {
    if (quantity == 0) {
        return 0;
    }

    uint64_t order_id = m_next_order_id++;
    uint64_t price = (side == Side::BID) ? std::numeric_limits<uint64_t>::max() : 0;

    Order* order = m_order_pool.construct(order_id, price, quantity, timestamp, side);

    if (side == Side::BID) {
        match_against_market(*order, m_asks);
    } else {
        match_against_market(*order, m_bids);
    }

    m_order_pool.destroy(order);

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
    auto it_order = m_orders.find(order_id);
    if (it_order == m_orders.end()) return false;

    Order& order = *it_order->second;
    uint64_t price = order.get_price();

    if (order.get_side() == Side::BID) {
        auto it_level = m_bids.find(price);
        if (it_level != m_bids.end()) {
            it_level->second.remove_order(order);
            if (it_level->second.empty()) {
                m_bids.erase(it_level);
            }
        }
    } else {
        auto it_level = m_asks.find(price);
        if (it_level != m_asks.end()) {
            it_level->second.remove_order(order);
            if (it_level->second.empty()) {
                m_asks.erase(it_level);
            }
        }
    }

    m_order_pool.destroy(it_order->second);
    m_orders.erase(it_order);
    return true;
}

bool OrderBook::modify_order(uint64_t order_id, uint64_t new_quantity, uint64_t current_timestamp) {
    if (new_quantity == 0) {
        return cancel_order(order_id);
    }

    auto it = m_orders.find(order_id);
    if (it == m_orders.end()) {
        return false;
    }

    Order& order = *it->second;

    if (new_quantity > order.get_quantity()) {
        // Cancel and resubmit (loses priority)
        double raw_price = order.get_price() * m_tick_size;
        Side side = order.get_side();

        if (!cancel_order(order_id)) {
            return false;
        }
        submit_order(raw_price, new_quantity, current_timestamp, side);
    } else {
        // Modify in place (keeps priority)
        uint64_t diff = order.get_quantity() - new_quantity;
        order.decrease_quantity(diff);

        uint64_t price = order.get_price();
        if (order.get_side() == Side::BID) {
            auto it_level = m_bids.find(price);
            if (it_level != m_bids.end()) {
                it_level->second.decrease_quantity(diff);
            }
        } else {
            auto it_level = m_asks.find(price);
            if (it_level != m_asks.end()) {
                it_level->second.decrease_quantity(diff);
            }
        }
    }

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

void OrderBook::set_trade_callback(TradeCallback callback) {
    m_trade_callback = std::move(callback);
}

const std::vector<Trade>& OrderBook::get_trades() const noexcept {
    return m_trades;
}

void OrderBook::clear_trades() noexcept {
    m_trades.clear();
}

void OrderBook::record_trade(uint64_t buyer_order_id,
                             uint64_t seller_order_id,
                             uint64_t price,
                             uint64_t quantity,
                             uint64_t timestamp) {
    Trade trade{
        m_next_trade_id++,
        buyer_order_id,
        seller_order_id,
        price,
        quantity,
        timestamp
    };
    m_trades.push_back(trade);

    if (m_trade_callback) {
        m_trade_callback(trade);
    }
}

std::vector<DepthLevel> OrderBook::get_depth(Side side, size_t max_levels) const {
    std::vector<DepthLevel> depth;
    depth.reserve(max_levels);

    if (side == Side::BID) {
        for (const auto& [price_ticks, level] : m_bids) {
            if (depth.size() >= max_levels) break;
            depth.push_back({
                static_cast<double>(price_ticks) * m_tick_size,
                level.get_total_quantity(),
                level.get_order_count()
            });
        }
    } else {
        for (const auto& [price_ticks, level] : m_asks) {
            if (depth.size() >= max_levels) break;
            depth.push_back({
                static_cast<double>(price_ticks) * m_tick_size,
                level.get_total_quantity(),
                level.get_order_count()
            });
        }
    }

    return depth;
}

double OrderBook::get_spread() const {
    if (m_bids.empty() || m_asks.empty()) {
        return 0.0;
    }

    uint64_t best_bid_ticks = m_bids.begin()->first;
    uint64_t best_ask_ticks = m_asks.begin()->first;

    return static_cast<double>(best_ask_ticks - best_bid_ticks) * m_tick_size;
}

double OrderBook::get_mid_price() const {
    if (m_bids.empty() || m_asks.empty()) {
        return 0.0;
    }

    uint64_t best_bid_ticks = m_bids.begin()->first;
    uint64_t best_ask_ticks = m_asks.begin()->first;

    return static_cast<double>(best_bid_ticks + best_ask_ticks) * m_tick_size / 2.0;
}

size_t OrderBook::get_order_count() const noexcept {
    return m_orders.size();
}

size_t OrderBook::get_bid_levels() const noexcept {
    return m_bids.size();
}

size_t OrderBook::get_ask_levels() const noexcept {
    return m_asks.size();
}

void OrderBook::reserve_orders(const size_t count) {
    m_order_pool.reserve(count);
    m_orders.reserve(count);
}
