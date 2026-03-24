#include "OrderBook.h"
#include <benchmark/benchmark.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace {
constexpr double kTickSize = 0.01;
constexpr uint64_t kQty = 10;

void preload_asks_increasing(OrderBook& ob, std::vector<double>& ask_prices, size_t count, uint64_t& ts) {
    ask_prices.clear();
    ask_prices.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        const double price = 100.0 + static_cast<double>(i) * 0.01;
        ask_prices.push_back(price);
        ob.submit_order(price, kQty, ts++, Side::ASK);
    }
}

void preload_bids_multiple_levels(OrderBook& ob, std::vector<uint64_t>& ids, size_t count) {
    ids.clear();
    ids.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        const double price = 100.0 + static_cast<double>(i % 1000) * 0.01;
        ids.push_back(ob.submit_order(price, kQty, static_cast<uint64_t>(i), Side::BID));
    }
}

void preload_bids_single_level(OrderBook& ob, std::vector<uint64_t>& ids, size_t count) {
    ids.clear();
    ids.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        ids.push_back(ob.submit_order(100.0, kQty, static_cast<uint64_t>(i), Side::BID));
    }
}

void preload_bids_for_modify(OrderBook& ob, std::vector<uint64_t>& ids, size_t count) {
    ids.clear();
    ids.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        const double price = 100.0 + static_cast<double>(i % 1000) * 0.01;
        ids.push_back(ob.submit_order(price, 100, static_cast<uint64_t>(i), Side::BID));
    }
}

// submit_order (no match, building depth)
static void BM_SubmitNoMatchBuildingDepth(benchmark::State& state) {
    OrderBook ob(kTickSize);
    ob.reserve_orders(static_cast<size_t>(state.range(0)) * 2);
    uint64_t ts = 0;

    for (auto _ : state) {
        const double price = 50.0 + static_cast<double>(ts % 1000) * 0.01;
        benchmark::DoNotOptimize(ob.submit_order(price, kQty, ts++, Side::BID));
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_SubmitNoMatchBuildingDepth)->Arg(100000)->Unit(benchmark::kNanosecond);

// submit_order (no match, same level)
static void BM_SubmitNoMatchSameLevel(benchmark::State& state) {
    OrderBook ob(kTickSize);
    ob.reserve_orders(static_cast<size_t>(state.range(0)) * 2);
    uint64_t ts = 0;

    for (auto _ : state) {
        benchmark::DoNotOptimize(ob.submit_order(100.0, kQty, ts++, Side::BID));
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_SubmitNoMatchSameLevel)->Arg(100000)->Unit(benchmark::kNanosecond);

// submit_order (immediate full match)
static void BM_SubmitImmediateFullMatch(benchmark::State& state) {
    const size_t batch = static_cast<size_t>(state.range(0));

    std::unique_ptr<OrderBook> ob;
    std::vector<double> ask_prices;
    uint64_t ts = 0;
    size_t ask_index = 0;

    auto reset = [&]() {
        ob = std::make_unique<OrderBook>(kTickSize);
        ob->reserve_orders(batch * 2);
        ts = 0;
        ask_index = 0;
        preload_asks_increasing(*ob, ask_prices, batch, ts);
    };

    state.PauseTiming();
    reset();
    state.ResumeTiming();

    for (auto _ : state) {
        if (ask_index >= ask_prices.size()) {
            state.PauseTiming();
            reset();
            state.ResumeTiming();
        }

        benchmark::DoNotOptimize(ob->submit_order(ask_prices[ask_index++], kQty, ts++, Side::BID));
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_SubmitImmediateFullMatch)->Arg(100000)->Unit(benchmark::kNanosecond);

// submit_order (partial match)
static void BM_SubmitPartialMatch(benchmark::State& state) {
    const size_t batch = static_cast<size_t>(state.range(0));

    std::unique_ptr<OrderBook> ob;
    uint64_t ts = 0;
    uint64_t remaining = 0;

    auto reset = [&]() {
        ob = std::make_unique<OrderBook>(kTickSize);
        ob->reserve_orders(batch + 1);
        ts = 0;
        remaining = static_cast<uint64_t>(batch) * kQty;
        ob->submit_order(100.0, remaining, ts++, Side::ASK);
    };

    state.PauseTiming();
    reset();
    state.ResumeTiming();

    for (auto _ : state) {
        if (remaining == 0) {
            state.PauseTiming();
            reset();
            state.ResumeTiming();
        }

        benchmark::DoNotOptimize(ob->submit_order(100.0, kQty, ts++, Side::BID));
        remaining -= kQty;
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_SubmitPartialMatch)->Arg(100000)->Unit(benchmark::kNanosecond);

// cancel_order (multiple levels)
static void BM_CancelMultipleLevels(benchmark::State& state) {
    const size_t batch = static_cast<size_t>(state.range(0));

    std::unique_ptr<OrderBook> ob;
    std::vector<uint64_t> ids;
    size_t idx = 0;

    auto reset = [&]() {
        ob = std::make_unique<OrderBook>(kTickSize);
        ob->reserve_orders(batch);
        preload_bids_multiple_levels(*ob, ids, batch);
        idx = 0;
    };

    state.PauseTiming();
    reset();
    state.ResumeTiming();

    for (auto _ : state) {
        if (idx >= ids.size()) {
            state.PauseTiming();
            reset();
            state.ResumeTiming();
        }

        benchmark::DoNotOptimize(ob->cancel_order(ids[idx++]));
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_CancelMultipleLevels)->Arg(100000)->Unit(benchmark::kNanosecond);

// cancel_order (single level)
static void BM_CancelSingleLevel(benchmark::State& state) {
    const size_t batch = static_cast<size_t>(state.range(0));

    std::unique_ptr<OrderBook> ob;
    std::vector<uint64_t> ids;
    size_t idx = 0;

    auto reset = [&]() {
        ob = std::make_unique<OrderBook>(kTickSize);
        ob->reserve_orders(batch);
        preload_bids_single_level(*ob, ids, batch);
        idx = 0;
    };

    state.PauseTiming();
    reset();
    state.ResumeTiming();

    for (auto _ : state) {
        if (idx >= ids.size()) {
            state.PauseTiming();
            reset();
            state.ResumeTiming();
        }

        benchmark::DoNotOptimize(ob->cancel_order(ids[idx++]));
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_CancelSingleLevel)->Arg(100000)->Unit(benchmark::kNanosecond);

// modify_order (decrease qty)
static void BM_ModifyDecreaseQty(benchmark::State& state) {
    const size_t batch = static_cast<size_t>(state.range(0));

    std::unique_ptr<OrderBook> ob;
    std::vector<uint64_t> ids;
    size_t idx = 0;
    uint64_t ts = 0;

    auto reset = [&]() {
        ob = std::make_unique<OrderBook>(kTickSize);
        ob->reserve_orders(batch);
        preload_bids_for_modify(*ob, ids, batch);
        idx = 0;
        ts = 0;
    };

    state.PauseTiming();
    reset();
    state.ResumeTiming();

    for (auto _ : state) {
        if (idx >= ids.size()) {
            state.PauseTiming();
            reset();
            state.ResumeTiming();
        }

        benchmark::DoNotOptimize(ob->modify_order(ids[idx++], 50, ts++));
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_ModifyDecreaseQty)->Arg(100000)->Unit(benchmark::kNanosecond);

// best_bid()
static void BM_BestBid(benchmark::State& state) {
    OrderBook ob(kTickSize);
    ob.reserve_orders(1000);
    for (size_t i = 0; i < 1000; ++i) {
        ob.submit_order(100.0 + static_cast<double>(i % 100) * 0.01, kQty, static_cast<uint64_t>(i), Side::BID);
    }

    for (auto _ : state) {
        benchmark::DoNotOptimize(ob.best_bid());
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_BestBid)->Unit(benchmark::kNanosecond);

// best_ask()
static void BM_BestAsk(benchmark::State& state) {
    OrderBook ob(kTickSize);
    ob.reserve_orders(1000);
    for (size_t i = 0; i < 1000; ++i) {
        ob.submit_order(100.0 + static_cast<double>(i % 100) * 0.01, kQty, static_cast<uint64_t>(i), Side::ASK);
    }

    for (auto _ : state) {
        benchmark::DoNotOptimize(ob.best_ask());
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_BestAsk)->Unit(benchmark::kNanosecond);

// get_depth(BID, 5)
static void BM_GetDepthBid5(benchmark::State& state) {
    OrderBook ob(kTickSize);
    ob.reserve_orders(1000);
    for (size_t i = 0; i < 1000; ++i) {
        ob.submit_order(100.0 + static_cast<double>(i % 100) * 0.01, kQty, static_cast<uint64_t>(i), Side::BID);
    }

    for (auto _ : state) {
        auto depth = ob.get_depth(Side::BID, 5);
        benchmark::DoNotOptimize(depth);
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_GetDepthBid5)->Unit(benchmark::kNanosecond);

// get_spread()
static void BM_GetSpread(benchmark::State& state) {
    OrderBook ob(kTickSize);
    ob.reserve_orders(2000);
    for (size_t i = 0; i < 1000; ++i) {
        ob.submit_order(100.0 + static_cast<double>(i % 100) * 0.01, kQty, static_cast<uint64_t>(i), Side::BID);
        ob.submit_order(101.0 + static_cast<double>(i % 100) * 0.01, kQty, static_cast<uint64_t>(1000 + i), Side::ASK);
    }

    for (auto _ : state) {
        benchmark::DoNotOptimize(ob.get_spread());
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_GetSpread)->Unit(benchmark::kNanosecond);

// get_mid_price()
static void BM_GetMidPrice(benchmark::State& state) {
    OrderBook ob(kTickSize);
    ob.submit_order(100.0, 10, 1, Side::BID);
    ob.submit_order(100.01, 10, 2, Side::ASK);

    for (auto _ : state) {
        benchmark::DoNotOptimize(ob.get_mid_price());
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_GetMidPrice)->Unit(benchmark::kNanosecond);

// Mixed operations throughput
static void BM_MixedThroughput(benchmark::State& state) {
    OrderBook ob(kTickSize);
    ob.reserve_orders(static_cast<size_t>(state.range(0)));
    uint64_t ts = 0;

    for (auto _ : state) {
        const double price = 100.0 + static_cast<double>(ts % 100) * 0.01;
        const Side side = (ts % 2 == 0) ? Side::BID : Side::ASK;
        benchmark::DoNotOptimize(ob.submit_order(price, kQty, ts, side));
        ++ts;
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MixedThroughput)->Arg(1000000)->Unit(benchmark::kNanosecond);

}

BENCHMARK_MAIN();
