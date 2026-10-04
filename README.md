# Event-Driven Backtester in C++

A deterministic, event-driven backtesting engine built with modern C++23, with Python tooling for market-data acquisition, validation, normalization, and analysis.

> **Status:** In development — Phases 0–3 are complete.
> **Current milestone:** complete deterministic trading and portfolio lifecycle.
> **Next milestone:** Moving-Average Crossover strategy and performance analytics.

## Overview

The project models a backtest as a causal sequence of explicit domain events rather than as a monolithic return calculation.

The implemented Phase 3 lifecycle is:

```text
Historical Market Data
        ↓
HistoricalDataFeed
        ↓
BacktestEngine
        ↓
completed MarketEvent
        ↓
Strategy
        ↓
optional SignalEvent
        ↓
PositionSizer
        ↓
target position
        ↓
OrderGenerator
        ↓
optional OrderEvent
        ↓
ExecutionHandler pending state
        ↓
first eligible later daily Open
        ↓
optional FillEvent
        ↓
Portfolio accounting
        ↓
session Close valuation
        ↓
PortfolioSnapshot
```

The daily temporal model deliberately separates execution-time information from completed-session information:

```text
Open(t)
→ resolve an order submitted earlier
→ optional FillEvent(t)
→ update Portfolio

completed Bar(t)
→ MarketEvent(t)
→ Strategy
→ optional SignalEvent(t)
→ PositionSizer using Close(t)
→ optional OrderEvent(t)
→ pending for a later open

Close(t)
→ mark Portfolio
→ record one PortfolioSnapshot(t)
```

A decision derived from `Close(t)` therefore cannot execute at `Open(t)`.

Performance analysis and reporting remain later MVP layers.

## Current implementation

### Market-data infrastructure

Phase 1 implements a provider-independent historical-data boundary:

- explicit Yahoo Finance acquisition configuration
- preservation of raw provider data and acquisition metadata
- provider-specific validation and normalization
- canonical daily OHLCV representation
- deterministic canonical CSV serialization
- C++ `Bar` domain type with explicit invariants
- `CsvBarReader` with strict validation and contextual errors
- `HistoricalDataFeed` with sequential delivery and no public future-data access

The canonical MVP schema is:

```text
session_date,symbol,open,high,low,close,volume
```

Provider-specific behavior remains outside the core C++ simulation domain.

### Event-driven engine

Phase 2 establishes the deterministic event infrastructure:

- `MarketEvent`
- `SignalEvent`
- `OrderEvent`
- `FillEvent`
- closed `Event` representation using `std::variant`
- type-safe dispatch using `std::visit`
- value-based event ownership
- deterministic FIFO `EventQueue`
- incremental `BacktestEngine::step()` boundary
- explicit feed exhaustion
- deterministic canonical-CSV-to-engine integration

`EventQueue` is a FIFO dispatch queue, not a market-time scheduler.

### Trading and portfolio layer

Phase 3 completes the first full trading lifecycle:

- runtime-polymorphic `Strategy` contract
- `BuyAndHoldStrategy`
- `Long`, `Short`, and `Flat` target-exposure signal protocol
- signed Portfolio position representation
- fill-driven cash and position accounting
- end-of-session mark-to-market and `PortfolioSnapshot` history
- deterministic `PositionSizer`
- stateless `OrderGenerator`
- one pending market order in `ExecutionHandler`
- next-available-open execution
- execution-time Buy-side cash clipping
- full Sell execution for the requested quantity
- no same-session close-to-open look-ahead
- final-session pending-order discard without synthetic execution
- integrated event dispatch inside `BacktestEngine`
- processed-before-return `step()` semantics
- deterministic event and Portfolio histories

The engine owns orchestration while domain behavior remains separated:

```text
Strategy
→ desired exposure

PositionSizer
→ target holdings

OrderGenerator
→ requested trade

ExecutionHandler
→ execution mechanics

Portfolio
→ executed financial state and valuation

BacktestEngine
→ causal ordering and component coordination
```

## Design principles

The project prioritizes:

- correctness before feature count
- deterministic and reproducible behavior
- explicit temporal semantics
- protection against look-ahead bias
- clear ownership and component boundaries
- separation of strategy intent, orders, executions, and Portfolio state
- provider-independent core domain types
- validation against deterministic fixtures and manual oracles
- modern C++ features only where they improve correctness or clarity

The MVP deliberately remains single-threaded and sequential. Event-driven does not imply concurrent execution.

## MVP scope

The first portfolio-ready version targets a deliberately small market model:

| Property | MVP |
|---|---|
| Instrument | SPY |
| Instruments | Single asset |
| Data | Daily OHLCV |
| Position | Long / flat |
| Orders | Market |
| Execution | Next available daily open |
| Currency | USD |
| Engine | Sequential and deterministic |

The completed MVP will include Buy-and-Hold and Moving-Average Crossover strategies, portfolio accounting, deterministic execution, performance metrics, CSV outputs, and Python visualization.

Features such as short selling, limit orders, intraday data, order books, realistic transaction costs, multi-asset portfolios, concurrency, and live trading are intentionally post-MVP.

## Build

### Prerequisites

- a C++23-compatible compiler
- CMake 3.28 or newer
- Ninja
- Python 3.13 or 3.14

The first CMake configure may require an internet connection to obtain Catch2 v3.

### Debug

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Run the current executable with:

```sh
./build/debug/backtester
```

### Release

```sh
cmake --preset release
cmake --build --preset release
ctest --preset release
```

Run the current executable with:

```sh
./build/release/backtester
```

## Python tooling

Create a virtual environment and install the project with its development dependencies:

```sh
python3.13 -m venv .venv
source .venv/bin/activate
python -m pip install -e '.[dev]'
```

Run the Python tests and linting:

```sh
pytest
ruff check .
```

The Python package requires Python `>=3.13,<3.15` and currently uses pandas, yfinance, and matplotlib.

Python owns provider-specific data tooling and later analysis/visualization. The core simulation engine remains in C++.

## Validation

The Phase 3 closure checkpoint passes:

```text
C++ Debug        → 147/147 PASS
C++ Release      → 147/147 PASS
git diff --check → PASS
```

The Python data-tooling tests and Ruff checks remain part of the standard validation workflow.

Validation through Phase 3 covers:

- `Bar`, event, Strategy, Portfolio, sizing, order-generation, and execution invariants
- malformed canonical-data rejection
- sequential historical-data delivery
- prevention of arbitrary future-data access through the simulation path
- deterministic FIFO event behavior
- exhaustive type-safe engine dispatch
- next-open execution timing
- Buy-side execution clipping at the actual open
- fill-driven Portfolio accounting
- one close snapshot per completed session
- final pending-order discard
- engine progression, exhaustion, and stable completion
- canonical CSV → engine integration
- repeated-run determinism
- complete manual-oracle end-to-end validation

The Phase 3 manual oracle independently verifies a four-session trajectory with:

```text
10 observable Events
4 PortfolioSnapshots
final cash     = 1,135
final position = 0
final equity   = 1,135
```

No production-code change was required for the integrated engine to match the oracle.

## Project structure

```text
.
├── app/
├── include/backtester/
│   ├── data/
│   ├── engine/
│   ├── events/
│   ├── execution/
│   ├── market/
│   ├── portfolio/
│   └── strategy/
├── src/
│   ├── data/
│   ├── engine/
│   ├── events/
│   ├── execution/
│   ├── market/
│   ├── portfolio/
│   └── strategy/
├── python/
│   └── backtester_data/
├── scripts/
├── tests/
│   ├── cpp/
│   └── python/
├── analysis/
├── data/
│   └── fixtures/
└── docs/
```

Only components with an implemented responsibility are introduced into the production architecture.

## Documentation

The repository contains detailed design documentation:

- [`architecture.md`](docs/architecture.md) — implemented architecture, component responsibilities, ownership rules, interfaces, invariants, and Phase implementation outcomes
- [`MVP-scope.md`](docs/MVP-scope.md) — fixed MVP assumptions, exclusions, and Definition of Done
- [`mvp-bar-contract.md`](docs/mvp-bar-contract.md) — provider-independent canonical daily-bar contract
- [`data-architecture.md`](docs/data-architecture.md) — current data boundaries and long-term data-platform direction
- [`project-principles.md`](docs/project-principles.md) — permanent development and validation principles
- [`development-guide.md`](docs/development-guide.md) — phase-by-phase implementation workflow
- [`phase_3.md`](docs/phase_3.md) — detailed Phase 3 temporal, financial, implementation, and validation contracts
- [`practical_cpp_notes.md`](docs/practical_cpp_notes.md) — C++ concepts and implementation patterns actually used by the codebase
- [`knowledge-guide.md`](docs/knowledge-guide.md) — conceptual map of the trading-system lifecycle
- [`references.md`](docs/references.md) — bibliography and implementation-oriented reading map
- [`post-mvp-architecture.md`](docs/post-mvp-architecture.md) — explicitly non-binding long-term architectural direction

## Roadmap

```text
✓ Phase 0 — Project Foundation
✓ Phase 1 — Market Data Infrastructure
✓ Phase 2 — Event-Driven Engine
✓ Phase 3 — Trading and Portfolio Layer

→ Phase 4 — Strategy and Performance
  Phase 5 — MVP Integration
```

Phase 4 extends the functioning engine rather than redesigning its core architecture:

```text
Moving-Average primitive
        ↓
Moving-Average Crossover Strategy
        ↓
Portfolio equity history
        ↓
return series
        ↓
performance metrics
        ↓
benchmark comparison
```

The current Phase 3 engine already provides the deterministic trading, execution, accounting, and valuation path that Phase 4 will consume.

## Long-term direction

After the MVP is complete, the architecture may progressively expand toward richer market data, multiple instruments, more realistic execution, risk controls, persistence and replay, market microstructure, and concurrency where justified, with a possible longer-term path toward live-trading adapters.

These extensions are deliberately kept outside the current MVP so that the core simulation remains small, deterministic, testable, reproducible, and explainable.
