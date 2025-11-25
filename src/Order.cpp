#include "Order.h"

Order::Order(uint64_t order_id,
             uint64_t price,
             uint64_t quantity,
             uint64_t timestamp,
             Side side)
    : m_order_id(order_id),
      m_price(price),
      m_quantity(quantity),
      m_timestamp(timestamp),
      m_side(side) {}

void Order::decrease_quantity(uint64_t qty) {
    m_quantity = (qty >= m_quantity) ? 0 : (m_quantity - qty);
}
