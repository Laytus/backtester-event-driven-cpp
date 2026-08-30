# Knowledge Guide

> **Purpose**
>
> This document is the conceptual map of the Event-Driven Backtester project.
> It explains how the main financial concepts, software responsibilities, and
> simulation ideas connect to form the complete system.
>
> It is **not** an implementation plan, bibliography, detailed architecture, or
> MVP scope definition. Those responsibilities belong to:
>
> - `development-guide.md`
> - `references.md`
> - `architecture.md`
> - `MVP-scope.md`

---

# 1. The Big Picture

The project simulates the lifecycle of a trading decision through historical
time.

```text
Historical Market Data
        ↓
HistoricalDataFeed
        ↓
MarketEvent
        ↓
Strategy
        ↓
SignalEvent
        ↓
Portfolio / Position Sizing
        ↓
OrderEvent
        ↓
ExecutionHandler
        ↓
FillEvent
        ↓
Portfolio Update
        ↓
Portfolio Valuation
        ↓
Equity Curve
        ↓
Performance Metrics
        ↓
Reporting
```

The central idea is:

> A backtester should reproduce how information becomes a decision, how that
> decision becomes an order, how execution changes portfolio state, and how the
> resulting portfolio evolves through time.

The system is therefore more than a strategy-return calculator.

It models the **causal lifecycle of a trading decision**.

---

# 2. Main Knowledge Domains

The project connects four main areas.

## 2.1 Market Microstructure

Relevant concepts include:

- markets;
- traders;
- orders;
- trades;
- bid and ask;
- spread;
- liquidity;
- market structure;
- execution.

Main question:

> How does a trading instruction interact with a market?

The MVP simplifies market microstructure substantially, but preserves correct
terminology and the distinction between order creation and execution.

---

## 2.2 Quantitative Backtesting

Relevant concepts include:

- historical data;
- strategy evaluation;
- backtesting biases;
- portfolio accounting;
- return measurement;
- performance analysis;
- reproducibility.

Main question:

> How can a trading strategy be evaluated historically without introducing
> invalid information or assumptions?

---

## 2.3 Event-Driven Systems

Relevant concepts include:

- events;
- producers;
- consumers;
- event queues;
- state transitions;
- deterministic ordering;
- simulation loops;
- separation of responsibilities.

Main question:

> How should the lifecycle of a trading decision be represented in software?

---

## 2.4 Modern C++ Software Design

Relevant concepts include:

- ownership;
- explicit types;
- invariants;
- value semantics;
- deterministic behavior;
- error handling;
- testing;
- maintainability.

Main question:

> How should the financial model be represented safely and clearly in code?

---

# 3. The Core Conceptual Separation

The most important separation in the project is:

```text
Market Information
        ↓
Trading Intent
        ↓
Trading Instruction
        ↓
Execution
        ↓
Portfolio State
```

These stages correspond naturally to:

```text
MarketEvent
→ SignalEvent
→ OrderEvent
→ FillEvent
```

Each event means something different.

This separation prevents strategy logic, capital allocation, execution
assumptions, and accounting from becoming mixed together.

---

# 4. Market Data

Market data represents information observable from the market.

For the MVP, the primary market-data object is a daily OHLCV bar containing:

- timestamp;
- open;
- high;
- low;
- close;
- volume.

Conceptually:

```text
Underlying Market Activity
        ↓
Aggregation
        ↓
Daily OHLCV Bar
```

A bar compresses many underlying market events into a small summary.

Information lost by this compression includes:

- intraday price path;
- exact trade sequence;
- quotes;
- bid/ask changes;
- order-book state;
- intraday liquidity;
- exact timing of the high and low.

Because the MVP does not observe those details, its execution model must remain
explicitly simplified.

---

# 5. Historical Data Pipeline

External provider data is separated from the internal domain model.

```text
External Provider
        ↓
Raw Provider Data
        ↓
Provider-Specific Validation / Normalization
        ↓
Canonical Dataset
        ↓
C++ Reader
        ↓
HistoricalDataFeed
```

The canonical dataset acts as a stable boundary between:

- provider-specific acquisition;
- the provider-independent backtesting engine.

The core domain should reason about market concepts, not provider-specific
column names or library objects.

---

# 6. HistoricalDataFeed

`HistoricalDataFeed` represents the passage of historical market information
through simulated time.

Its conceptual responsibility is:

> Reveal market observations sequentially as the simulation advances.

Conceptually:

```text
Full Historical Dataset
        ↓
HistoricalDataFeed
        ↓
Bar 1
        ↓
Bar 2
        ↓
Bar 3
        ↓
...
```

The complete dataset may exist in memory or storage, but downstream components
must not behave as if future observations are already available.

---

# 7. MarketEvent

A `MarketEvent` means:

> New market information is now available to the simulated trading system.

For the MVP, this normally corresponds to a newly completed daily bar.

```text
HistoricalDataFeed
        ↓
Completed Bar Becomes Available
        ↓
MarketEvent
```

A `MarketEvent` does not imply that a trade should occur.

It only announces that observable market state has advanced.

---

# 8. Strategy

A strategy converts observable market information into trading intent.

```text
Observable Market Information
        ↓
Strategy Logic
        ↓
Trading Intent
```

The strategy may own strategy-specific state such as:

- previous signal state;
- moving-average history;
- warm-up status.

The strategy does **not** own:

- cash;
- executed holdings;
- execution prices;
- fills.

The strategy answers:

> Given the information available now, what do I want the portfolio to do?

---

# 9. Signal

A signal represents trading intent.

For the MVP, the main signal states are:

- `LONG`;
- `EXIT`.

Conceptually:

```text
LONG
→ desire to hold a long position
```

```text
EXIT
→ desire to return to a flat position
```

A signal does not:

- execute a trade;
- modify cash;
- modify executed position quantity;
- choose a fill price.

---

# 10. Signal, Order, and Fill

These three concepts answer different questions.

## Signal

> What does the strategy want?

Example:

```text
LONG
```

## Order

> What instruction should be submitted to achieve that intent?

Example:

```text
BUY 100 SPY MARKET
```

## Fill

> What trade did the simulated execution model actually execute?

Example:

```text
BUY 100 SPY
filled at the next available open
```

The relationship is:

```text
Signal
        ↓
Sizing / Order Generation
        ↓
Order
        ↓
Execution Model
        ↓
Fill
```

This distinction is one of the central design ideas of the project.

---

# 11. Position Sizing

Position sizing converts trading intent into quantity.

```text
Signal
+ Portfolio State
+ Sizing Rule
        ↓
Order Quantity
```

It answers:

> How much should be traded?

It does not decide:

- whether the strategy wants to trade;
- when execution occurs;
- at what price execution occurs.

The MVP uses a simple deterministic sizing policy.

Advanced allocation logic is outside the MVP.

---

# 12. Order

An order represents an instruction submitted to the execution layer.

For the MVP, an order conceptually identifies:

- instrument;
- side;
- quantity;
- order type;
- submission time.

The MVP supports market orders only.

An order is not evidence that portfolio state has already changed.

That happens only after execution produces a fill.

---

# 13. Execution

Execution transforms an order into a fill under a defined simulation model.

The execution layer answers:

- when does the order execute?
- at what simulated price?
- under what assumptions?
- does the order fill?

For the MVP:

```text
Completed Daily Bar
        ↓
Signal
        ↓
Order
        ↓
ExecutionHandler
        ↓
Next Available Daily Open
        ↓
FillEvent
```

The key principle is:

> Execution assumptions belong to the execution layer, not the strategy.

This keeps strategy intent separate from market-execution assumptions.

---

# 14. Fill

A fill represents an executed trade.

A `FillEvent` means that the simulated transaction has occurred.

```text
OrderEvent
        ↓
ExecutionHandler
        ↓
FillEvent
        ↓
Portfolio State Transition
```

A fill can change:

- position quantity;
- cash;
- market exposure.

For the MVP, valid market orders are assumed to fill completely.

---

# 15. Position

A position represents the quantity currently held as the result of executed
trades.

The internal representation is signed:

```text
quantity > 0  → long
quantity = 0  → flat
quantity < 0  → short
```

The MVP permits only long and flat behavior.

Signed quantity nevertheless preserves a clean representation for future short
selling.

A position represents executed holdings, not unexecuted strategy intent.

---

# 16. Portfolio

The portfolio represents the financial state created by executed trading
activity.

It owns concepts such as:

- cash;
- positions;
- market value;
- total equity;
- valuation history.

Conceptually:

```text
Previous Portfolio State
+ Fill
+ Observable Market Price
        ↓
New Portfolio State
```

The portfolio records the consequences of trading decisions.

It does not decide whether a strategy is attractive.

---

# 17. Cash, Market Value, and Equity

These quantities are related but distinct.

## Cash

Funds not currently represented by the market value of positions.

A fill changes cash according to the executed transaction.

## Market Value

The value assigned to the current position using the chosen observable
valuation price.

For one instrument:

```text
Market Value
=
Position Quantity × Valuation Price
```

## Equity

Total portfolio value.

```text
Equity
=
Cash
+
Market Value of Positions
```

Correct accounting of these quantities is one of the central correctness
requirements of the backtester.

---

# 18. Mark-to-Market

Mark-to-market means valuing an existing position using current observable
market information.

It is different from executing a trade.

```text
No New Fill
        ↓
Market Price Changes
        ↓
Position Market Value Changes
        ↓
Portfolio Equity Changes
```

Portfolio equity can therefore change even when:

- cash is unchanged;
- position quantity is unchanged.

---

# 19. Temporal Model

Backtesting is fundamentally a simulation through time.

The MVP uses the following causal sequence:

```text
Daily Bar Becomes Complete
        ↓
Strategy Observes Completed Bar
        ↓
Signal May Be Generated
        ↓
Order Is Submitted
        ↓
Order Waits for Next Available Bar
        ↓
Order Fills at Next Available Open
        ↓
Portfolio Updates from Fill
        ↓
Portfolio Is Valued Using Available Information
```

The governing principle is:

> Information must not influence a decision before that information would have
> been available in the simulated timeline.

---

# 20. Market Time and Information Availability

A timestamp does not automatically mean that every value associated with that
period was already known at that instant.

For a completed daily bar, the full:

- open;
- high;
- low;
- close;
- volume

are only known once the bar is complete.

Therefore the simulation must reason about **information availability**, not
only recorded timestamps.

This distinction is essential for preventing look-ahead bias.

---

# 21. Look-Ahead Bias

Look-ahead bias occurs when a simulated decision uses information that would
not yet have been available at the time of the decision.

Invalid example:

```text
Use Today's Close
        ↓
Generate Signal
        ↓
Pretend the Order Filled at Today's Open
```

The open occurred before the close became known.

The MVP avoids this ambiguity through:

```text
Completed Bar
→ Signal
→ Order
→ Next Available Open Fill
```

This creates a clear causal timeline.

---

# 22. Event-Driven Architecture

An event-driven system models state changes as explicit events.

Rather than evaluating the whole backtest as one monolithic batch calculation,
the simulation progresses through a sequence of events and reactions.

```text
Event Arrives
        ↓
Consumer Reacts
        ↓
State May Change
        ↓
New Event May Be Produced
        ↓
Simulation Continues
```

This structure mirrors the logical lifecycle of a trading system.

---

# 23. Event Producer and Consumer Relationships

The conceptual relationships are:

| Event | Typical Producer | Typical Consumer |
|---|---|---|
| `MarketEvent` | HistoricalDataFeed / engine | Strategy |
| `SignalEvent` | Strategy | Portfolio / sizing logic |
| `OrderEvent` | Portfolio / order-generation logic | ExecutionHandler |
| `FillEvent` | ExecutionHandler | Portfolio |

These relationships define responsibilities.

They do not imply concurrency.

---

# 24. Event Queue

The event queue contains events waiting to be processed.

```text
Producer
        ↓
Event Queue
        ↓
BacktestEngine / Dispatcher
        ↓
Consumer
```

For the MVP, deterministic ordering is more important than throughput.

Therefore:

```text
Event-Driven
≠ Concurrent
```

The engine can be single-threaded and still be event-driven.

---

# 25. BacktestEngine

The `BacktestEngine` coordinates the simulation.

Its conceptual role is to:

- advance simulated time;
- coordinate market-data delivery;
- process pending events;
- route events to the correct consumers.

The engine coordinates the components.

It should not become:

- the strategy;
- the portfolio;
- the execution model;
- the performance calculator.

---

# 26. Determinism

A deterministic backtest produces the same result given the same:

- input data;
- configuration;
- initial capital;
- strategy parameters;
- execution assumptions.

```text
Same Inputs
        ↓
Same Event Sequence
        ↓
Same State Transitions
        ↓
Same Outputs
```

Determinism improves:

- testing;
- debugging;
- reproducibility;
- manual validation;
- regression analysis.

The MVP deliberately prioritizes determinism over unnecessary market realism.

---

# 27. Buy-and-Hold

Buy-and-Hold is the first strategy primarily because it simplifies system
validation.

```text
Simple Strategy Logic
        ↓
Complete Event Lifecycle
        ↓
Portfolio Accounting Validation
```

Because strategy complexity is minimal, failures are easier to attribute to:

- event flow;
- order generation;
- execution;
- portfolio accounting.

Its role is therefore both strategic and diagnostic.

---

# 28. Moving-Average Crossover

The Moving-Average Crossover strategy adds simple stateful trend-following
behavior.

A moving average summarizes recent prices over a rolling window.

```text
Recent Prices
        ↓
Rolling Average
        ↓
Smoothed Price Series
```

With two windows:

```text
Short Moving Average
        versus
Long Moving Average
        ↓
Trend State
```

The strategy introduces:

- rolling calculations;
- warm-up periods;
- stateful signal generation;
- entry and exit transitions.

Its purpose in the MVP is not to demonstrate alpha.

Its purpose is to validate a non-trivial strategy on the same engine.

---

# 29. Why Moving Averages Lag

Moving averages are calculated from historical observations.

Price therefore changes before the moving average fully reacts.

```text
Price Changes
        ↓
Rolling Window Updates
        ↓
Moving Average Reacts
```

This lag is an inherent property of the calculation, not an implementation
error.

---

# 30. Equity Curve and Returns

The equity curve is the time series of portfolio equity.

```text
Time
→ Equity
→ Equity
→ Equity
→ ...
```

It summarizes the financial consequences of:

- strategy decisions;
- position sizing;
- execution;
- portfolio accounting;
- market-price movement.

From the equity curve, the system derives a return series.

Returns measure relative changes in portfolio value and form the basis for most
performance statistics.

The exact return convention must be explicit in the implementation.

---

# 31. Performance Metrics

The MVP uses several metrics because they answer different questions.

## Total Return

> How much did portfolio value change over the complete period?

## Annualized Return

> What annualized growth rate corresponds to the observed performance under the
> project's chosen convention?

## Annualized Volatility

> How variable were periodic returns, expressed on an annual scale?

## Sharpe Ratio

> How much return was achieved relative to return variability, under explicit
> assumptions about the risk-free rate and annualization?

## Maximum Drawdown

> What was the largest decline from a previous equity peak?

No single metric fully describes strategy quality.

The formulas and conventions belong to the performance implementation and must
be validated with known results.

---

# 32. Benchmark

A benchmark provides a reference against which strategy results can be
interpreted.

```text
Strategy Performance
        compared with
Reference Performance
```

A positive return alone does not show that a strategy added value.

For the MVP, Buy-and-Hold provides the natural baseline where applicable.

---

# 33. Performance Layer

The performance layer evaluates completed portfolio history.

```text
Portfolio History
        ↓
Return Series
        ↓
Performance Metrics
        ↓
Performance Summary
```

It does not participate in trading decisions.

This separation prevents analytics from influencing historical simulation
state.

---

# 34. Reporting

Reporting converts backtest outputs into human- or tool-readable forms.

Examples include:

- CSV exports;
- equity-curve data;
- performance summaries;
- plots.

```text
Backtest Results
        ↓
Serialization / Visualization
        ↓
Human-Readable Output
```

Reporting should not alter simulation behavior.

---

# 35. Main Sources of Backtesting Error

The project explicitly guards against several failure modes.

## Look-Ahead Bias

Future information influences an earlier decision.

## Data Leakage

Information outside the valid simulated information set reaches strategy logic.

## Invalid Historical Data

Examples:

- missing required values;
- duplicate timestamps;
- non-monotonic timestamps;
- malformed values;
- inconsistent OHLCV observations.

## Incorrect Event Timing

Examples:

- fill before order submission;
- using a price unavailable at the simulated time;
- duplicated market events.

## Incorrect Portfolio Accounting

Examples:

- position changes before a fill;
- cash updated with the wrong sign;
- inconsistent equity calculation.

## Mixing Strategy and Execution

Example:

- strategy generates a signal and directly assumes a fill price.

That removes the distinction between intent and execution.

---

# 36. Deterministic Fixtures and Manual Oracle

A deterministic fixture is a small dataset with exact expected behavior.

```text
Known Input
        ↓
Known Event Sequence
        ↓
Known State Transitions
        ↓
Known Output
```

A manual oracle independently calculates the expected backtest behavior.

It may specify exact:

- signal timing;
- order timing;
- fill timing;
- execution price;
- quantity;
- cash;
- market value;
- equity.

The first complete backtest should match such an oracle exactly.

This provides much stronger evidence of correctness than checking whether a
real-data result merely looks plausible.

---

# 37. Reproducibility

A backtest is reproducible when the same documented inputs and assumptions can
recreate the same result.

Relevant inputs include:

- data provenance;
- canonical dataset;
- configuration;
- initial capital;
- strategy parameters;
- execution assumptions;
- code version.

Reproducibility connects quantitative methodology with software engineering.

---

# 38. Conceptual Boundaries of the MVP

The MVP deliberately models a simplified market.

It includes:

- one instrument;
- daily bars;
- long and flat behavior;
- market orders;
- deterministic full fills;
- next-available-open execution;
- sequential event processing.

It excludes:

- short selling;
- limit orders;
- partial fills;
- bid/ask-aware execution;
- realistic market impact;
- order-book dynamics;
- multiple assets;
- concurrent processing.

These are deliberate scope choices.

They are not claims about how real markets operate.

---

# 39. Long-Term Evolution

The same conceptual lifecycle can support future extensions.

## More Realistic Execution

```text
Order
        ↓
More Realistic Execution Policy
        ↓
Fill
```

Possible additions:

- commissions;
- slippage;
- bid/ask-aware fills;
- partial fills;
- persistent order lifecycle.

## Additional Order Types

```text
Signal
        ↓
Order
        ↓
Market / Limit / Stop / ...
```

The signal-order-fill distinction remains unchanged.

## Short Selling

Signed positions already represent the conceptual direction:

```text
quantity < 0
→ short
```

Additional borrow, margin, and risk rules would still be required.

## Multiple Assets

The same lifecycle can be extended:

```text
Market Data
→ Strategy
→ Signals
→ Orders
→ Fills
→ Portfolio
```

Portfolio allocation then becomes more important.

## Order-Book Simulation

Execution may later depend on explicit market microstructure:

```text
Order
        ↓
Order Book / Matching Model
        ↓
Fill or Partial Fill
```

## Research Platform

Later research workflows may include:

- parameter experiments;
- walk-forward analysis;
- out-of-sample testing;
- robustness analysis;
- multiple strategy comparisons.

## Production-Oriented Development

Possible future additions include:

- live data;
- paper trading;
- broker adapters;
- persistence;
- monitoring;
- concurrency.

These extend the project without changing the fundamental causal model.

---

# 40. Complete Concept Map

```text
FINANCIAL MARKET
        │
        │ produces observable information
        ▼
MARKET DATA
        │
        │ normalized into
        ▼
CANONICAL BARS
        │
        │ revealed through simulated time
        ▼
HISTORICAL DATA FEED
        │
        │ produces
        ▼
MARKET EVENT
        │
        │ consumed by
        ▼
STRATEGY
        │
        │ generates trading intent
        ▼
SIGNAL EVENT
        │
        │ combined with sizing / portfolio state
        ▼
ORDER EVENT
        │
        │ submitted to
        ▼
EXECUTION HANDLER
        │
        │ applies execution assumptions
        ▼
FILL EVENT
        │
        │ changes
        ▼
PORTFOLIO
        │
        ├── cash
        ├── position
        ├── market value
        └── equity
        │
        │ recorded through time
        ▼
EQUITY CURVE
        │
        │ transformed into
        ▼
RETURN SERIES
        │
        │ evaluated by
        ▼
PERFORMANCE METRICS
        │
        ▼
REPORTING
```

Across the complete chain:

> **Time and information availability constrain every valid transition.**

---

# 41. Four Questions for Every Component

When studying, implementing, or reviewing a component, answer:

1. **What does it represent?**
2. **Why does it exist separately?**
3. **What information does it consume and produce?**
4. **At what point in simulated time can that information exist?**

The fourth question is particularly important.

Many backtesting failures are not arithmetic errors.

They are errors about **when information becomes available and when state is
allowed to change**.

---

# 42. Final Mental Model

The complete project can be understood through one causal chain:

```text
Information Becomes Available
        ↓
Strategy Forms Intent
        ↓
Intent Becomes an Order
        ↓
Execution Turns the Order into a Fill
        ↓
The Fill Changes Portfolio State
        ↓
Market Evolution Changes Portfolio Value
        ↓
Portfolio History Produces Performance Statistics
```

Every major component exists because one stage of this chain has a distinct
responsibility.

The central objective of the backtester is therefore:

> Reproduce this causal sequence through historical time without allowing future
> information to influence the past.

That mental model should remain valid as the project grows beyond the MVP.
