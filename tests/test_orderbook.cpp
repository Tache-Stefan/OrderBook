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

TEST_CASE("Modify order - decrease quantity keeps priority") {
    OrderBook ob(0.01);

    uint64_t order1 = ob.submit_order(100.00, 10, 1, Side::BID);
    uint64_t order2 = ob.submit_order(100.00, 20, 2, Side::BID);

    REQUIRE(ob.best_bid() != nullptr);
    CHECK(ob.best_bid()->get_order_id() == order1);  // order1 has priority

    // Decrease quantity - should keep priority
    CHECK(ob.modify_order(order1, 5, 3) == true);
    CHECK(ob.best_bid()->get_order_id() == order1);  // order1 still has priority
    CHECK(ob.best_bid()->get_quantity() == 5);
}

TEST_CASE("Modify order - increase quantity loses priority") {
    OrderBook ob(0.01);

    uint64_t order1 = ob.submit_order(100.00, 10, 1, Side::BID);
    uint64_t order2 = ob.submit_order(100.00, 20, 2, Side::BID);

    REQUIRE(ob.best_bid() != nullptr);
    CHECK(ob.best_bid()->get_order_id() == order1);  // order1 has priority

    // Increase quantity - should lose priority (new order created)
    CHECK(ob.modify_order(order1, 15, 3) == true);
    CHECK(ob.best_bid()->get_order_id() == order2);  // order2 now has priority
}

TEST_CASE("Modify order - zero quantity cancels") {
    OrderBook ob(0.01);

    uint64_t order_id = ob.submit_order(100.00, 10, 1, Side::BID);
    CHECK(ob.best_bid()->get_order_id() == order_id);

    CHECK(ob.modify_order(order_id, 0, 2) == true);
    CHECK(ob.best_bid() == nullptr);
}

TEST_CASE("Modify order - non-existent order") {
    OrderBook ob(0.01);

    CHECK(ob.modify_order(999, 10, 1) == false);
}

TEST_CASE("Modify order - ask side") {
    OrderBook ob(0.01);

    uint64_t order_id = ob.submit_order(100.00, 10, 1, Side::ASK);
    REQUIRE(ob.best_ask() != nullptr);
    CHECK(ob.best_ask()->get_quantity() == 10);

    CHECK(ob.modify_order(order_id, 7, 2) == true);
    CHECK(ob.best_ask()->get_quantity() == 7);
}

TEST_CASE("Trade logging") {
    OrderBook ob(0.01);

    ob.submit_order(100.00, 10, 1, Side::ASK);  // order_id = 1
    ob.submit_order(100.00, 7, 2, Side::BID);   // order_id = 2, matches

    const auto& trades = ob.get_trades();
    REQUIRE(trades.size() == 1);

    CHECK(trades[0].trade_id == 1);
    CHECK(trades[0].buyer_order_id == 2);   // BID is buyer
    CHECK(trades[0].seller_order_id == 1);  // ASK is seller
    CHECK(trades[0].price == 10000);
    CHECK(trades[0].quantity == 7);
}

TEST_CASE("Trade callback") {
    OrderBook ob(0.01);

    std::vector<Trade> captured_trades;
    ob.set_trade_callback([&](const Trade& trade) {
        captured_trades.push_back(trade);
    });

    ob.submit_order(100.00, 10, 1, Side::ASK);
    ob.submit_order(100.00, 15, 2, Side::BID);  // Fills 10, leaves 5

    REQUIRE(captured_trades.size() == 1);
    CHECK(captured_trades[0].quantity == 10);
}

TEST_CASE("Multiple trades") {
    OrderBook ob(0.01);

    ob.submit_order(100.00, 5, 1, Side::ASK);
    ob.submit_order(101.00, 5, 2, Side::ASK);
    ob.submit_order(102.00, 10, 3, Side::BID);  // Should match both asks

    const auto& trades = ob.get_trades();
    REQUIRE(trades.size() == 2);

    CHECK(trades[0].price == 10000);  // First match at 100.00
    CHECK(trades[0].quantity == 5);
    CHECK(trades[1].price == 10100);  // Second match at 101.00
    CHECK(trades[1].quantity == 5);
}
