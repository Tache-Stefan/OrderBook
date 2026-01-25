#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
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

// ==================== Immediate Or Cancel (IOC) Tests =================

TEST_CASE("IOC order - full fill") {
    OrderBook ob(0.01);

    ob.submit_order(100.00, 50, 1, Side::ASK);
    uint64_t id = ob.submit_order(100.00, 50, 2, Side::BID, OrderType::IOC);

    CHECK(id != 0);
    CHECK(ob.get_trades().size() == 1);
    CHECK(ob.get_trades()[0].quantity == 50);
    CHECK(ob.best_ask() == nullptr);
    CHECK(ob.best_bid() == nullptr);
}

TEST_CASE("IOC order - partial fill") {
    OrderBook ob(0.01);

    ob.submit_order(100.00, 30, 1, Side::ASK);
    uint64_t id = ob.submit_order(100.00, 50, 2, Side::BID, OrderType::IOC);

    CHECK(id != 0);
    CHECK(ob.get_trades().size() == 1);
    CHECK(ob.get_trades()[0].quantity == 30);
    CHECK(ob.best_ask() == nullptr);
    CHECK(ob.best_bid() == nullptr);
}

TEST_CASE("IOC order - no match") {
    OrderBook ob(0.01);

    ob.submit_order(101.00, 50, 1, Side::ASK);
    uint64_t id = ob.submit_order(100.00, 50, 2, Side::BID, OrderType::IOC);

    CHECK(id != 0);
    CHECK(ob.get_trades().empty());
    CHECK(ob.best_bid() == nullptr);
    CHECK(ob.best_ask() != nullptr);
    CHECK(ob.best_ask()->get_quantity() == 50);
}

TEST_CASE("IOC order - sell side") {
    OrderBook ob(0.01);

    ob.submit_order(100.00, 30, 1, Side::BID);
    uint64_t id = ob.submit_order(100.00, 50, 2, Side::ASK, OrderType::IOC);

    CHECK(id != 0);
    CHECK(ob.get_trades().size() == 1);
    CHECK(ob.get_trades()[0].quantity == 30);
    CHECK(ob.best_bid() == nullptr);
    CHECK(ob.best_ask() == nullptr);
}

TEST_CASE("IOC order - sweeps multiple levels") {
    OrderBook ob(0.01);

    ob.submit_order(100.00, 20, 1, Side::ASK);
    ob.submit_order(100.01, 20, 2, Side::ASK);
    ob.submit_order(100.02, 20, 3, Side::ASK);

    uint64_t id = ob.submit_order(100.02, 50, 4, Side::BID, OrderType::IOC);

    CHECK(id != 0);
    CHECK(ob.get_trades().size() == 3);
    CHECK(ob.best_ask()->get_price() == 10002);
    CHECK(ob.best_ask()->get_quantity() == 10);
}

// ==================== Fill Or Kill (FOK) Tests =================

TEST_CASE("FOK order - full fill possible") {
    OrderBook ob(0.01);

    ob.submit_order(100.00, 50, 1, Side::ASK);
    uint64_t id = ob.submit_order(100.00, 50, 2, Side::BID, OrderType::FOK);

    CHECK(id != 0);
    CHECK(ob.get_trades().size() == 1);
    CHECK(ob.get_trades()[0].quantity == 50);
    CHECK(ob.best_ask() == nullptr);
}

TEST_CASE("FOK order - full fill not possible") {
    OrderBook ob(0.01);

    ob.submit_order(100.00, 30, 1, Side::ASK);
    uint64_t id = ob.submit_order(100.00, 50, 2, Side::BID, OrderType::FOK);

    CHECK(id == 0);
    CHECK(ob.get_trades().empty());
    CHECK(ob.best_ask() != nullptr);
    CHECK(ob.best_ask()->get_quantity() == 30);
}

TEST_CASE("FOK order - price too aggressive") {
    OrderBook ob(0.01);

    ob.submit_order(100.00, 50, 1, Side::ASK);
    uint64_t id = ob.submit_order(99.00, 50, 2, Side::BID, OrderType::FOK);

    CHECK(id == 0);
    CHECK(ob.get_trades().empty());
    CHECK(ob.best_ask() != nullptr);
    CHECK(ob.best_ask()->get_quantity() == 50);
}

TEST_CASE("FOK order - fills multiple levels") {
    OrderBook ob(0.01);

    ob.submit_order(100.00, 20, 1, Side::ASK);
    ob.submit_order(100.01, 20, 2, Side::ASK);
    ob.submit_order(100.02, 20, 3, Side::ASK);

    uint64_t id = ob.submit_order(100.02, 50, 4, Side::BID, OrderType::FOK);

    CHECK(id != 0);
    CHECK(ob.get_trades().size() == 3);
    CHECK(ob.best_ask()->get_price() == 10002);
    CHECK(ob.best_ask()->get_quantity() == 10);
}

TEST_CASE("FOK order - sell side") {
    OrderBook ob(0.01);

    ob.submit_order(100.00, 30, 1, Side::BID);
    ob.submit_order(99.99, 30, 2, Side::BID);
    uint64_t id = ob.submit_order(99.99, 50, 3, Side::ASK, OrderType::FOK);

    CHECK(id != 0);
    CHECK(ob.get_trades().size() == 2);
}

// =================== Market Order Tests =================

TEST_CASE("Market order - buy sweeps multiple levels") {
    OrderBook ob(0.01);

    ob.submit_order(100.00, 20, 1, Side::ASK);
    ob.submit_order(100.01, 20, 2, Side::ASK);
    ob.submit_order(100.02, 20, 3, Side::ASK);

    uint64_t id = ob.submit_market_order(50, 4, Side::BID);

    CHECK(id != 0);
    CHECK(ob.get_trades().size() == 3);
    CHECK(ob.best_ask()->get_price() == 10002);
    CHECK(ob.best_ask()->get_quantity() == 10);
}

TEST_CASE("Market order - sell sweeps multiple levels") {
    OrderBook ob(0.01);

    ob.submit_order(100.00, 20, 1, Side::BID);
    ob.submit_order(99.99, 20, 2, Side::BID);
    ob.submit_order(99.98, 20, 3, Side::BID);

    uint64_t id = ob.submit_market_order(50, 4, Side::ASK);

    CHECK(id != 0);
    CHECK(ob.get_trades().size() == 3);
    CHECK(ob.best_bid()->get_price() == 9998);
    CHECK(ob.best_bid()->get_quantity() == 10);
}

TEST_CASE("Market order - partial fill") {
    OrderBook ob(0.01);

    ob.submit_order(100.00, 20, 1, Side::ASK);

    uint64_t id = ob.submit_market_order(50, 2, Side::BID);

    CHECK(id != 0);
    CHECK(ob.get_trades().size() == 1);
    CHECK(ob.get_trades()[0].quantity == 20);
    CHECK(ob.best_ask() == nullptr);
    CHECK(ob.best_bid() == nullptr);
}

TEST_CASE("Market order - empty book") {
    OrderBook ob(0.01);

    uint64_t id = ob.submit_market_order(50, 1, Side::BID);

    CHECK(id != 0);
    CHECK(ob.get_trades().empty());
    CHECK(ob.best_bid() == nullptr);
}

// =================== Depth & Statistics Tests =================

TEST_CASE("Get depth - bids") {
    OrderBook ob(0.01);

    ob.submit_order(100.00, 50, 1, Side::BID);
    ob.submit_order(100.00, 30, 2, Side::BID);
    ob.submit_order(99.99, 100, 3, Side::BID);
    ob.submit_order(99.98, 200, 4, Side::BID);

    auto depth = ob.get_depth(Side::BID, 10);

    REQUIRE(depth.size() == 3);
    
    CHECK(depth[0].price == Catch::Approx(100.00));
    CHECK(depth[0].quantity == 80);
    CHECK(depth[0].order_count == 2);
    
    CHECK(depth[1].price == Catch::Approx(99.99));
    CHECK(depth[1].quantity == 100);
    CHECK(depth[1].order_count == 1);
    
    CHECK(depth[2].price == Catch::Approx(99.98));
    CHECK(depth[2].quantity == 200);
    CHECK(depth[2].order_count == 1);
}

TEST_CASE("Get depth - asks") {
    OrderBook ob(0.01);

    ob.submit_order(100.01, 50, 1, Side::ASK);
    ob.submit_order(100.01, 25, 2, Side::ASK);
    ob.submit_order(100.02, 100, 3, Side::ASK);

    auto depth = ob.get_depth(Side::ASK, 10);

    REQUIRE(depth.size() == 2);
    
    CHECK(depth[0].price == Catch::Approx(100.01));
    CHECK(depth[0].quantity == 75);
    CHECK(depth[0].order_count == 2);
    
    CHECK(depth[1].price == Catch::Approx(100.02));
    CHECK(depth[1].quantity == 100);
}

TEST_CASE("Get depth - max levels limit") {
    OrderBook ob(0.01);

    for (int i = 0; i < 10; ++i) {
        ob.submit_order(100.00 + i * 0.01, 10, i, Side::ASK);
    }

    auto depth = ob.get_depth(Side::ASK, 3);
    CHECK(depth.size() == 3);

    auto full_depth = ob.get_depth(Side::ASK, 100);
    CHECK(full_depth.size() == 10);
}

TEST_CASE("Get depth - empty book") {
    OrderBook ob(0.01);

    auto bid_depth = ob.get_depth(Side::BID, 10);
    auto ask_depth = ob.get_depth(Side::ASK, 10);

    CHECK(bid_depth.empty());
    CHECK(ask_depth.empty());
}

TEST_CASE("Get spread") {
    OrderBook ob(0.01);

    ob.submit_order(100.00, 10, 1, Side::BID);
    ob.submit_order(100.05, 10, 2, Side::ASK);

    CHECK(ob.get_spread() == Catch::Approx(0.05));
}

TEST_CASE("Get spread - empty book") {
    OrderBook ob(0.01);

    CHECK(ob.get_spread() == 0.0);

    ob.submit_order(100.00, 10, 1, Side::BID);
    CHECK(ob.get_spread() == 0.0);
}

TEST_CASE("Get mid price") {
    OrderBook ob(0.01);

    ob.submit_order(100.00, 10, 1, Side::BID);
    ob.submit_order(100.10, 10, 2, Side::ASK);

    CHECK(ob.get_mid_price() == Catch::Approx(100.05));
}

TEST_CASE("Get mid price - empty book") {
    OrderBook ob(0.01);

    CHECK(ob.get_mid_price() == 0.0);
}

TEST_CASE("Get order count") {
    OrderBook ob(0.01);

    CHECK(ob.get_order_count() == 0);

    ob.submit_order(100.00, 10, 1, Side::BID);
    CHECK(ob.get_order_count() == 1);

    ob.submit_order(100.00, 10, 2, Side::BID);
    CHECK(ob.get_order_count() == 2);

    ob.submit_order(101.00, 10, 3, Side::ASK);
    CHECK(ob.get_order_count() == 3);
}

TEST_CASE("Get bid/ask levels count") {
    OrderBook ob(0.01);

    CHECK(ob.get_bid_levels() == 0);
    CHECK(ob.get_ask_levels() == 0);

    ob.submit_order(100.00, 10, 1, Side::BID);
    ob.submit_order(100.00, 10, 2, Side::BID);
    ob.submit_order(99.99, 10, 3, Side::BID);

    CHECK(ob.get_bid_levels() == 2);

    ob.submit_order(100.01, 10, 4, Side::ASK);
    CHECK(ob.get_ask_levels() == 1);
}

TEST_CASE("Depth updates after trades") {
    OrderBook ob(0.01);

    ob.submit_order(100.00, 100, 1, Side::ASK);
    ob.submit_order(100.00, 30, 2, Side::BID);

    auto depth = ob.get_depth(Side::ASK, 10);
    CHECK(depth.size() == 1);
    CHECK(depth[0].quantity == 70);
}

TEST_CASE("Depth updates after cancel") {
    OrderBook ob(0.01);

    uint64_t id1 = ob.submit_order(100.00, 50, 1, Side::BID);
    uint64_t id2 = ob.submit_order(100.00, 30, 2, Side::BID);

    auto depth = ob.get_depth(Side::BID, 10);
    CHECK(depth[0].quantity == 80);

    CHECK(ob.cancel_order(id1) == true);

    depth = ob.get_depth(Side::BID, 10);
    CHECK(depth[0].quantity == 30);
    CHECK(depth[0].order_count == 1);
}
