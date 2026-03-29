# High-Performance Order Book

![C++20](https://img.shields.io/badge/C++-20-blue.svg)

A low-latency limit order book implementation in C++20, designed for high-frequency trading systems.

## Performance

Benchmarked on Windows 11, I5-11320H, GCC 15.2 with `-O3 -march=native`:

| Operation | Avg | P50 | P99 | P99.9 |
|-----------|-----|-----|-----|-------|
| Submit order (no match) | 117-172 ns | 100 ns | 200-300 ns | - |
| Submit order (with match) | 61-204 ns | 0-100 ns | 200-800 ns | - |
| Cancel order | 98-110 ns | 100 ns | 300-400 ns | - |
| Modify order | 83 ns | 100 ns | 400 ns | - |
| Best bid/ask query | 26 ns | <100 ns | 100 ns | - |
| Get depth (5 levels) | 83 ns | 100 ns | 200 ns | - |

**Throughput**: 11.0 million operations/second

## Features

- **Sub-200ns latency** for order operations
- **Custom memory pool**
- **Price-time priority**
- **Order types**: Limit, Market, IOC (Immediate-or-Cancel), FOK (Fill-or-Kill)
- **Integer tick prices**
- Real-time trade callbacks

## Build

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build

# Run tests
./build/OrderBookTests

# Run benchmarks
./build/OrderBookBenchmark
```

## Usage

```cpp
#include "OrderBook.h"

int main() {
    OrderBook ob(0.01);  // tick size = $0.01
    
    // Submit orders
    ob.submit_order(100.00, 50, 1, Side::BID);   // Buy 50 @ $100.00
    ob.submit_order(100.05, 30, 2, Side::ASK);   // Sell 30 @ $100.05
    
    // Query book
    Order* best_bid = ob.best_bid();  // $100.00
    Order* best_ask = ob.best_ask();  // $100.05
    double spread = ob.get_spread();  // $0.05
    
    // Match orders
    ob.submit_order(100.05, 20, 3, Side::BID);  // Executes trade
    
    // Get trades
    for (const auto& trade : ob.get_trades()) {
        std::cout << trade.quantity << " @ " << trade.price << "\n";
    }
}
```
