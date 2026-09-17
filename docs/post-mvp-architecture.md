# Post-MVP Architecture

> **Purpose**
>
> This document records long-term architectural directions for extending the Event-Driven Backtester beyond the MVP.
>
> It is intentionally non-binding. Nothing in this document should delay MVP implementation or be treated as a current production requirement.
>
> The purpose is to preserve architectural intent where future evolution is already foreseeable, especially for live trading, richer market data, execution infrastructure, scheduling, event transport, and performance-sensitive systems.

---

# 1. Guiding Principle

The long-term architecture should preserve the same core domain semantics across historical backtesting and live trading whenever practical.

The preferred direction is:

```text
shared domain model
+
shared event semantics
+
different infrastructure adapters
```

The backtesting engine should not become a disposable implementation that must later be replaced by an unrelated live-trading architecture.

At the same time, future requirements must not force premature abstractions into the MVP.

---

# 2. Shared Backtest and Live Architecture

The intended long-term direction is:

```text
                  ┌── HistoricalDataFeed
                  │
Market source ────┤
                  │
                  └── LiveMarketDataAdapter
                           ↓
                      MarketEvent
                           ↓
                       EventQueue
                           ↓
                         Engine
                           ↓
                        Strategy
                           ↓
                      SignalEvent
                           ↓
               Portfolio / PositionSizer
                           ↓
                       OrderEvent
                           ↓
               ┌───────────┴───────────┐
               ↓                       ↓
        ExecutionHandler          Broker Adapter
               ↓                       ↓
               └───────────┬───────────┘
                           ↓
                       FillEvent
                           ↓
                        Portfolio
```

The main architectural principle is that backtesting and live trading should differ primarily through their boundary adapters rather than through incompatible domain semantics.

---

# 3. Domain Events Remain Stable

The core event protocol is expected to remain centered around:

- `MarketEvent`
- `SignalEvent`
- `OrderEvent`
- `FillEvent`

These events represent domain facts rather than infrastructure details.

Future extensions may add new event types when a concrete domain requirement appears, but the existing meanings should remain stable where possible.

Infrastructure metadata should not be added directly to every event unless it is part of the event's domain meaning.

---

# 4. Live Market Data Adapter

A future live-trading system will require a component that converts external market-data messages into the same market-domain representation used by the engine.

Conceptually:

```text
Exchange / Market Data Provider
        ↓
LiveMarketDataAdapter
        ↓
canonical internal market representation
        ↓
MarketEvent
```

Responsibilities may eventually include:

- provider protocol handling
- subscription management
- symbol mapping
- timestamp extraction
- sequence tracking
- reconnect behavior
- normalization into internal domain types
- detection of malformed or inconsistent provider messages

The live adapter should remain outside strategy and portfolio logic.

---

# 5. Broker Adapter

A future broker adapter will connect internal `OrderEvent` instructions to an external execution venue or brokerage API.

Conceptually:

```text
OrderEvent
        ↓
BrokerAdapter
        ↓
broker / exchange
        ↓
execution reports
        ↓
FillEvent
```

Potential responsibilities include:

- order submission
- cancellation
- modification
- broker order identifiers
- execution identifiers
- rejection handling
- partial fills
- execution status updates
- connectivity and reconnection behavior

The broker adapter should translate external execution messages into internal domain events rather than exposing broker-specific representations directly to the rest of the engine.

---

# 6. EventEnvelope

Future live systems may require metadata that belongs to event transport rather than to the domain payload itself.

A possible abstraction is:

```text
EventEnvelope
    event
    source
    event_time
    received_time
    sequence_number
    correlation_id
```

The purpose is to separate:

```text
domain meaning
```

from:

```text
transport and infrastructure metadata
```

Potential metadata includes:

- event source
- exchange or provider timestamp
- local receive timestamp
- transport sequence number
- correlation identifier
- trace identifier
- ingestion metadata

`EventEnvelope` is not required by the MVP.

It should be introduced only when infrastructure metadata is genuinely needed across multiple event types.

---

# 7. Event Time vs Receive Time

A live system may need to distinguish multiple notions of time.

Possible timestamps include:

```text
event_time
→ when the event occurred at the external source

received_time
→ when the local system received the message

processed_time
→ when the engine processed the event
```

These concepts should remain distinct when they become relevant.

The current daily backtester does not require this distinction because it operates on completed session observations.

---

# 8. Scheduler

`EventQueue` should remain responsible for dispatch order rather than future-time scheduling.

A future scheduler may be introduced for:

- timers
- delayed actions
- scheduled callbacks
- time-based strategy triggers
- deferred execution opportunities
- periodic risk checks
- heartbeat events
- live operational timers

Conceptually:

```text
Scheduler
        ↓
eligible event
        ↓
EventQueue
        ↓
Engine
```

The scheduler should remain a separate component so that queue semantics stay simple.

---

# 9. Event Transport Evolution

The MVP can use a simple FIFO queue.

Future implementations may require stronger transport guarantees or higher throughput.

Possible evolutions include:

- thread-safe FIFO queue
- multi-producer single-consumer queue
- bounded queue
- ring buffer
- lock-free queue
- specialized low-latency transport

These changes should be driven by measured requirements such as:

- concurrency
- throughput
- latency
- memory pressure
- backpressure behavior

The domain event protocol should remain as independent as practical from the underlying transport.

---

# 10. Concurrency Model

The MVP deliberately avoids concurrency.

A future live system may introduce concurrent producers such as:

```text
market-data adapter
broker adapter
scheduler
risk subsystem
internal services
```

Possible long-term models include:

- single-threaded deterministic event loop with concurrent adapters feeding one queue
- dedicated I/O threads with a single engine thread
- actor-like components
- partitioned event processing
- specialized low-latency pipelines

No concurrency model should be selected before the concrete performance and operational requirements are known.

Correctness and deterministic reasoning should remain priorities even when concurrency is introduced.

---

# 11. Multi-Symbol Market Data

The MVP uses one symbol per canonical dataset.

Future market-data infrastructure may support:

- multiple symbols
- synchronized observations
- sparse symbol updates
- symbol-specific trading calendars
- heterogeneous trading sessions

The architecture should avoid assuming that all instruments update simultaneously.

Future synchronization policy must be explicit rather than relying on accidental row alignment.

---

# 12. Intraday and Tick Data

Post-MVP data support may evolve from:

```text
daily bars
```

toward:

```text
intraday bars
ticks
quotes
trades
order-book updates
```

Higher-frequency data may require changes to:

- timestamp precision
- market-data domain types
- storage
- memory management
- event throughput
- scheduling
- latency measurement

The current `Bar` model should not be forced to represent every future market-data type.

New domain types should be introduced when their semantics differ materially.

---

# 13. Order Book and Market Microstructure

A future version may support:

- bid and ask quotes
- Level 1 market data
- Level 2 market data
- limit order books
- order-book reconstruction
- liquidity modeling
- queue position
- market impact

These features should be introduced as dedicated domain concepts rather than by overloading the current daily-bar model.

---

# 14. Order Model Evolution

The MVP supports market orders.

Future order semantics may include:

- limit orders
- stop orders
- stop-limit orders
- time-in-force policies
- order expiration
- cancellation
- modification
- partial execution
- multi-leg structures

New order types should only be added when their execution semantics are implemented and tested.

The existence of an enum value alone must not imply unsupported trading behavior.

---

# 15. Execution Model Evolution

The MVP uses deterministic full fills under a simple next-available-open rule.

Future simulated execution may include:

- partial fills
- spread crossing
- market impact
- latency
- queue position
- volume constraints
- venue-specific behavior
- transaction fees
- commissions
- exchange fees
- rebates

The execution layer should continue to own execution assumptions.

Strategy logic should not embed execution behavior.

---

# 16. Execution Analytics

`FillEvent` should represent execution facts.

Execution-quality analytics should remain derived from those facts and appropriate reference data.

Future analytics may include:

- slippage
- implementation shortfall
- arrival-price performance
- spread cost
- market impact
- realized execution quality

These metrics should not become mandatory fields of the core fill event unless their values are genuinely supplied as execution facts.

---

# 17. Identifiers and Correlation

Future live trading will likely require explicit identifiers.

Possible identifiers include:

- internal order ID
- broker order ID
- execution ID
- strategy ID
- account ID
- correlation ID
- event sequence ID

Identifiers should be introduced when there is a concrete need to correlate asynchronous or externally acknowledged operations.

They should not be added to the MVP solely for hypothetical completeness.

---

# 18. Portfolio and Account Evolution

The MVP portfolio is intentionally small.

Future extensions may include:

- multiple instruments
- multiple currencies
- realized and unrealized PnL
- margin
- leverage
- borrowing
- short positions
- financing costs
- cash interest
- multiple accounts
- broker reconciliation

These features should preserve clear ownership of financial state.

---

# 19. Risk Layer

A professional live system will eventually require explicit risk controls.

Potential future responsibilities include:

- pre-trade limits
- position limits
- notional limits
- leverage limits
- exposure limits
- loss limits
- kill switch
- order throttling
- symbol restrictions
- portfolio-level risk checks

Risk management should become a first-class architectural boundary rather than being hidden inside strategy logic.

---

# 20. Corporate Actions

Future data and portfolio infrastructure may explicitly model:

- dividends
- stock splits
- symbol changes
- mergers
- spin-offs
- delistings

Corporate actions should be represented explicitly when portfolio accounting becomes dependent on them.

The MVP intentionally does not infer or fabricate these effects from adjusted price series.

---

# 21. Trading Calendars

A future market-data layer may require authoritative exchange calendars for:

- expected sessions
- holidays
- early closes
- extraordinary closures
- timezone rules
- daylight-saving transitions

Calendar logic should be centralized rather than duplicated across market-data, strategy, and execution components.

---

# 22. Persistence and Replay

A future system may require durable event storage for:

- debugging
- auditability
- deterministic replay
- incident investigation
- strategy validation
- live/backtest comparison

Possible future designs include:

```text
incoming event
        ↓
persistent event log
        ↓
EventQueue
        ↓
Engine
```

or an equivalent replay architecture.

Persistence should be added only when operational requirements justify it.

---

# 23. Deterministic Replay

A strong long-term objective is the ability to replay recorded market and execution events through the same domain engine.

Conceptually:

```text
recorded event stream
        ↓
replay adapter
        ↓
EventQueue
        ↓
Engine
```

This could improve:

- debugging
- production incident analysis
- regression testing
- live/backtest behavioral comparison

Replay requirements should influence future event metadata only when concrete implementation work begins.

---

# 24. Observability

Future live infrastructure will require operational observability.

Potential concerns include:

- structured logging
- metrics
- latency monitoring
- queue depth
- dropped messages
- reconnects
- order rejects
- execution acknowledgments
- risk alerts
- health checks

Observability should inspect system behavior without becoming part of domain logic.

---

# 25. Configuration

Future live infrastructure may require richer configuration for:

- providers
- broker accounts
- environments
- symbols
- risk limits
- strategy instances
- connectivity
- credentials

Configuration should remain separate from domain behavior.

Secrets must not become ordinary repository configuration.

---

# 26. Performance-Oriented Ownership

The MVP prefers value semantics because they simplify ownership and correctness.

If profiling later shows that high-frequency workloads require different behavior, possible optimizations include:

- immutable shared storage
- handles into stable storage
- object pools
- arena allocation
- zero-copy event payloads
- preallocated buffers

These optimizations should be introduced only after measuring a real bottleneck.

They must not weaken ownership clarity without a justified performance benefit.

---

# 27. Backpressure

A live system may eventually receive events faster than they can be processed.

Future transport design may therefore need explicit backpressure behavior.

Possible concerns include:

- bounded queues
- producer throttling
- message dropping policy
- prioritization
- overload handling
- recovery after backlog

These semantics are operationally important and must not be left accidental once concurrency is introduced.

---

# 28. Fault Tolerance

Future live infrastructure may require explicit failure behavior for:

- network disconnections
- stale market data
- duplicate messages
- missing sequence numbers
- broker outages
- partial acknowledgments
- process restarts
- corrupted local state

Recovery semantics should be explicit before live capital is exposed.

---

# 29. Environment Separation

A future system should clearly distinguish environments such as:

```text
backtest
paper trading
live trading
```

These environments should share as much domain logic as practical while using different infrastructure adapters and operational safeguards.

Paper trading should not require a separate strategy implementation.

---

# 30. Testing Evolution

The test architecture should grow with the system.

Future testing may include:

- deterministic unit tests
- component integration tests
- event replay tests
- broker sandbox tests
- market-data adapter tests
- fault-injection tests
- concurrency tests
- latency benchmarks
- paper-trading validation
- live shadow-mode validation

Deterministic testing should remain the foundation even as operational testing expands.

---

# 31. Security

A future live system will require stronger security boundaries.

Potential concerns include:

- credential storage
- API key management
- account permissions
- network access controls
- audit logging
- secret rotation
- least-privilege broker permissions

Security-sensitive infrastructure should be designed explicitly before live deployment.

---

# 32. Non-Binding Status

The items in this document are architectural directions rather than commitments.

They should be promoted into `architecture.md` only when one of the following occurs:

- implementation begins
- the decision becomes necessary for correctness
- a current interface must preserve a specific future boundary
- the project explicitly accepts the feature into scope

Until then, this document should guide compatibility without forcing premature implementation.

---

# 33. Post-MVP Architectural Principle

The long-term system should evolve by preserving stable domain boundaries while replacing or extending infrastructure around them.

The preferred direction is:

```text
stable domain semantics
+
replaceable adapters
+
explicit event transport
+
measured performance evolution
+
deterministic testing
```

The system should become more capable without making the original backtesting core disposable.
