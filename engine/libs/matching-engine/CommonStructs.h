namespace scrimmage::matching {

enum class TimeInForce : char {
    GoodTillCancel = 0,    // Rests indefinitely.
    GoodForDay = 1,        // Rests untill the end of the trading day.
    ImmediateOrCancel = 2, // Fill as much as possible, cancel the remainder.
    FillOrKill = 3,        // Reject entire order if not 100% filled.
    AtTheOpen = 4,         // Typically used in simulation, not live trading.
    GoodTillCancelPlus = 5 // GTC - and do not cross existing orders.
};

enum class OrderSide : char { Buy = 'B', Sell = 'S'};

enum class AddResult : uint8_t {
    Added = 0,
    Filled = 1,
    Partial = 2,
    Rejected = 3,
    Cancelled = 4
};

enum class ExchangeProtocol : uint8_t {
    Ouch = 1,
    Pillar = 2,
    CME = 3
};

static constexpr uint32_t MARKET_ORDER_PRICE = 0;  // Price sentinel for market orders
static constexpr uint16_t POOL_NONE = 0;  // Sentinel for empty/unused pool slots (also used as end-of-list in chains)

} // namespace neuwillow::matching