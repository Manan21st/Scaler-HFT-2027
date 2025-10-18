#include <iostream>
#include <vector>
#include <map>
#include <unordered_map>
#include <list>
#include <cstdint>
#include <iomanip>
#include <algorithm>
#include <memory>

struct PriceLevel { 
    double price;
    uint64_t total_quantity;
};

struct Order {
    uint64_t order_id;
    double price;
    uint64_t quantity;
    bool is_buy; 
};

struct BookLevel {
    uint64_t total_quantity = 0;
    std::list<Order> orders;
};

class OrderBook {
private:
    std::map<double, BookLevel, std::greater<double>> bids_;
    std::map<double, BookLevel> asks_; // Manan
    std::unordered_map<uint64_t, std::list<Order>::iterator> order_lookup_;
    void match_orders();
public:
    void add_order(const Order& order);
    bool cancel_order(uint64_t order_id);
    bool amend_order(uint64_t order_id, double new_price, uint64_t new_quantity);
    void get_snapshot(size_t depth, std::vector<PriceLevel>& bids, std::vector<PriceLevel>& asks) const;
    void print_book(size_t depth = 10) const;
};

void OrderBook::add_order(const Order& new_order) {
    if (new_order.is_buy) {
        BookLevel& level = bids_[new_order.price];
        level.total_quantity += new_order.quantity;
        level.orders.push_back(new_order);
        order_lookup_[new_order.order_id] = std::prev(level.orders.end());
    } else {
        BookLevel& level = asks_[new_order.price];
        level.total_quantity += new_order.quantity;
        level.orders.push_back(new_order); // Manan
        order_lookup_[new_order.order_id] = std::prev(level.orders.end());
    }
    match_orders();
}

bool OrderBook::cancel_order(uint64_t order_id) {
    auto it = order_lookup_.find(order_id);
    if (it == order_lookup_.end()) { return false; }

    auto order_it = it->second;
    double price = order_it->price;
    bool is_buy = order_it->is_buy;

    if (is_buy) {
        auto& level = bids_.at(price);
        level.total_quantity -= order_it->quantity;
        level.orders.erase(order_it);
        if (level.total_quantity == 0) bids_.erase(price);
    } else {
        auto& level = asks_.at(price); // Manan
        level.total_quantity -= order_it->quantity;
        level.orders.erase(order_it);
        if (level.total_quantity == 0) asks_.erase(price);
    }
    order_lookup_.erase(it);
    return true;
}

bool OrderBook::amend_order(uint64_t order_id, double new_price, uint64_t new_quantity) {
    auto it = order_lookup_.find(order_id);
    if (it == order_lookup_.end()) { return false; }
    Order old_order = *(it->second);
    cancel_order(order_id);
    add_order({order_id, new_price, new_quantity, old_order.is_buy});
    return true;
}

void OrderBook::get_snapshot(size_t depth, std::vector<PriceLevel>& bids, std::vector<PriceLevel>& asks) const {
    bids.clear(); asks.clear(); bids.reserve(depth); asks.reserve(depth);
    auto bid_it = bids_.begin();
    for (size_t i = 0; i < depth && bid_it != bids_.end(); ++i, ++bid_it) {
        bids.push_back({bid_it->first, bid_it->second.total_quantity});
    }
    auto ask_it = asks_.begin();
    for (size_t i = 0; i < depth && ask_it != asks_.end(); ++i, ++ask_it) {
        asks.push_back({ask_it->first, ask_it->second.total_quantity});
    }
}

void OrderBook::print_book(size_t depth) const {
    std::vector<PriceLevel> bids, asks;
    get_snapshot(depth, bids, asks);

    std::cout << "--- ORDER BOOK (Top " << depth << ") ---\n";
    std::cout << "BIDS (Price: Qty)\t|\tASKS (Price: Qty)\n";
    std::cout << "-------------------------------------------\n";
    std::cout << std::fixed << std::setprecision(2);

    size_t max_rows = std::max(bids.size(), asks.size());
    for(size_t i = 0; i < max_rows; ++i) {
        if (i < bids.size()) { std::cout << bids[i].price << ": " << bids[i].total_quantity; }
        std::cout << "\t\t|\t";
        if (i < asks.size()) { std::cout << asks[i].price << ": " << asks[i].total_quantity; }
        std::cout << "\n";
    }
    std::cout.unsetf(std::ios_base::floatfield);
    std::cout << "-------------------------------------------\n";
}

void OrderBook::match_orders() {
    while (!bids_.empty() && !asks_.empty() && bids_.begin()->first >= asks_.begin()->first) {
        BookLevel& best_bid_level = bids_.begin()->second;
        BookLevel& best_ask_level = asks_.begin()->second;
        Order& buy_order = best_bid_level.orders.front();
        Order& sell_order = best_ask_level.orders.front();

        uint64_t trade_quantity = std::min(buy_order.quantity, sell_order.quantity);

        double trade_price;
        if (buy_order.order_id < sell_order.order_id) {
            trade_price = buy_order.price;
        } else {
            trade_price = sell_order.price;
        }

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "\n>>> TRADE EXECUTED: " << trade_quantity << " units @ " << trade_price << " <<<\n";
        std::cout.unsetf(std::ios_base::floatfield);

        buy_order.quantity -= trade_quantity;
        sell_order.quantity -= trade_quantity;
        best_bid_level.total_quantity -= trade_quantity;
        best_ask_level.total_quantity -= trade_quantity;

        if (buy_order.quantity == 0) {
            order_lookup_.erase(buy_order.order_id);
            best_bid_level.orders.pop_front();
        }
        if (sell_order.quantity == 0) {
            order_lookup_.erase(sell_order.order_id);
            best_ask_level.orders.pop_front();
        }

        if (best_bid_level.total_quantity == 0) bids_.erase(bids_.begin());
        if (best_ask_level.total_quantity == 0) asks_.erase(asks_.begin());
    }
}