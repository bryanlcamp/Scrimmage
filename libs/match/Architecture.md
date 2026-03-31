+------------------------+           +------------------------+
|  Client / Exchange     |           |  Synthetic Data        |
|  TCP/UDP Orders        |           |  Injector (optional)   |
|  (FIX, proprietary)    |           |  Streams fake orders   |
+-----------+------------+           +-----------+------------+
            |                                |
            | Order / Cancel / Heartbeat     | Synthetic Orders
            v                                v
+-----------+------------+      +-----------+------------+
|  SPSC Ring Buffers      |<----+ OrderBookFeed / Pool   |
|  (per symbol range)     |      |  Maintains bids/asks  |
+-----------+------------+      +-----------+------------+
            |                                ^
            | Order events                   |
            v                                |
+-----------+------------+                   |
|   Matching Engine       |                   |
|   Core Logic            |------------------+
|   - Match bids/asks     |
|   - Generate fills      |
|   - Update book         |
+-----------+------------+
            |
            | Execution Reports (TCP)
            | Match Reports (UDP multicast)
            v
+-----------+------------+
|  Clients / Market Data  |
|  Consumers             |
+------------------------+