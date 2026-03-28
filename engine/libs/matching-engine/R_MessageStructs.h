#pragma once

// All of these structs will be used in the "hot" path. 
// They are intentionally not padded here because each
// instance will be used in by a single thread in an SPSC ring buffer.

struct AddRequest {
  uint64_t    symbol;           // 8 bytes
  uint64_t    price;            // 8 bytes: 1/10000 dollars
  uint64_t    quantity;         // 8 bytes
  uint64_t    clientOrderId;    // 8 bytes: Echoed back to clients.
  uint64_t    clientTimestamp;  // 8 bytes: Echoed back to clients.
  uint8_t     side;             // 1 byte
  uint8_t     timeInForce;      // 1 byte
}; // 42 bytes

struct alignas(64) AddResult {
  uint64_t   symbol;            // 8 bytes
  uint64_t   price;             // 8 bytes: 1/10000 dollars.
  uint64_t   originalQuantity;  // 8 bytes: Echoed back to clients.
  uint64_t   remainingQuantity; // 8 bytes: Quantity have been matched initially.
  uint64_t   orderId;           // 8 bytes: Exchange-provided order id.
  uint64_t   clientOrderId;     // 8 bytes: Echoed back to clients.
  uint64_t   clientTimestamp;   // 8 bytes: Echoed back to clients.
  uint64_t   exchangeTimestamp; // 8 bytes: Time when exchange sent packet back to client.
}; // 64 bytes

struct alignas ModifyRequest {
  uint64_t    orderId;          // 8 bytes: Exchange-provided order id.  
  uint64_t    symbol;           // 8 bytes
  uint64_t    ogiginqlPrice;    // 8 bytes: 1/10000 dollarsccccc
  uint64_t    newPrice;         // 8 bytes: 1/10000 dollars   
  uint64_t    quantity;         // 8 bytes
  uint64_t    clientOrderId;    // 8 bytes: Echoed back to clients.
  uint64_t    clientTimestamp;  // 8 bytes: Echoed back to clients.
} // 48 bytes

struct alignas(64) ModifyResult {
  uint64_t   orderId;           // 8 bytes: Exchange-provided order id.
  uint64_t   symbol;            // 8 bytes
  uint64_t   newPrice;          // 8 bytes: 1/10000 dollars.
  uint64_t   originalPrice;     // 8 bytes: Echoed back to clients.
  uint64_t   newQuantity;       // 8 bytes: Echoed back to clients.
  uint64_t   remainingQuantity; // 8 bytes: Quantity have been matched initially.
  uint64_t   exchangeTimestamp; // 8 bytes: Time when exchange sent packet back to client.
  uint8_t    result;            // 1 byte : (Cancel failed/ Replaced failed, etc.)
}; // 57 bytes

struct alignas(32) CancelRequest {
  uint64_t   orderId;           // 8 bytes: Exchange-provided order id.
  uint64_t   symbol;            // 8 bytes
  uint64_t   clientTimestamp;   // 8 bytes: Echoed back to clients.  
  uint64_t   exchangeTimestamp; // 8 bytes: Time when exchange sent packet back to client.
}; // 32 bytes

struct alignas(64) CancelResult {
  uint64_t   orderId;           // 8 bytes: Exchange-provided order id.
  uint64_t   symbol;            // 8 bytes
  uint64_t   remainingQuantity; // 8 bytes: Confirming the remaining quantity when officially cancelled.
  uint64_t    clientTimestamp;  // 8 bytes: Echoed back to clients.  
  uint64_t   exchangeTimestamp; // 8 bytes: Time when exchange sent packet back to client.
  uint8_t    result;            // 1 byte : (Cancel failed b/c it was already filled, etc.)
}; // 41 bytes