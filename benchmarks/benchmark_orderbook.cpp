#include "OrderBook.h"
#include <chrono>
#include <vector>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <random>
#include <string>
#include <memory>

struct BenchmarkResult {
    std::string name;
    double avg_ns;
    double p50_ns;
    double p99_ns;
    double p999_ns;
    size_t iterations;
};

template<typename SetupFunc, typename Func>
BenchmarkResult benchmark(const std::string& name, size_t iterations, size_t warmup_iterations, SetupFunc&& setup, Func&& func) {
    setup();
    
    std::vector<int64_t> latencies;
    latencies.reserve(iterations);

    // Warmup
    for (size_t i = 0; i < warmup_iterations; ++i) {
        func(i);
    }

    // Actual benchmark
    for (size_t i = 0; i < iterations; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        func(warmup_iterations + i);
        auto end = std::chrono::high_resolution_clock::now();
        latencies.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    }

    std::sort(latencies.begin(), latencies.end());

    double sum = 0;
    for (auto l : latencies) sum += l;

    return BenchmarkResult{
        name,
        sum / static_cast<double>(iterations),
        static_cast<double>(latencies[iterations / 2]),
        static_cast<double>(latencies[static_cast<size_t>(iterations * 0.99)]),
        static_cast<double>(latencies[static_cast<size_t>(iterations * 0.999)]),
        iterations
    };
}

void print_result(const BenchmarkResult& r) {
    std::cout << std::left << std::setw(40) << r.name
              << " | avg: " << std::right << std::setw(8) << std::fixed << std::setprecision(1) << r.avg_ns << " ns"
              << " | p50: " << std::setw(8) << r.p50_ns << " ns"
              << " | p99: " << std::setw(8) << r.p99_ns << " ns"
              << " | p99.9: " << std::setw(8) << r.p999_ns << " ns"
              << "\n";
    std::cout.flush();
}

void print_header() {
    std::cout << "\n" << std::string(110, '=') << "\n";
    std::cout << "OrderBook Benchmark\n";
    std::cout << std::string(110, '=') << "\n\n";
    std::cout.flush();
}

void print_section(const std::string& name) {
    std::cout << "\n--- " << name << " " << std::string(60 - name.length(), '-') << "\n";
    std::cout.flush();
}

int main() {
    constexpr size_t ITERATIONS = 100000;
    constexpr size_t WARMUP = 1000;

    print_header();

    std::vector<BenchmarkResult> results;

    print_section("Submit Order");

    {
        OrderBook ob(0.01);
        uint64_t ts = 0;
        auto result = benchmark("submit_order (no match, building depth)", ITERATIONS, WARMUP,
            [&]() { ts = 0; },
            [&](size_t i) {
                double price = 50.0 + (i % 1000) * 0.01;
                ob.submit_order(price, 10, ts++, Side::BID);
            });
        print_result(result);
        results.push_back(result);
    }

    {
        OrderBook ob(0.01);
        uint64_t ts = 0;
        auto result = benchmark("submit_order (no match, same level)", ITERATIONS, WARMUP,
            [&]() { ts = 0; },
            [&](size_t) {
                ob.submit_order(100.0, 10, ts++, Side::BID);
            });
        print_result(result);
        results.push_back(result);
    }

    {
        std::unique_ptr<OrderBook> ob_ptr;
        uint64_t ts = 0;
        size_t ask_index = 0;
        constexpr size_t TOTAL_OPS = ITERATIONS + WARMUP;
        std::vector<double> ask_prices;
        ask_prices.reserve(TOTAL_OPS);

        auto result = benchmark("submit_order (immediate full match)", ITERATIONS, WARMUP,
            [&]() {
                ob_ptr = std::make_unique<OrderBook>(0.01);
                ts = 0;
                ask_prices.clear();

                for (size_t i = 0; i < TOTAL_OPS; ++i) {
                    double price = 100.0 + i * 0.01;
                    ask_prices.push_back(price);
                    ob_ptr->submit_order(price, 10, ts++, Side::ASK);
                }
            },
            [&](size_t) {
                double price = ask_prices[ask_index++];
                ob_ptr->submit_order(price, 10, ts++, Side::BID);
            });
        print_result(result);
        results.push_back(result);
    }

    {
        std::unique_ptr<OrderBook> ob_ptr;
        uint64_t ts = 0;
        constexpr size_t TOTAL_OPS = ITERATIONS + WARMUP;

        auto result = benchmark("submit_order (partial match)", ITERATIONS, WARMUP,
            [&]() {
                ob_ptr = std::make_unique<OrderBook>(0.01);
                ts = 0;
                ob_ptr->submit_order(100.0, static_cast<uint64_t>(TOTAL_OPS) * 10, ts++, Side::ASK);
            },
            [&](size_t) {
                ob_ptr->submit_order(100.0, 10, ts++, Side::BID);
            });
        print_result(result);
        results.push_back(result);
    }

    print_section("Cancel Order");

    {
        std::unique_ptr<OrderBook> ob_ptr;
        std::vector<uint64_t> order_ids;
        constexpr size_t TOTAL_OPS = ITERATIONS + WARMUP;
        
        auto result = benchmark("cancel_order (multiple levels)", ITERATIONS, WARMUP,
            [&]() {
                ob_ptr = std::make_unique<OrderBook>(0.01);
                order_ids.clear();
                order_ids.reserve(TOTAL_OPS);
                for (size_t i = 0; i < TOTAL_OPS; ++i) {
                    double price = 100.0 + (i % 1000) * 0.01;
                    order_ids.push_back(ob_ptr->submit_order(price, 10, i, Side::BID));
                }
            },
            [&](size_t i) {
                (void)ob_ptr->cancel_order(order_ids[i]);
            });
        print_result(result);
        results.push_back(result);
    }

    {
        std::unique_ptr<OrderBook> ob_ptr;
        std::vector<uint64_t> order_ids;
        constexpr size_t TOTAL_OPS = ITERATIONS + WARMUP;
        
        auto result = benchmark("cancel_order (single level)", ITERATIONS, WARMUP,
            [&]() {
                ob_ptr = std::make_unique<OrderBook>(0.01);
                order_ids.clear();
                order_ids.reserve(TOTAL_OPS);
                for (size_t i = 0; i < TOTAL_OPS; ++i) {
                    order_ids.push_back(ob_ptr->submit_order(100.0, 10, i, Side::BID));
                }
            },
            [&](size_t i) {
                (void)ob_ptr->cancel_order(order_ids[i]);
            });
        print_result(result);
        results.push_back(result);
    }

    print_section("Modify Order");

    {
        std::unique_ptr<OrderBook> ob_ptr;
        std::vector<uint64_t> order_ids;
        constexpr size_t TOTAL_OPS = ITERATIONS + WARMUP;
        
        auto result = benchmark("modify_order (decrease qty)", ITERATIONS, WARMUP,
            [&]() {
                ob_ptr = std::make_unique<OrderBook>(0.01);
                order_ids.clear();
                order_ids.reserve(TOTAL_OPS);
                for (size_t i = 0; i < TOTAL_OPS; ++i) {
                    double price = 100.0 + (i % 1000) * 0.01;
                    order_ids.push_back(ob_ptr->submit_order(price, 100, i, Side::BID));
                }
            },
            [&](size_t i) {
                (void)ob_ptr->modify_order(order_ids[i], 50, i);
            });
        print_result(result);
        results.push_back(result);
    }

    print_section("Queries");

    {
        OrderBook ob(0.01);
        for (size_t i = 0; i < 1000; ++i) {
            ob.submit_order(100.0 + (i % 100) * 0.01, 10, i, Side::BID);
        }
        
        auto result = benchmark("best_bid()", ITERATIONS, WARMUP,
            []() {},
            [&](size_t) {
                volatile auto* bid = ob.best_bid();
                (void)bid;
            });
        print_result(result);
        results.push_back(result);
    }

    {
        OrderBook ob(0.01);
        for (size_t i = 0; i < 1000; ++i) {
            ob.submit_order(100.0 + (i % 100) * 0.01, 10, i, Side::ASK);
        }
        
        auto result = benchmark("best_ask()", ITERATIONS, WARMUP,
            []() {},
            [&](size_t) {
                volatile auto* ask = ob.best_ask();
                (void)ask;
            });
        print_result(result);
        results.push_back(result);
    }

    print_section("Throughput");

    {
        OrderBook ob(0.01);
        constexpr size_t OPS = 1000000;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (size_t i = 0; i < OPS; ++i) {
            double price = 100.0 + (i % 100) * 0.01;
            Side side = (i % 2 == 0) ? Side::BID : Side::ASK;
            ob.submit_order(price, 10, i, side);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        double ops_per_sec = static_cast<double>(OPS) / (static_cast<double>(duration_ms) / 1000.0);
        
        std::cout << "Mixed operations throughput: " 
                  << std::fixed << std::setprecision(0) << ops_per_sec
                  << " ops/sec (" << OPS << " ops in " << duration_ms << " ms)\n";
    }

    std::cout << "\n" << std::string(110, '=') << "\n";
    std::cout << "Summary\n";
    std::cout << std::string(110, '=') << "\n\n";

    std::cout << "| Operation                               | Avg (ns) | P50 (ns) | P99 (ns) | P99.9 (ns) |\n";
    std::cout << "|-----------------------------------------|----------|----------|----------|------------|\n";
    for (const auto& r : results) {
        std::cout << "| " << std::left << std::setw(39) << r.name 
                  << " | " << std::right << std::setw(8) << static_cast<int>(r.avg_ns)
                  << " | " << std::setw(8) << static_cast<int>(r.p50_ns)
                  << " | " << std::setw(8) << static_cast<int>(r.p99_ns)
                  << " | " << std::setw(10) << static_cast<int>(r.p999_ns) << " |\n";
    }

    std::cout << "\n";

    return 0;
}
