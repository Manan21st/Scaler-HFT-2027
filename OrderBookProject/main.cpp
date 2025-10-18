#include "OrderBook.cpp"

// Manan Agrawal 23bcs10206 
// OrderBook Capstone Project (High Performance C++)

int main() {
    OrderBook book;

    std::cout << "--- 1. Setting up initial book state ---\n";
    book.add_order({1001, 100.50, 10, true});
    book.add_order({1002, 100.50, 5,  true});
    book.add_order({1003, 99.75, 20, true});
    book.add_order({2001, 101.00, 15, false});
    book.add_order({2002, 101.25, 10, false});
    book.add_order({2003, 101.00, 5,  false});
    book.print_book(5);

    std::cout << "\n--- 2. Adding aggressive BUY order to trigger matches ---\n";
    book.add_order({3001, 101.50, 25, true});
    book.print_book(5);

    std::cout << "\n--- 3. Canceling an existing order (ID 1003) ---\n";
    book.cancel_order(1003);
    // Manan
    book.print_book(5);

    std::cout << "\n--- 4. Amending an existing order (ID 2002) to trigger a match ---\n";
    book.amend_order(2002, 100.25, 20);
    book.print_book(5);

    std::cout << "\n--- 5. Getting a market data snapshot ---\n"; // Manan
    std::vector<PriceLevel> bids, asks;
    book.get_snapshot(3, bids, asks);

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "BIDS (Snapshot):\n";
    if (bids.empty()) { std::cout << "  (empty)\n"; }
    else { for(const auto& level : bids) { std::cout << "  Price: " << level.price << ", Qty: " << level.total_quantity << "\n"; } }
    
    std::cout << "ASKS (Snapshot):\n";
    if (asks.empty()) { std::cout << "  (empty)\n"; }
    else { for(const auto& level : asks) { std::cout << "  Price: " << level.price << ", Qty: " << level.total_quantity << "\n"; } }
    std::cout.unsetf(std::ios_base::floatfield);

    return 0;
}