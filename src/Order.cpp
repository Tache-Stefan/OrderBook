#include "Order.h"

Order::Order(std::uint64_t order_id,
             std::uint64_t price,
             std::uint64_t quantity,
             std::uint64_t timestamp,
             Side side)
    : m_order_id(order_id),
      m_price(price),
      m_quantity(quantity),
      m_timestamp(timestamp),
      m_side(side) {}
