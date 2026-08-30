# MVP Scope

> **Purpose**
>
> This document defines the fixed scope of the Event-Driven Backtester MVP.
> Its purpose is to prevent scope creep, make implementation decisions explicit,
> and establish clear completion criteria for the first portfolio-ready version
> of the project.
>
> The project is expected to continue after the MVP. Features excluded here are
> deferred, not necessarily rejected.

---

# 1. MVP Objective

Build a small, correct, deterministic, and reproducible event-driven backtesting engine in modern C++ that is suitable for:

- demonstrating entry-level Quant Developer skills;
- validating the core architecture of the long-term project;
- showing sound backtesting methodology;
- providing a foundation that can be extended after the MVP.

The MVP must prioritize:

- correctness;
- clear component boundaries;
- explicit time semantics;
- deterministic behavior;
- reproducibility;
- strong validation;
- concise documentation;
- interview explainability.

The MVP does **not** need to demonstrate a profitable strategy.

---

# 2. Fixed MVP Market Model

The MVP uses the following market assumptions:

| Item | MVP Decision |
|---|---|
| Instrument | SPY |
| Number of instruments | One |
| Asset class | U.S. equity ETF |
| Data frequency | Daily bars |
| Market data | OHLCV |
| Position direction | Long or flat |
| Position representation | Signed quantity |
| Order type | Market order |
| Execution model | Next available daily open |
| Engine | Sequential and deterministic |
| Currency | USD |
| Data source | External provider normalized into a provider-independent canonical format |

The internal domain model must not depend on a specific market-data provider.

---

# 3. Historical Market Data

## 3.1 Included

The MVP includes:

- historical daily OHLCV data;
- raw provider data preservation;
- provider-specific normalization;
- provider-independent canonical dataset;
- deterministic timestamp ordering;
- missing-value validation;
- duplicate-timestamp validation;
- basic OHLCV consistency validation;
- C++ CSV parsing;
- sequential historical data delivery;
- protection against access to future bars.

The canonical bar representation contains at least:

- timestamp;
- open;
- high;
- low;
- close;
- volume.

## 3.2 Data Pipeline

The intended data flow is:

```text
External Data Provider
        ↓
Raw Provider Data
        ↓
Validation / Normalization
        ↓
Canonical Dataset
        ↓
C++ Reader
        ↓
Historical Market Data Feed
```

Python may be used for:

- data acquisition;
- provider-specific cleaning;
- validation;
- normalization;
- visualization.

C++ owns the backtesting engine and core domain behavior.

---

# 4. Event-Driven Model

The MVP is explicitly event-driven.

The intended lifecycle is:

```text
Historical Market Data Feed
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
Execution Handler
        ↓
FillEvent
        ↓
Portfolio Update
        ↓
Equity / Performance
```

Each major component must have a clear and limited responsibility.

Event-driven does **not** imply concurrent or asynchronous execution.

The MVP engine is single-threaded, sequential, and deterministic.

---

# 5. Temporal Model

Time semantics must be explicit.

For the MVP:

```text
Daily bar becomes complete
        ↓
Strategy observes the completed bar
        ↓
Signal may be generated
        ↓
Order is submitted
        ↓
Order executes at the next available daily open
        ↓
Portfolio is updated from the fill
        ↓
Portfolio value is recorded using information available at that time
```

This model is chosen to avoid look-ahead bias and ambiguous same-bar execution assumptions.

The strategy must not choose the execution price.

Execution timing and execution-price assumptions belong to the execution layer.

---

# 6. Strategy Scope

The MVP contains exactly two strategies.

## 6.1 Buy-and-Hold

Purpose:

- validate the complete event flow;
- provide a simple deterministic integration case;
- establish a benchmark reference.

The strategy enters a long position according to the configured sizing rule and remains invested for the remainder of the backtest.

## 6.2 Moving-Average Crossover

Purpose:

- demonstrate stateful signal generation;
- demonstrate rolling market-data use;
- provide a simple non-trivial trading strategy;
- validate strategy-to-portfolio interaction.

The exact short and long moving-average windows must be configurable.

The MVP does not require optimization of those parameters.

---

# 7. Signal Model

Signals represent trading intent.

The MVP requires at least:

- `LONG`;
- `EXIT`.

A signal:

- does not execute a trade;
- does not directly modify cash;
- does not directly modify positions;
- does not choose the execution price.

The strategy owns signal-generation logic.

---

# 8. Order Model

The MVP supports market orders only.

An order must explicitly represent at least:

- instrument;
- side;
- quantity;
- order type;
- submission time.

The design must not prevent future support for additional order types.

The MVP does not require a complete persistent order lifecycle.

---

# 9. Execution Model

The MVP uses a deterministic simulated execution handler.

Its responsibilities include:

- receiving market orders;
- determining when they execute;
- determining the simulated execution price;
- producing `FillEvent`s.

The fixed MVP execution rule is:

> A market order generated after a completed daily bar is filled at the next available daily open.

The MVP assumes that valid market orders are fully filled.

---

# 10. Portfolio and Position Accounting

The portfolio owns:

- cash;
- position quantity;
- market value;
- total equity;
- portfolio history.

The MVP supports:

```text
quantity > 0  → long
quantity = 0  → flat
```

The internal representation uses signed quantities so that short selling can be added later without replacing the position representation.

The MVP must correctly update:

- quantity after fills;
- cash after fills;
- market value;
- total portfolio equity.

Portfolio accounting must be validated against deterministic manually calculated examples.

---

# 11. Position Sizing

The MVP requires a simple deterministic position-sizing policy.

The exact policy must be explicit and reproducible.

It must be sufficient to convert a valid trading signal into an order quantity.

The MVP does not require:

- portfolio optimization;
- volatility targeting;
- Kelly sizing;
- dynamic leverage;
- multi-asset capital allocation.

---

# 12. Performance Measurement

The MVP reports at least:

- total return;
- annualized return;
- annualized volatility;
- Sharpe ratio;
- maximum drawdown.

It must also support comparison with a buy-and-hold benchmark where applicable.

Every quantitative metric must have:

- an explicit formula;
- documented assumptions;
- a deterministic known-result test.

A specific return or Sharpe ratio is **not** part of the Definition of Done.

---

# 13. Output and Reporting

The MVP includes:

- CSV export of relevant backtest results;
- equity-curve data;
- performance summary;
- Python-based visualization;
- at least one portfolio/equity plot;
- benchmark comparison where applicable.

Reporting must remain separate from core simulation logic.

The plotting layer may depend on Python libraries such as pandas and matplotlib.

---

# 14. CLI and Configuration

The MVP includes a minimal command-line interface sufficient to run the backtester reproducibly.

Configuration must support the essential inputs required to reproduce a run, including where applicable:

- input dataset;
- strategy;
- strategy parameters;
- initial capital;
- output location.

The MVP does not require a graphical user interface.

---

# 15. Testing and Validation

Testing is part of the MVP scope.

The project must include:

- parser tests;
- data-validation tests;
- event-flow tests;
- strategy tests;
- portfolio-accounting tests;
- execution tests;
- performance-metric tests;
- integration tests;
- at least one end-to-end deterministic test.

## 15.1 Manual Oracle

The first complete backtest must be validated against a small manually calculated deterministic dataset.

The oracle must make it possible to verify:

- event order;
- signal timing;
- order timing;
- fill timing;
- execution price;
- position quantity;
- cash;
- market value;
- equity.

Real historical data is not a substitute for this validation.

---

# 16. Reproducibility

A completed MVP must allow another developer to:

- clone the repository;
- install or obtain documented dependencies;
- acquire or generate the required data;
- configure the project;
- build Debug and Release versions;
- run all tests;
- execute both MVP strategies;
- regenerate outputs;
- regenerate plots;
- reproduce the documented results.

Data provenance, assumptions, configuration, and execution instructions must be explicit.

---

# 17. Documentation Required for the MVP

The repository must contain enough documentation to explain:

- project objective;
- MVP scope;
- system architecture;
- event flow;
- component responsibilities;
- temporal model;
- execution assumptions;
- portfolio-accounting assumptions;
- performance metrics;
- build instructions;
- run instructions;
- testing instructions;
- main limitations;
- post-MVP direction.

Documentation should remain concise and useful.

The MVP must not be delayed by unnecessary documentation.

---

# 18. Explicit MVP Exclusions

The following features are outside the MVP.

## 18.1 Trading Model

- short selling;
- limit orders;
- stop orders;
- partial fills;
- persistent order lifecycle;
- cancellations;
- rejected orders;
- expired orders;
- realistic bid/ask execution;
- market impact;
- sophisticated slippage models;
- realistic commissions;
- borrow constraints;
- margin models.

## 18.2 Market Data

- intraday bars;
- tick data;
- quote data;
- Level I market data;
- Level II market data;
- order-book reconstruction;
- multiple simultaneous instruments;
- multiple exchanges;
- multiple currencies.

## 18.3 Strategy and Research

- mean-reversion strategies;
- advanced momentum strategies;
- statistical arbitrage;
- machine learning;
- parameter optimization;
- walk-forward analysis;
- large-scale sensitivity analysis;
- Monte Carlo analysis;
- automated strategy discovery.

## 18.4 Portfolio and Risk

- multi-asset portfolio construction;
- risk-parity allocation;
- volatility targeting;
- Kelly sizing;
- leverage optimization;
- sophisticated risk models.

## 18.5 Infrastructure

- live trading;
- paper trading;
- broker integration;
- streaming market data;
- distributed processing;
- concurrency;
- database persistence;
- production monitoring;
- production deployment infrastructure.

These exclusions may be revisited only after the MVP Definition of Done is satisfied.

---

# 19. Architecture Constraints for Future Extension

Although post-MVP features are excluded, the MVP must preserve a small number of architectural boundaries that prevent unnecessary rewrites.

The MVP should preserve:

- provider-independent market-data types;
- explicit event types;
- explicit timestamps;
- separation between strategy and execution;
- separation between portfolio and market data;
- signed position quantities;
- explicit order type;
- replaceable execution policy;
- reporting separated from simulation;
- sequential deterministic event processing.

These constraints justify clean boundaries.

They do **not** justify implementing unused post-MVP features in advance.

---

# 20. Scope-Control Rule

A new feature may enter the MVP only if it is required to:

1. make the existing MVP correct;
2. make the existing MVP reproducible;
3. validate a core component;
4. remove a serious architectural flaw;
5. materially improve the portfolio/interview value without creating significant additional scope.

Otherwise, it is deferred.

When uncertain:

> Prefer the smallest correct implementation that satisfies the current MVP.

---

# 21. MVP Definition of Done

The MVP is complete when all of the following are true.

## Build

- [ ] Clean Debug build succeeds.
- [ ] Clean Release build succeeds.
- [ ] Required compiler warnings are enabled.
- [ ] The project builds without warnings.

## Data

- [ ] Historical data can be acquired or generated reproducibly.
- [ ] Raw and canonical data are separated.
- [ ] Canonical data validation is implemented.
- [ ] Historical bars are delivered sequentially without future-data access.

## Engine

- [ ] The event model is explicit.
- [ ] The event queue is deterministic.
- [ ] Market events are processed chronologically.
- [ ] The complete simulation loop works end to end.

## Trading

- [ ] Buy-and-Hold works.
- [ ] Moving-Average Crossover works.
- [ ] Signals, orders, and fills remain separate concepts.
- [ ] Market orders execute according to the next-open rule.

## Portfolio

- [ ] Cash accounting is correct.
- [ ] Position accounting is correct.
- [ ] Market value is correct.
- [ ] Equity is correct.
- [ ] A deterministic manual oracle is reproduced exactly.

## Performance

- [ ] Total return is implemented and validated.
- [ ] Annualized return is implemented and validated.
- [ ] Annualized volatility is implemented and validated.
- [ ] Sharpe ratio is implemented and validated.
- [ ] Maximum drawdown is implemented and validated.
- [ ] Benchmark comparison is available.

## Testing

- [ ] Unit tests pass.
- [ ] Integration tests pass.
- [ ] End-to-end tests pass.
- [ ] Quantitative calculations have known-result tests.
- [ ] Important state transitions are validated deterministically.

## Outputs

- [ ] CSV results are generated.
- [ ] Equity-curve data are generated.
- [ ] Python plots can be regenerated.
- [ ] Results are reproducible from documented configuration.

## Documentation

- [ ] README is complete.
- [ ] Architecture is understandable.
- [ ] Temporal assumptions are documented.
- [ ] Execution assumptions are documented.
- [ ] Main limitations are documented.
- [ ] Build, test, and run instructions are complete.

## Interview Readiness

- [ ] The complete event lifecycle can be explained clearly.
- [ ] The reason for separating signal, order, and fill can be explained.
- [ ] The temporal model and look-ahead protection can be explained.
- [ ] Portfolio accounting can be explained.
- [ ] Performance metrics and their limitations can be explained.
- [ ] The main MVP trade-offs and deferred features can be explained.

---

# 22. Final Scope Principle

The MVP exists to prove that the core system is:

```text
correct
+ deterministic
+ testable
+ reproducible
+ explainable
+ extensible through clean boundaries
```

Anything beyond what is necessary to establish those properties belongs after the MVP.
