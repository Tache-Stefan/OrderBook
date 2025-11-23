#pragma once

#include <cstdint>
#include <deque>

enum class Side {
    BID,
    ASK
};

class Order {
public:
    using LevelIterator = std::deque<Order*>::iterator;

    Order(std::uint64_t order_id,
          std::uint64_t price,
          std::uint64_t quantity,
          std::uint64_t timestamp,
          Side side);

    inline void set_iterator_in_level(LevelIterator it) { m_iterator_in_level = it; }
    inline LevelIterator get_iterator_in_level() const { return m_iterator_in_level; }
    inline uint64_t get_order_id() const { return m_order_id; }
    inline uint64_t get_price() const { return m_price; }
    inline uint64_t get_quantity() const { return m_quantity; }
    inline Side get_side() const { return m_side; }

private:
    std::uint64_t m_order_id;
    std::uint64_t m_price;
    std::uint64_t m_quantity;
    std::uint64_t m_timestamp;
    LevelIterator m_iterator_in_level = LevelIterator{};
    Side m_side;
};
