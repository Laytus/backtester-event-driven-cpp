# References

> **Purpose**
>
> This document is the single source of truth for the bibliography of the
> Event-Driven Backtester MVP.
>
> It defines:
>
> - the official project references;
> - the role of each reference;
> - the minimum reading required for each development phase;
> - material that is optional or reference-only.
>
> Reading is driven by implementation needs. The objective is not to complete
> books or articles before coding.

---

# 1. Reading Policy

The MVP follows three reading levels.

## Required

Read before or during the corresponding phase because the material is directly
needed to understand or implement the phase correctly.

## Optional

Read only when clarification is needed or when additional conceptual depth is
useful.

## Reference-only

Do not read proactively for the MVP. Consult only when a concrete
implementation or conceptual question appears.

The governing rule is:

```text
Read only what is needed
→ answer the implementation question
→ return to development
```

Previously completed study does not need to be repeated unless a specific
question remains unresolved.

---

# 2. Official MVP References

## 2.1 Anatoly B. Schmidt — Financial Markets and Trading

**Full title:** *Financial Markets and Trading: An Introduction to Market
Microstructure and Trading Strategies*

**Edition used:** 2011

**Primary role:**

- market terminology;
- traders and market participants;
- order types;
- bid and ask;
- spread;
- liquidity;
- market structure;
- prices and returns;
- moving-average trading rules;
- strategy performance concepts.

This is the primary market-microstructure reference for the MVP.

It replaces Larry Harris — *Trading and Exchanges* as the project's official
MVP microstructure reference.

Harris may remain a future supplementary reference, especially for deeper
post-MVP market microstructure work, but it is not required for MVP
development.

---

## 2.2 Ernest P. Chan — Quantitative Trading, 2nd Edition

**Full title:** *Quantitative Trading: How to Build Your Own Algorithmic Trading
Business*

**Edition used:** 2nd edition, 2021

**Primary role:**

- historical data;
- backtesting methodology;
- performance measurement;
- backtesting pitfalls;
- transaction-cost awareness;
- quantitative research methodology.

This is the primary backtesting-methodology reference for the MVP.

Chapter 3 — **Backtesting** is the main required chapter.

Relevant sections include:

- Finding and Using Historical Databases;
- Performance Measurement;
- Common Backtesting Pitfalls to Avoid;
- Transaction Costs.

Not every section is required in every phase.

---

## 2.3 QuantStart — Event-Driven Backtesting with Python

**Series used:** *Event-Driven Backtesting with Python*

**Primary role:**

- event-driven architecture;
- event types;
- event queue;
- historical data handler;
- strategy layer;
- portfolio layer;
- execution layer;
- performance implementation.

The series is an architectural reference.

It must **not** be translated line by line into C++.

The project should preserve the useful separation of responsibilities while
designing types and ownership according to the needs of this C++ codebase.

Relevant MVP articles:

- Part I — Event-driven backtester overview;
- Part II — Event hierarchy;
- Part III — DataHandler;
- Part IV — Strategy;
- Part V — Portfolio;
- Part VI — ExecutionHandler;
- Part VII — Performance.

Part VIII concerns live Interactive Brokers execution and is outside the MVP.

---

## 2.4 Ernest P. Chan — Algorithmic Trading

**Full title:** *Algorithmic Trading: Winning Strategies and Their Rationale*

**Edition used:** 2013

**MVP role:** supplementary only.

This book is **not required reading for the MVP**.

Potentially useful material:

- Chapter 1 — Backtesting and Automated Execution.

The following material is not required for the MVP:

- mean-reversion chapters;
- Interday Momentum Strategies;
- Intraday Momentum Strategies;
- advanced risk-management material.

In particular, Chapter 6 — **Interday Momentum Strategies** is not required to
implement the MVP moving-average crossover strategy.

---

# 3. Technical References

Technical documentation should be consulted on demand rather than read
proactively.

## C++

Primary source:

- cppreference.

Use for:

- C++23 language and standard-library behavior;
- `std::chrono`;
- `std::variant`;
- `std::visit`;
- `std::optional`;
- `std::expected`;
- ranges;
- containers;
- algorithms;
- ownership-related standard-library facilities.

Prefer authoritative documentation over tutorials when exact behavior matters.

---

## CMake

Primary source:

- official CMake documentation.

Use for:

- target-based CMake;
- compiler features;
- target properties;
- testing integration;
- build configuration.

---

## Catch2

Primary source:

- official Catch2 v3 documentation.

Use for:

- test cases;
- sections;
- matchers;
- floating-point comparisons;
- CMake integration.

---

## Python Data Tooling

Consult official documentation as needed for:

- Python;
- pandas;
- yfinance;
- matplotlib;
- pytest;
- Ruff.

These tools support acquisition, normalization, validation, testing, and
visualization.

They do not define the core financial-domain architecture.

---

# 4. Phase Reading Map

The reading map below is authoritative for the MVP.

If `development-guide.md` contains a different reading assignment, this
document takes precedence.

---

# Phase 0 — Foundations

## Objective

Understand the complete lifecycle of the backtester and the minimum market
concepts needed to reason about the system.

## Required

### Schmidt — Chapter 1: Financial Markets: Traders, Orders, and Systems

Relevant sections:

- Traders;
- Orders;
- The Bid/Ask Spread;
- Liquidity;
- Market Structures.

Purpose:

- establish correct market terminology;
- understand the distinction between orders and trades;
- understand why execution exists as a separate concern.

### QuantStart — Part I

Purpose:

- understand event-driven backtesting;
- identify the major components;
- understand the complete event lifecycle;
- understand the role of the event queue.

### Chan — Quantitative Trading, Chapter 3

Read only the material required to understand:

- realistic historical simulation;
- look-ahead bias and related backtesting pitfalls.

## Optional

### Schmidt — Chapter 2: Modern Financial Markets

Relevant sections:

- The U.S. Equity Markets;
- High-Frequency Trading — overview only.

Useful for market context, but not required to implement the MVP.

## Completion Note

Phase 0 theory should not be repeatedly studied before implementation.

Once the system lifecycle, temporal model, and component boundaries are clear,
development should proceed.

---

# Phase 1 — Market Data Infrastructure

## Objective

Build a valid historical-data pipeline and sequential market-data feed.

## Required

### Chan — Quantitative Trading, Chapter 3

Read:

- Finding and Using Historical Databases;
- relevant data-related material from Common Backtesting Pitfalls to Avoid.

Focus on:

- historical-data quality;
- correct use of historical datasets;
- data-related sources of backtest error.

### QuantStart — Part III

Focus on:

- DataHandler responsibility;
- historical versus live-data interface concept;
- sequential delivery of market data;
- preventing strategy code from directly owning data acquisition.

## Reference-only

Use provider and Python-library documentation when implementing:

- download behavior;
- provider-specific fields;
- normalization;
- CSV output;
- missing-value handling;
- date parsing.

Provider-specific behavior should be verified against the provider
documentation rather than inferred from generic finance texts.

---

# Phase 2 — Event-Driven Engine

## Objective

Implement the event protocol, event queue, and deterministic simulation loop.

## Required

### QuantStart — Part I

Revisit only the sections describing:

- Event;
- Event Queue;
- main event loop;
- component interactions.

### QuantStart — Part II

Focus on:

- `MarketEvent`;
- `SignalEvent`;
- `OrderEvent`;
- `FillEvent`;
- event routing.

The Python class hierarchy is illustrative.

The C++ implementation should use the simplest type model that preserves
explicit event semantics and deterministic dispatch.

## Optional

No additional finance reading is required unless an event has unclear
financial meaning.

---

# Phase 3 — Trading and Portfolio Layer

## Objective

Complete the first end-to-end trading lifecycle:

```text
MarketEvent
→ Strategy
→ SignalEvent
→ Portfolio
→ OrderEvent
→ ExecutionHandler
→ FillEvent
→ Portfolio
```

## Required

### QuantStart — Part IV

Focus on:

- Strategy responsibility;
- market-data input;
- signal generation;
- separation between strategy and portfolio.

### QuantStart — Part V

Focus on:

- position tracking;
- holdings;
- signal-to-order transition;
- fill-driven portfolio updates;
- equity-curve state.

Do not reproduce QuantStart's portfolio implementation mechanically.

### QuantStart — Part VI

Focus on:

- ExecutionHandler responsibility;
- OrderEvent to FillEvent transition;
- separation between trading intent and simulated execution.

QuantStart's current-price fill assumption is not the project's execution rule.

The MVP uses the temporal and execution model defined in `MVP-scope.md`.

### Schmidt — Chapter 1: Orders

Revisit only if needed to clarify:

- order terminology;
- market-order meaning;
- distinction between an order and an executed trade.

## Optional

### Chan — Algorithmic Trading, Chapter 1

Use only if additional context is needed on:

- backtesting versus execution;
- automated execution;
- backtesting pitfalls.

### Chan — Quantitative Trading, Chapter 5: Execution Systems

Reference only if a broader understanding of automated execution is useful.

It is not required to implement the deterministic MVP execution handler.

## Explicitly Not Required

### Schmidt — Chapter 13: Execution Strategies

Not required for Phase 3.

Its focus includes:

- benchmark-driven schedules;
- cost-driven schedules;
- the taker's dilemma.

These topics concern execution optimization beyond the deterministic
next-bar-open execution model required by the MVP.

---

# Phase 4 — Strategy and Performance

## Objective

Implement the moving-average crossover strategy and validated performance
measurement.

## Required

### Schmidt — Chapter 10: Technical Trading Strategies

Read:

- Trend Strategies;
- Moving-Average Rules.

Purpose:

- understand the financial meaning of moving-average trading rules;
- understand why moving averages are lagging indicators;
- implement the MVP strategy without treating it as an arbitrary coding
  exercise.

The broader momentum and oscillator material is not required.

### Chan — Quantitative Trading, Chapter 3

Read:

- Performance Measurement;
- relevant material from Common Backtesting Pitfalls to Avoid.

Focus on:

- returns;
- Sharpe ratio;
- drawdown;
- benchmark comparison;
- limitations of backtest performance.

### QuantStart — Part VII

Focus on:

- deriving performance metrics from the completed equity curve;
- Sharpe-ratio implementation structure;
- maximum-drawdown implementation structure.

QuantStart's formulas and conventions must not be copied without checking the
project's chosen definitions.

Each metric in the project must have an explicit convention and deterministic
known-result test.

## Optional

### Schmidt — Chapter 7: Statistical Distributions and Dynamics of Returns

Relevant sections:

- Prices and Returns;
- Random Walk and Predictability of Returns.

Useful for additional conceptual depth on return series.

### Schmidt — Chapter 12: Back-Testing of Trading Strategies

Relevant section:

- Performance Measures.

Useful as a secondary perspective.

The sections on:

- Resampling Techniques;
- Comparing Trading Strategies;

are not required for the MVP.

## Explicitly Not Required

### Chan — Algorithmic Trading, Chapter 6: Interday Momentum Strategies

Not required for the moving-average crossover implementation.

The chapter studies multi-day momentum strategies and their economic
mechanisms; it is not necessary for the deliberately simple MVP trend strategy.

---

# Phase 5 — MVP Integration

## Objective

Make the repository reproducible, runnable, testable, and portfolio-ready.

## Required

No finance-book reading is required.

Use technical documentation only when necessary.

Relevant references include:

- CMake documentation;
- Catch2 documentation;
- cppreference;
- Python documentation;
- pandas documentation;
- matplotlib documentation;
- pytest documentation;
- Ruff documentation.

The focus of this phase is integration and reproducibility rather than new
theory.

---

# 5. Minimum Required Reading Summary

This table represents the minimum planned reading for implementation.

| Phase | Required material |
|---|---|
| Phase 0 | Schmidt Ch.1; QuantStart Part I; selected Chan Ch.3 pitfalls |
| Phase 1 | Chan Ch.3 historical-data sections; QuantStart Part III |
| Phase 2 | QuantStart Parts I–II |
| Phase 3 | QuantStart Parts IV–VI; Schmidt Ch.1 Orders as needed |
| Phase 4 | Schmidt Ch.10 Moving-Average Rules; Chan Ch.3 Performance Measurement; QuantStart Part VII |
| Phase 5 | Technical documentation only |

This is intentionally much smaller than the original reading plan.

---

# 6. Material Deferred Beyond the MVP

The following topics may become important later but must not delay the MVP.

## Market Microstructure

Potential future reading:

- deeper Schmidt market-microstructure chapters;
- Larry Harris — *Trading and Exchanges*;
- limit-order market literature.

Relevant when implementing:

- limit orders;
- bid/ask-aware execution;
- transaction-cost models;
- order books;
- matching engines.

---

## Execution

Potential future reading:

- Schmidt Chapter 13 — Execution Strategies;
- Chan execution material;
- specialized execution and transaction-cost literature.

Relevant when implementing:

- execution scheduling;
- slippage;
- market impact;
- transaction-cost optimization.

---

## Advanced Backtesting Methodology

Potential future reading:

- Schmidt Chapter 12 beyond basic performance measures;
- Chan material on deeper strategy validation;
- dedicated literature on out-of-sample testing and overfitting.

Relevant when implementing:

- parameter optimization;
- walk-forward analysis;
- bootstrap methods;
- robustness analysis;
- multiple-comparison controls.

---

## Strategy Expansion

Potential future reading:

- Chan — *Algorithmic Trading* mean-reversion chapters;
- Chan — Interday Momentum Strategies;
- other systematic-strategy literature.

Relevant only after the MVP strategy layer and accounting are validated.

---

# 7. Reference Priority Rules

When multiple references discuss the same topic, use the following order.

## Market terminology and microstructure

1. Schmidt;
2. supplementary sources only if needed.

## Backtesting methodology and biases

1. Chan — *Quantitative Trading*;
2. Chan — *Algorithmic Trading*, Chapter 1 if additional detail is needed;
3. supplementary sources only for unresolved questions.

## Event-driven architecture

1. the project's own `architecture.md`;
2. QuantStart as the external architectural reference.

Once an architectural decision is explicitly adopted in `architecture.md`,
the project design takes precedence over QuantStart.

## C++ behavior

1. cppreference / standard-oriented documentation;
2. compiler or library documentation where relevant.

## Build and testing tools

1. official tool documentation.

## Provider-specific market-data behavior

1. provider documentation;
2. library documentation;
3. empirical validation against small known datasets.

Never infer provider-specific behavior solely from a finance textbook.

---

# 8. Conflict Resolution

This document owns bibliography and phase reading assignments.

Therefore:

- `references.md` decides what is required reading;
- `development-guide.md` may point to this document but should not maintain a
  competing bibliography;
- `knowledge-guide.md` may mention sources conceptually but should not define
  reading requirements;
- `project-principles.md` should define only the general rule that authoritative
  references are used;
- `MVP-scope.md` should not contain reading assignments.

If another project document conflicts with this bibliography, update that
document to match `references.md`.

---

# 9. Final Reading Principle

The bibliography exists to support implementation, not to delay it.

For the MVP:

```text
Concrete implementation question
        ↓
Consult the minimum relevant source
        ↓
Resolve the question
        ↓
Document the decision if necessary
        ↓
Return to code
```

Do not complete additional chapters merely because they are related to the
project.

A reference becomes useful when it improves the correctness, clarity, or
explainability of a concrete implementation decision.
