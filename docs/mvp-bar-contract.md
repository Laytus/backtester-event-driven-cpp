# Canonical Daily Bar Contract

> **Status:** Frozen for the MVP
> **Phase:** Phase 1 — Market Data Infrastructure
> **Related:** `data-architecture.md`
>
> This document defines the provider-independent canonical daily `Bar` contract used by the Event-Driven Backtester MVP and records how the contract should evolve post-MVP.

---

## 1. Purpose

A `Bar` represents the aggregated market activity of one instrument during one completed trading session.

For the MVP, each `Bar` represents one daily trading session of SPY:

```text
session_date
symbol
open
high
low
close
volume
```

The C++ backtester consumes only this canonical representation. Provider-specific conventions must be resolved upstream.

---

## 2. MVP domain contract

Conceptually:

```cpp
struct Bar {
    std::chrono::year_month_day session_date;
    std::string symbol;

    double open;
    double high;
    double low;
    double close;

    std::uint64_t volume;
};
```

The implementation may add constructors or validation helpers, but the financial meaning of these fields is frozen for the MVP.

---

## 3. Instrument identity

For the MVP, `symbol` is the operational instrument identifier.

```text
symbol = SPY
```

This is an MVP simplification, not the long-term identity model.

Post-MVP:

```text
symbol
    ↓
Instrument Master
    ↓
permanent instrument_id
```

Ticker changes and other mutable identifiers must eventually be maintained as historical metadata rather than treated as permanent identity.

---

## 4. Session date and timezone semantics

The daily bar uses:

```text
session_date
```

Format:

```text
YYYY-MM-DD
```

Example:

```text
2026-09-03
```

`session_date` identifies the trading session in the exchange's local trading calendar.

It does not represent:

- midnight UTC;
- exact market open time;
- exact market close time.

Timezone metadata belongs conceptually to the venue/calendar layer rather than being repeated in every daily bar.

Post-MVP intraday/live convention:

```text
event timestamp → UTC
venue/calendar  → local trading-session interpretation
```

---

## 5. OHLC semantics and types

The fields:

```text
open
high
low
close
```

represent the canonical prices for the complete trading session.

MVP type:

```cpp
double
```

The adjustment convention is defined in Section 7.

Fixed-point or decimal representations may be reconsidered post-MVP if execution, accounting, venue, or asset-class requirements justify them.

---

## 6. Volume semantics

`volume` represents the total trading volume aggregated over the complete session represented by the bar.

For SPY:

```text
unit = shares
```

It does not represent:

- number of trades;
- number of orders;
- dollar notional.

MVP type:

```cpp
std::uint64_t
```

Volume must be consistent with the split-adjustment convention used by the canonical dataset.

---

## 7. MVP price-adjustment policy

The MVP canonical dataset uses:

```text
split-adjusted OHLC
split-consistent volume
NOT dividend-adjusted prices
```

This is a deliberate MVP compatibility policy.

### Why split-adjusted

The MVP portfolio does not yet process corporate actions explicitly.

With raw prices, a 2-for-1 stock split could appear approximately as:

```text
pre-split close = 100
post-split open = 50
```

Without explicit split handling, the backtester could interpret this as an economic loss.

Split adjustment removes this artificial discontinuity.

### Why not dividend-adjusted

Dividend-adjusted or total-return-adjusted prices rewrite historical prices to incorporate distributions. They are useful for research and analytics, but they are not literal historical execution prices.

The MVP therefore avoids dividend adjustment while using split adjustment to compensate for a corporate-action capability that has not yet been implemented.

### Raw acquisition

Provider defaults must not determine canonical semantics implicitly.

For yfinance, the intended acquisition configuration is explicit:

```python
auto_adjust=False
actions=True
repair=False
```

Raw provider output is preserved separately.

The normalization layer creates the canonical split-adjusted, non-dividend-adjusted OHLCV series.

The exact split-only transformation must be deterministic and tested before it is trusted.

### Post-MVP target

The professional target architecture is:

```text
unadjusted market prices
        +
explicit corporate actions
        ↓
execution + portfolio accounting
```

with separate derived analytical datasets:

```text
split-adjusted
dividend-adjusted
total-return
```

The MVP split-adjusted canonical representation is therefore not the permanent execution-price architecture.

---

## 8. Canonical CSV schema

Required header:

```csv
session_date,symbol,open,high,low,close,volume
```

Example:

```csv
session_date,symbol,open,high,low,close,volume
2026-09-01,SPY,642.10,645.30,640.80,644.75,68732100
2026-09-02,SPY,644.20,647.90,643.60,647.10,72184500
```

Rules:

- UTF-8;
- comma delimiter;
- header required;
- decimal separator `.` ;
- no thousands separators;
- date format `YYYY-MM-DD`;
- no optional columns;
- no empty required fields;
- one row per instrument per trading session.

The C++ parser must not understand yfinance-specific columns or conventions.

---

## 9. Bar-level invariants

Every canonical `Bar` must satisfy:

```text
symbol is not empty

open  > 0
high  > 0
low   > 0
close > 0

all prices are finite

high >= open
high >= close
low  <= open
low  <= close
high >= low
```

`volume` is unsigned and therefore cannot be negative.

Zero volume may be representable, but suspicious zero-volume observations belong to data-quality review rather than silent correction.

---

## 10. Dataset-level invariants

For the single-instrument MVP:

```text
session_date values are strictly increasing
one Bar exists at most once per session_date
duplicate session dates are forbidden
```

The parser must not silently sort unordered canonical input.

---

## 11. Missing-data policy

### Missing field inside a Bar

A row with any missing required field is invalid.

The MVP does not use:

- empty OHLCV values;
- `null`;
- `NaN`;
- forward filling;
- interpolation;
- zero as a replacement value.

The row is rejected.

### Missing trading session

A gap between observed session dates is not automatically repaired.

It may indicate:

- holiday;
- market closure;
- missing vendor data;
- upstream processing error.

MVP rule:

```text
possible missing session
        ↓
flag / investigate upstream
        ↓
never fabricate a Bar
```

`HistoricalDataFeed` delivers the canonical bars that exist. A future calendar layer will determine whether an expected session is genuinely missing.

---

## 12. Validation responsibilities

### Python data pipeline

Python owns:

- provider-specific parsing;
- raw-to-canonical normalization;
- split adjustment;
- required-field validation;
- missing-value detection;
- timestamp validation;
- chronological validation;
- duplicate detection;
- OHLC consistency;
- anomaly detection;
- provider-specific quality handling.

### C++ domain boundary

C++ independently validates the conditions required to protect the simulation domain:

- expected schema/header;
- required column count;
- date parsing;
- numeric parsing;
- finite prices;
- positive prices;
- OHLC relationships;
- valid volume;
- non-empty symbol;
- strict chronological order;
- duplicate dates.

Responsibility boundary:

```text
Python → establish data quality and canonicalize provider data
C++    → enforce the canonical contract before simulation
```

---

## 13. Parser errors

The C++ parser follows:

> **Fail loudly; never silently repair canonical input.**

It must reject at least:

- incorrect header;
- missing or unexpected columns;
- invalid date;
- empty symbol;
- malformed numeric values;
- `NaN` or infinite prices;
- non-positive OHLC values;
- invalid OHLC relationships;
- invalid volume;
- missing required values;
- duplicate session dates;
- non-monotonic session dates.

Where practical, an error should identify:

```text
input/file
line number
field or record
offending value
reason
```

Example:

```text
Canonical CSV error
line: 143
field: high
value: 418.20
reason: high < close (419.10)
```

The exact C++ exception/error type is deferred to the parser implementation task.

---

## 14. Parser error resolution

Parser errors are resolved upstream:

```text
C++ detects invalid canonical input
        ↓
simulation stops
        ↓
error identifies contract violation
        ↓
raw/normalization pipeline is investigated
        ↓
canonical dataset is corrected or rebuilt
        ↓
simulation is rerun
```

The C++ parser does not guess intended values.

---

## 15. Temporal use by the backtester

A daily `Bar` becomes observable only after the represented session is complete.

MVP lifecycle:

```text
completed daily Bar
        ↓
HistoricalDataFeed
        ↓
MarketEvent
        ↓
Strategy observes completed OHLCV
        ↓
SignalEvent
        ↓
OrderEvent
        ↓
fill at next available session open
```

The strategy must never access future bars through the normal public interface.

---

## 16. Decisions frozen for the MVP

- `symbol` is the MVP instrument identifier.
- One `Bar` represents one complete daily trading session.
- `session_date` uses `YYYY-MM-DD`.
- The date identifies the exchange-local trading session, not an exact instant.
- Timezone metadata belongs to the venue/calendar concept.
- OHLC use `double`.
- Volume uses `std::uint64_t`.
- SPY volume means full-session shares.
- OHLC are split-adjusted.
- Volume is split-consistent with the canonical price series.
- Prices are not dividend-adjusted.
- Required fields cannot be missing.
- Missing sessions are never fabricated automatically.
- Rows must be strictly chronological.
- Duplicate sessions are forbidden.
- Python handles provider/data-quality normalization.
- C++ independently enforces its domain contract.
- Parser failures stop processing rather than silently repairing data.

---

## 17. Post-MVP evolution

### Identity

```text
MVP:     symbol
Future:  permanent instrument_id + historical symbol mapping
```

### Time

```text
MVP:     exchange-local session_date
Future:  UTC event timestamps + venue/calendar interpretation
```

### Corporate actions

```text
MVP:     split-adjusted canonical OHLCV
Future:  raw execution prices + explicit corporate-action processing
```

### Analytical data

```text
MVP:     one canonical daily series
Future:  raw market data + versioned adjusted/total-return derived views
```

### Calendar validation

```text
MVP:     never fabricate missing sessions
Future:  venue calendars + holidays + early closes + session exceptions
```

### Data governance

```text
MVP:     files + deterministic transformations
Future:  instrument master + dataset/schema versions + lineage
         + quality issues + reconciliation + audit
```

These are future architectural evolutions and must not delay the MVP.

---

## 18. Contract summary

The MVP canonical record is:

```text
session_date,symbol,open,high,low,close,volume
```

with:

```text
one completed trading session
+
one MVP instrument identifier
+
split-adjusted, non-dividend-adjusted OHLC
+
split-consistent full-session share volume
+
strict deterministic validation
```

Provider complexity is resolved upstream. The C++ backtester receives a small, deterministic, explicitly defined market-data representation.
