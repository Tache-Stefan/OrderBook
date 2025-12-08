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

    Order(uint64_t order_id,
          uint64_t price_in_ticks,
          uint64_t quantity,
          uint64_t timestamp,
          Side side);

    inline void set_iterator_in_level(LevelIterator it) noexcept { m_iterator_in_level = it; }
    [[nodiscard]] inline LevelIterator get_iterator_in_level() const noexcept { return m_iterator_in_level; }
    [[nodiscard]] inline uint64_t get_order_id() const noexcept { return m_order_id; }
    [[nodiscard]] inline uint64_t get_price() const noexcept { return m_price; }
    [[nodiscard]] inline uint64_t get_quantity() const noexcept { return m_quantity; }
    [[nodiscard]] inline Side get_side() const noexcept { return m_side; }
    void decrease_quantity(uint64_t qty) noexcept;

private:
    uint64_t m_order_id;
    uint64_t m_price;
    uint64_t m_quantity;
    uint64_t m_timestamp;
    LevelIterator m_iterator_in_level = LevelIterator{};
    Side m_side;
};
