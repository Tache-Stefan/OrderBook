#include <catch2/catch_test_macros.hpp>
#include "OrderBook.h"

TEST_CASE("Basic add and best bid / ask") {
    OrderBook ob(0.01);

    ob.submit_order(100.00, 10, 1, Side::BID);
    ob.submit_order(101.00, 5, 2, Side::BID);
    ob.submit_order(102.00, 7, 3, Side::ASK);

    REQUIRE(ob.best_bid() != nullptr);
    REQUIRE(ob.best_ask() != nullptr);

    CHECK(ob.best_bid()->get_price() == 10100);
    CHECK(ob.best_ask()->get_price() == 10200);
}

TEST_CASE("Matching") {
    OrderBook ob(0.01);

    ob.submit_order(100.00, 10, 1, Side::ASK);
    ob.submit_order(101.00, 7, 2, Side::BID);

    // Should fully fill the 7 quantity BID, partially fill the 10 quantity ASK
    auto* ask = ob.best_ask();
    REQUIRE(ask != nullptr);
    CHECK(ask->get_quantity() == 3);

    auto* bid = ob.best_bid();
    CHECK(bid == nullptr);
}

TEST_CASE("Cancel order") {
    OrderBook ob(0.01);

    uint64_t order_id = ob.submit_order(100.00, 10, 1, Side::BID);
    REQUIRE(ob.best_bid() != nullptr);
    CHECK(ob.best_bid()->get_order_id() == order_id);

    CHECK(ob.cancel_order(order_id) == true);
    CHECK(ob.best_bid() == nullptr);
}