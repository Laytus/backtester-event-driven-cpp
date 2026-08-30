# Event-Driven Backtester in C++

A learning-focused, event-driven backtesting system built with modern C++.
The repository currently contains only the foundations needed for Phase 0 and
a small C++ scaffold for beginning Phase 1.

## Prerequisites

- A C++23 compiler
- CMake 3.28 or newer
- Ninja
- Python 3.13 or newer

## C++ quick start

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
./build/dev/backtester
```

The first configure downloads Catch2 v3, so it requires an internet
connection.

## Python quick start

```sh
python3.13 -m venv .venv
source .venv/bin/activate
python -m pip install -e '.[dev]'
pytest
ruff check .
```

Exploratory scripts belong in `analysis/`; downloaded and derived datasets
belong in `data/raw/` and `data/processed/`. Data files are intentionally not
tracked by Git.

## Current layout

- `docs/`: principles, development guidance, domain knowledge, and Phase 0 notes
- `include/backtester/`: public C++ headers
- `src/`: the minimal library implementation
- `app/`: command-line entry point
- `tests/cpp/`: C++ tests using Catch2 v3
- `tests/python/`: Python tests using pytest
- `analysis/`: small research and data-preparation scripts
- `data/`: ignored local market data

Engine, strategy, execution, and portfolio modules will be introduced only
when their project phases require them.

