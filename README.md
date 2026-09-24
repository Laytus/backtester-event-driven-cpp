# Event-Driven Backtester in C++

A deterministic, event-driven backtesting engine built with modern C++23, with Python tooling for market-data acquisition, validation, normalization, and analysis.

> **Status:** In development — Phases 0–2 are complete.
> **Current milestone:** deterministic market-data and event-engine infrastructure.
> **Next milestone:** strategy, portfolio accounting, position sizing, and simulated execution.

## Overview

The project models a backtest as a causal sequence of explicit domain events rather than as a monolithic return calculation.

The complete MVP lifecycle is designed as:

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
Portfolio
        ↓
Performance
```

The implementation currently reaches the deterministic event-engine boundary:

```text
Yahoo Finance
        ↓
Python acquisition / validation / normalization
        ↓
Canonical daily OHLCV CSV
        ↓
CsvBarReader
        ↓
HistoricalDataFeed
        ↓
BacktestEngine
        ↓
MarketEvent
        ↓
EventQueue
        ↓
BacktestEngine::step()
        ↓
Observable Event
```

Strategy behavior, portfolio accounting, position sizing, simulated execution, and performance analytics are intentionally deferred to later MVP phases.

## Current implementation

### Market-data infrastructure

Phase 1 implements a provider-independent historical-data boundary:

- explicit Yahoo Finance acquisition configuration;
- preservation of raw provider data and acquisition metadata;
- provider-specific validation and normalization;
- canonical daily OHLCV representation;
- deterministic canonical CSV serialization;
- C++ `Bar` domain type with explicit invariants;
- `CsvBarReader` with strict validation and contextual errors;
- `HistoricalDataFeed` with sequential delivery and no public future-data access.

The canonical MVP schema is:

```text
session_date,symbol,open,high,low,close,volume
```

Provider-specific behavior remains outside the core C++ simulation domain.

### Event-driven engine

Phase 2 implements the deterministic event infrastructure:

- `MarketEvent`;
- `SignalEvent`;
- `OrderEvent`;
- `FillEvent`;
- closed `Event` representation using `std::variant`;
- type-safe dispatch using `std::visit`;
- value-based event ownership;
- deterministic FIFO `EventQueue`;
- incremental `BacktestEngine`;
- exactly one `MarketEvent` per delivered `Bar`;
- explicit feed exhaustion and deterministic engine termination;
- deterministic canonical-CSV-to-engine integration coverage.

At this stage, the engine processes the market-data portion of the lifecycle end to end. The remaining event types establish the protocol that the later Strategy, Portfolio, and Execution components will use.

## Design principles

The project prioritizes:

- correctness before feature count;
- deterministic and reproducible behavior;
- explicit temporal semantics;
- protection against look-ahead bias;
- clear ownership and component boundaries;
- separation of strategy intent, orders, executions, and portfolio state;
- provider-independent core domain types;
- validation against deterministic fixtures;
- modern C++ features only where they improve correctness or clarity.

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

- a C++23-compatible compiler;
- CMake 3.28 or newer;
- Ninja;
- Python 3.13 or 3.14.

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

The Python package requires Python `>=3.13,<3.15` and currently uses pandas, yfinance, and matplotlib. Python owns provider-specific data tooling and later analysis/visualization; the core simulation engine remains in C++.

## Validation

At the completion of Phase 2, the project passes:

```text
Python tests     → PASS
Ruff             → PASS
C++ Debug        → 64/64 PASS
C++ Release      → 64/64 PASS
git diff --check → PASS
```

Validation covers:

- domain invariants;
- malformed canonical-data rejection;
- sequential historical-data delivery;
- prevention of arbitrary future-data access through the simulation path;
- event construction and representation;
- deterministic FIFO queue behavior;
- engine progression and exhaustion;
- canonical CSV → engine integration;
- repeated-run determinism.

## Project structure

```text
.
├── app/
├── include/backtester/
│   ├── data/
│   ├── engine/
│   ├── events/
│   └── market/
├── src/
│   ├── data/
│   ├── engine/
│   ├── events/
│   └── market/
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

- [`architecture.md`](docs/architecture.md) — implemented architecture, component responsibilities, ownership rules, interfaces, and invariants;
- [`MVP-scope.md`](docs/MVP-scope.md) — fixed MVP assumptions, exclusions, and Definition of Done;
- [`mvp-bar-contract.md`](docs/mvp-bar-contract.md) — provider-independent canonical daily-bar contract;
- [`data-architecture.md`](docs/data-architecture.md) — current data boundaries and long-term data-platform direction;
- [`project-principles.md`](docs/project-principles.md) — permanent development and validation principles;
- [`development-guide.md`](docs/development-guide.md) — phase-by-phase implementation workflow;
- [`knowledge-guide.md`](docs/knowledge-guide.md) — conceptual map of the trading-system lifecycle;
- [`references.md`](docs/references.md) — bibliography and implementation-oriented reading map;
- [`post-mvp-architecture.md`](docs/post-mvp-architecture.md) — explicitly non-binding long-term architectural direction.

## Roadmap

```text
✓ Phase 0 — Project Foundation
✓ Phase 1 — Market Data Infrastructure
✓ Phase 2 — Event-Driven Engine

→ Phase 3 — Trading and Portfolio Layer
  Phase 4 — Strategy and Performance
  Phase 5 — MVP Integration
```

Phase 3 will introduce the first complete trading lifecycle:

```text
MarketEvent
→ Strategy
→ SignalEvent
→ Portfolio / Position Sizing
→ OrderEvent
→ ExecutionHandler
→ FillEvent
→ Portfolio
```

## Long-term direction

After the MVP is complete, the architecture may progressively expand toward richer market data, multiple instruments, more realistic execution, risk controls, persistence and replay, market microstructure, and concurrency where justified, with a possible longer-term path toward live-trading adapters.

These extensions are deliberately kept outside the current MVP so that the core simulation remains small, deterministic, testable, reproducible, and explainable.
