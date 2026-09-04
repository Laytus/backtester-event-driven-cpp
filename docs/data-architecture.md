# Data Architecture

> **Purpose**
>
> This document defines the long-term data architecture of the Event-Driven Backtester project and its future evolution into a broader quantitative research and live-trading platform.
>
> It establishes:
>
> - the logical data model;
> - data ownership and lifecycle;
> - instrument identity;
> - source and provenance rules;
> - point-in-time semantics;
> - corporate-action handling;
> - data-quality and reconciliation principles;
> - dataset and schema versioning;
> - auditability and lineage;
> - monitoring and automation;
> - the subset implemented during the MVP;
> - the post-MVP implementation roadmap.
>
> This is a **living document**.
>
> The target architecture may evolve as implementation requirements become clearer, but the core principles should remain stable unless a concrete correctness or scalability issue justifies a change.

---

# 1. Architectural Objective

The data platform should eventually support:

- deterministic backtesting;
- quantitative research;
- historical universe reconstruction;
- point-in-time analysis;
- multi-source validation;
- corporate-action processing;
- reproducible datasets;
- live-data ingestion;
- production monitoring;
- auditability;
- future automated trading workflows.

The system should follow institutional data-management principles without attempting to reproduce the infrastructure scale of a large financial institution.

The objective is:

> Build a data architecture that remains trustworthy, explainable, reproducible, and extensible as the project evolves from a single-asset backtester into a quantitative research and live-trading system.

---

# 2. Core Data Principles

## 2.1 Permanent Internal Identity

Every financial instrument receives a stable internal identifier.

The internal identifier must not change when mutable attributes change, including:

- ticker;
- instrument name;
- exchange listing;
- external identifiers;
- corporate structure;
- vendor-specific codes.

Example:

```text
instrument_id = 12345

2020–2022:
symbol = FB

2022–present:
symbol = META
```

The internal identity remains:

```text
instrument_id = 12345
```

The internal identifier is the primary identity used throughout the data platform.

## 2.2 Historical Metadata Instead of Overwriting

Mutable identifiers and attributes must preserve history.

Do not overwrite:

```text
ticker = META
```

and lose the fact that the instrument previously traded as `FB`.

Instead store validity intervals:

```text
instrument_id
symbol
valid_from
valid_to
```

The same principle applies where relevant to:

- external identifiers;
- exchange listings;
- instrument names;
- sector classifications;
- index membership;
- vendor symbology.

## 2.3 Immutable Raw Data

Raw vendor data must be preserved as received whenever practical.

The raw layer should record:

- source;
- ingestion timestamp;
- request parameters;
- file/object identifier;
- checksum;
- source schema or version where known.

Raw data must not be silently modified after ingestion.

Corrections create new downstream versions instead of rewriting the original evidence.

## 2.4 Separate Raw, Canonical, and Derived Data

The architecture distinguishes:

```text
Raw Data
    ↓
Normalized / Validated Data
    ↓
Canonical Data
    ↓
Derived Data
```

### Raw

Provider-originated observations.

### Canonical

Provider-independent representation trusted by the platform.

### Derived

Values calculated from canonical data.

Examples:

- adjusted price series;
- return series;
- continuous futures;
- indicators;
- analytics.

Derived datasets must not replace canonical source data.

## 2.5 Data Provenance

Every trusted data record should be traceable to its source.

The platform should eventually be able to answer:

> Where did this value come from?

Conceptually:

```text
Backtest Result
        ↓
Dataset Version
        ↓
Canonical Record
        ↓
Transformation
        ↓
Ingestion Run
        ↓
Raw Vendor Record / File
        ↓
Vendor
```

## 2.6 Point-in-Time Correctness

Historical research must respect when information became available.

Where relevant, the platform should distinguish:

```text
event_time
effective_time
available_time
ingested_at
```

These timestamps answer different questions.

### Event Time

When the underlying event occurred.

### Effective Time

When the event became economically or legally effective.

### Available Time

When the information could reasonably have been known to the trading system.

### Ingested At

When the platform received or processed the information.

The backtester must never use information before its valid availability time.

## 2.7 Reproducibility

A backtest should eventually be reproducible from:

```text
code version
+ strategy configuration
+ dataset version
+ schema version
+ execution assumptions
```

Changing historical data must not silently change previously documented results.

## 2.8 Validation Before Trust

Incoming data are not trusted simply because they came from a vendor.

The path is:

```text
Incoming
    ↓
Validation
    ↓
Normalization
    ↓
Quality Checks
    ↓
Reconciliation
    ↓
Canonical / Trusted
```

Invalid or suspicious records should be rejected, quarantined, or flagged.

## 2.9 Flag Before Automatically Correcting

Suspicious values should normally be flagged before being altered.

For example, an extreme return may indicate:

- a vendor error;
- a stock split;
- an acquisition;
- a genuine market move;
- a stale previous observation.

The data-quality pipeline should not erase potentially valid information merely because it appears unusual.

## 2.10 Source Hierarchy

Multiple vendors should not be merged arbitrarily.

The system should eventually define:

```text
primary source
secondary validation source
fallback source
```

Cross-source disagreement should produce a reconciliation case rather than an arbitrary average.

---

# 3. Target Data Platform

The target platform can be understood as:

```text
                          DATA SOURCES
                 ┌────────────┼────────────┐
                 ▼            ▼            ▼
             Vendor A     Vendor B     Other Sources
                 │            │            │
                 └────────────┼────────────┘
                              ▼
                       INGESTION LAYER
                              │
                              ▼
                       IMMUTABLE RAW
                              │
                              ▼
                     SCHEMA VALIDATION
                              │
                       ┌──────┴──────┐
                       │             │
                      OK          INVALID
                       │             │
                       │        QUARANTINE
                       ▼
                     NORMALIZATION
                       │
                       ▼
                INSTRUMENT RESOLUTION
                       │
                       ▼
                   QUALITY CHECKS
                       │
                       ▼
              CROSS-SOURCE RECONCILIATION
                       │
                 ┌─────┴─────┐
                 │           │
                OK        FLAGGED
                 │           │
                 │      REVIEW WORKFLOW
                 ▼
                 CANONICAL DATA
                       │
                       ▼
                  DATASET VERSION
                       │
              ┌────────┼────────┐
              ▼        ▼        ▼
          Backtest  Research   Live Systems
```

Monitoring and audit operate across the entire pipeline.

---

# 4. Logical Data Domains

The long-term data model is separated into six logical domains:

```text
reference
market
raw
quality
metadata
audit
```

Each domain owns a distinct responsibility.

---

# 5. Reference Domain

The reference domain owns financial-instrument identity and slowly changing metadata.

Core entities include:

```text
instrument
instrument_identifier
symbol_history
listing
venue
currency
asset-specific details
```

---

# 6. Instrument Master

The universal master entity should be named `instrument`.

It is broader than a security master because the platform may eventually represent:

- equities;
- bonds;
- ETFs;
- futures;
- options;
- currencies;
- commodities;
- indices;
- other financial instruments.

Conceptual schema:

```text
instrument
----------
instrument_id
instrument_type
asset_class
currency_id
status
created_at
```

`instrument_id` is the permanent internal identity.

The exact identifier implementation is deferred.

Possible implementations include:

- integer sequence;
- UUID;
- sortable UUID;
- another stable opaque identifier.

The identifier should have no business meaning.

---

# 7. Instrument Identifiers

External identifiers belong in a separate historical table.

Conceptually:

```text
instrument_identifier
---------------------
instrument_id
identifier_type
identifier_value
valid_from
valid_to
source_id
```

Possible identifier types include:

- ISIN;
- CUSIP;
- FIGI;
- SEDOL;
- vendor-specific identifiers;
- internal legacy identifiers.

External identifiers are attributes.

They are not the primary internal identity.

---

# 8. Symbol History

Ticker symbols must be historical.

Conceptually:

```text
symbol_history
--------------
instrument_id
listing_id
symbol
valid_from
valid_to
source_id
```

This allows:

```text
instrument_id = 12345

FB      valid until ...
META    valid from ...
```

Historical data should preserve the symbol valid at the historical time where useful.

---

# 9. Listings

An instrument and a market listing are distinct concepts.

Conceptually:

```text
listing
-------
listing_id
instrument_id
venue_id
currency_id
valid_from
valid_to
status
```

This supports future cases where one instrument:

- trades on multiple venues;
- changes exchange;
- changes listing currency;
- has multiple share classes or related listings.

The exact modeling of these cases is deferred until needed.

---

# 10. Asset-Specific Tables

Do not place every possible asset-class field directly in `instrument`.

Use specialized extensions where needed.

Potential future tables:

```text
equity_details
bond_details
future_contract
option_contract
fx_pair
commodity_contract
index_details
```

Examples of specialized data:

## Bond

- coupon;
- maturity;
- face value;
- issue date;
- seniority;
- issuer.

## Future

- underlying;
- expiration;
- contract multiplier;
- tick size.

## Option

- underlying;
- strike;
- expiration;
- call/put;
- multiplier.

This avoids a universal table dominated by irrelevant nullable fields.

---

# 11. Venue and Calendar Reference Data

The platform should maintain reference data for trading venues.

Conceptual entities:

```text
venue
market_calendar
trading_session
session_exception
```

This data should eventually describe:

- timezone;
- regular sessions;
- holidays;
- early closes;
- late opens;
- special sessions;
- known market closures.

Calendars should be referenced by the data-quality pipeline and the backtester.

---

# 12. Market Domain

The market domain contains trusted provider-independent financial observations.

Potential long-term datasets include:

```text
bar
trade
quote
corporate_action
adjustment_factor
fundamental_observation
universe_membership
```

Only a small subset is required for the MVP.

---

# 13. Canonical Bar

The canonical daily `Bar` used by the MVP belongs to the market domain.

Conceptually:

```text
bar
---
instrument_id
timestamp
open
high
low
close
volume
```

The detailed contract is defined separately during Task 1.1.

Important principle:

> The canonical bar should not depend on vendor-specific field names or adjustment conventions.

---

# 14. Corporate Actions

Corporate actions should be modeled separately from price bars.

Potential event types include:

- stock split;
- reverse split;
- cash dividend;
- stock dividend;
- merger;
- acquisition;
- spin-off;
- symbol change;
- delisting;
- rights issue;
- other distributions.

Conceptually:

```text
corporate_action
----------------
corporate_action_id
instrument_id
action_type
announcement_time
effective_time
ex_date
record_date
payment_date
available_time
status
```

Not every action uses every date.

The exact schema will depend on the event type.

---

# 15. Corporate-Action Observations by Source

Vendor reports about corporate actions should remain distinct from the resolved canonical action.

Conceptually:

```text
Vendor A ─┐
Vendor B ─┼→ corporate_action_observation
Vendor C ─┘
                     ↓
                reconciliation
                     ↓
              corporate_action
```

Potential table:

```text
corporate_action_observation
----------------------------
observation_id
source_id
instrument_id
vendor_event_id
payload_reference
observed_at
```

This preserves vendor disagreement and enables audit.

---

# 16. Adjustment Factors and Adjusted Data

Adjusted prices should be treated as derived data.

Conceptually:

```text
Raw / Canonical Prices
        +
Corporate Actions
        ↓
Adjustment Logic
        ↓
Adjusted View / Dataset
```

Do not make adjusted prices the only stored representation.

Future adjustment datasets may include:

- split-adjusted prices;
- dividend-adjusted prices;
- total-return series.

The exact adjustment methodology must be versioned.

---

# 17. Historical Universe Membership

Future multi-asset research requires point-in-time universe membership.

Conceptually:

```text
universe
universe_membership
```

Example:

```text
universe_membership
-------------------
universe_id
instrument_id
valid_from
valid_to
available_time
```

This is required to avoid reconstructing historical universes using today's surviving securities.

The system must preserve:

- delisted securities;
- bankrupt securities;
- acquired securities;
- former index constituents.

---

# 18. Raw Domain

The raw domain preserves provider-originated evidence.

Core concepts:

```text
data_source
ingestion_run
raw_object
raw_record / staging
```

---

# 19. Data Sources

Each external origin receives a registered identity.

Conceptually:

```text
data_source
-----------
source_id
name
source_type
vendor
dataset_name
status
```

Examples:

```text
Yahoo Finance
Vendor A Corporate Actions
Vendor B Equity Daily Bars
Internal Manual Override
```

The rest of the platform should use `source_id` rather than hard-coded vendor names where practical.

---

# 20. Ingestion Runs

Every ingestion execution should eventually be identifiable.

Conceptually:

```text
ingestion_run
-------------
ingestion_run_id
source_id
started_at
completed_at
status
request_parameters
records_received
records_accepted
records_rejected
```

This provides operational traceability.

---

# 21. Raw Objects and Files

Bulk market data do not necessarily belong in relational tables.

The raw layer may eventually store:

```text
raw_object
----------
raw_object_id
source_id
ingestion_run_id
storage_uri
checksum
schema_hint
created_at
```

The raw payload may live in:

- filesystem;
- object storage;
- compressed vendor files;
- immutable archives.

Relational metadata records where the raw evidence lives.

---

# 22. Vendor-Specific Staging

Do not create one permanent canonical schema per vendor by default.

Vendor-specific staging is acceptable when necessary.

Example:

```text
vendor_a_daily_bar_staging
vendor_b_corporate_action_staging
```

These staging tables are adapters.

They must not become interfaces consumed directly by the backtester.

The canonical layer remains provider-independent.

---

# 23. Quality Domain

The quality domain records validation problems, discrepancies, and review workflows.

Core entities may include:

```text
quality_check
quality_issue
reconciliation_case
quarantine_record
```

---

# 24. Quality Checks

A quality check defines an automated validation rule.

Conceptually:

```text
quality_check
-------------
quality_check_id
name
category
severity_default
active
rule_version
```

Potential categories include:

- schema;
- timestamp;
- OHLC consistency;
- session completeness;
- duplicate detection;
- price anomaly;
- volume anomaly;
- corporate-action consistency;
- cross-source discrepancy.

---

# 25. Quality Issues

When a check fails or raises suspicion, create a quality issue.

Conceptually:

```text
quality_issue
-------------
issue_id
quality_check_id
dataset_version_id
instrument_id
event_time
severity
status
detected_at
resolved_at
resolution
```

Suggested statuses:

```text
OPEN
UNDER_REVIEW
CONFIRMED_ERROR
VALID_OBSERVATION
RESOLVED
IGNORED
```

The system should preserve the review history.

---

# 26. Reconciliation Cases

Cross-source discrepancies deserve explicit workflow.

Conceptually:

```text
reconciliation_case
-------------------
case_id
instrument_id
event_time
field
primary_value
secondary_value
status
resolution
resolved_at
```

The resolution may identify:

- primary source confirmed;
- secondary source confirmed;
- corporate action explains difference;
- manual override;
- unresolved discrepancy.

---

# 27. Quarantine

Data that fail critical validation should not enter trusted canonical datasets automatically.

Conceptually:

```text
Incoming Data
        ↓
Critical Validation Failure
        ↓
Quarantine
        ↓
Review / Reprocessing
```

Quarantine preserves the rejected evidence rather than deleting it.

---

# 28. Metadata Domain

The metadata domain supports reproducibility and versioning.

Core entities:

```text
dataset
dataset_version
schema
schema_version
lineage
transformation
```

---

# 29. Dataset

A dataset identifies a logical body of data.

Examples:

```text
SPY_DAILY_BARS
US_EQUITY_DAILY_BARS
US_CORPORATE_ACTIONS
SP500_MEMBERSHIP
```

Conceptually:

```text
dataset
-------
dataset_id
name
dataset_type
description
owner
status
```

---

# 30. Dataset Version

A dataset version identifies an immutable or logically frozen state of a dataset.

Conceptually:

```text
dataset_version
---------------
dataset_version_id
dataset_id
version
created_at
schema_version_id
parent_version_id
status
checksum / manifest
```

A backtest should reference a specific dataset version.

Example:

```text
code_commit       = a1b2c3
dataset_version   = SPY_DAILY_V17
schema_version    = BAR_V2
strategy_config   = ...
```

---

# 31. Schema Versioning

Schemas should evolve explicitly.

Conceptually:

```text
schema
    ↓
schema_version
```

Potential table:

```text
schema_version
--------------
schema_version_id
schema_id
version
definition
created_at
status
```

A change to:

- required fields;
- field semantics;
- timestamp convention;
- adjustment convention;
- units;

should produce an explicit schema version where appropriate.

---

# 32. Transformations

Transformations should be identifiable.

Examples:

```text
vendor normalization
corporate-action adjustment
currency conversion
resampling
data correction
```

Conceptually:

```text
transformation
--------------
transformation_id
name
version
code_reference
configuration
```

This allows dataset lineage to describe how one version was produced from another.

---

# 33. Data Lineage

Lineage links sources, transformations, and output datasets.

Conceptually:

```text
source raw object
        ↓
transformation
        ↓
canonical dataset version
        ↓
derived dataset version
```

The long-term platform should make this path queryable.

---

# 34. Audit Domain

Market history and internal audit history are different concepts.

Do not mix:

```text
market events
```

with:

```text
system actions
```

The audit domain records what the platform itself did.

Potential entities:

```text
audit_event
pipeline_run
manual_override
```

---

# 35. Audit Events

Conceptually:

```text
audit_event
-----------
audit_event_id
event_type
actor
entity_type
entity_id
timestamp
details
```

Examples:

- dataset promoted to trusted;
- data correction approved;
- source hierarchy changed;
- reconciliation case resolved;
- schema version activated.

---

# 36. Manual Overrides

Manual changes must be exceptional and fully auditable.

Conceptually:

```text
manual_override
---------------
override_id
target_type
target_id
previous_value
new_value
reason
actor
created_at
approved_by
```

A manual correction should never silently overwrite history.

---

# 37. Pipeline Runs

Automated pipeline executions should eventually be monitored as first-class operational entities.

Conceptually:

```text
pipeline_run
------------
pipeline_run_id
pipeline_name
started_at
completed_at
status
input_versions
output_versions
error_summary
```

This is distinct from vendor ingestion when a pipeline performs downstream processing.

---

# 38. Automated Data-Quality Pipeline

The long-term processing flow should resemble:

```text
INGEST
  ↓
STORE RAW
  ↓
SCHEMA VALIDATION
  ↓
NORMALIZATION
  ↓
INSTRUMENT RESOLUTION
  ↓
TEMPORAL VALIDATION
  ↓
QUALITY CHECKS
  ↓
CROSS-SOURCE RECONCILIATION
  ↓
CORPORATE-ACTION VALIDATION
  ↓
CANONICAL DATA
  ↓
DATASET VERSION
  ↓
PUBLISH
```

Critical failures go to quarantine.

Non-critical anomalies create quality issues.

---

# 39. Data-Quality Rules

The platform should eventually automate at least the following classes of checks.

## Schema

- required fields;
- valid types;
- valid enumerations;
- parseable timestamps.

## Time

- chronological ordering;
- duplicate timestamps;
- expected sessions;
- missing sessions;
- unexpected records outside trading sessions.

## OHLC

- `high >= open`;
- `high >= close`;
- `low <= open`;
- `low <= close`;
- `high >= low`;
- finite values;
- valid price domain.

## Volume

- non-negative values;
- suspicious discontinuities;
- missing data where volume is expected.

## Returns

- extreme moves;
- discontinuities;
- stale prices;
- possible unprocessed corporate actions.

## Cross-Source

- price discrepancies;
- volume discrepancies;
- missing observations;
- metadata conflicts;
- corporate-action conflicts.

---

# 40. Monitoring

The long-term system should monitor both data quality and pipeline health.

Potential metrics:

```text
freshness
last successful ingestion
records received
records accepted
records rejected
missing instruments
missing sessions
open quality issues
reconciliation backlog
pipeline duration
vendor latency
schema failures
dataset publication status
```

Monitoring should eventually generate alerts when thresholds are violated.

---

# 41. Storage Architecture

The project should not assume that all data belong in one relational database.

A likely future architecture is:

```text
PostgreSQL
    → instrument master
    → reference data
    → metadata
    → lineage
    → quality
    → audit
    → operational state

Parquet / Object Storage
    → bulk historical market data
    → raw vendor archives
    → canonical large datasets
    → derived research datasets

DuckDB
    → local analytical queries
    → research over Parquet

Future Streaming Infrastructure
    → live market data
    → real-time ingestion
```

This is a target direction, not an MVP requirement.

---

# 42. Why Not One Table Per Vendor

Creating permanent application-facing tables such as:

```text
yfinance_prices
vendor_a_prices
vendor_b_prices
```

and allowing downstream code to consume them directly would couple the system to vendor schemas.

Instead:

```text
Vendor-Specific Raw / Staging
        ↓
Adapter / Normalizer
        ↓
Canonical Model
```

Vendor-specific storage remains acceptable inside the ingestion boundary.

The rest of the platform should depend on canonical contracts.

---

# 43. Why Not One Database for Everything

Different workloads have different storage requirements.

Reference data require:

- constraints;
- joins;
- historical metadata;
- transactional updates.

Historical bars may require:

- columnar storage;
- compression;
- large scans;
- partitioning.

Live data may require:

- low-latency streaming;
- in-memory state;
- append-heavy processing.

The architecture should allow these workloads to use appropriate technologies while preserving common identifiers and schemas.

---

# 44. Research and Live Trading Semantics

Historical and live systems should eventually share common domain semantics.

They should agree on:

- `instrument_id`;
- symbol resolution;
- venue identity;
- corporate-action definitions;
- market calendars;
- schema semantics;
- units;
- timestamp conventions.

The delivery mechanism differs:

```text
Research
→ historical dataset replay

Live
→ streaming observations
```

but the financial meaning should remain consistent.

---

# 45. Biases Addressed by the Data Architecture

A strong data architecture helps mitigate several important backtesting risks.

## Look-Ahead Bias

Controlled through point-in-time availability and temporal contracts.

## Survivorship Bias

Controlled through historical instrument and universe membership data.

## Corporate-Action Errors

Controlled through explicit corporate-action datasets and versioned adjustment logic.

## Symbol Bias

Controlled through permanent internal identifiers and historical symbology.

## Vendor Revision Risk

Controlled through immutable raw data and dataset versioning.

## Data-Snooping Through Silent Corrections

Reduced by lineage, versioning, and explicit correction history.

The architecture reduces these risks.

It does not replace proper research methodology.

---

# 46. MVP Implementation

The MVP deliberately implements only the smallest useful subset of this architecture.

The MVP data flow is:

```text
yfinance
    ↓
raw provider file
    ↓
Python validation / normalization
    ↓
canonical daily OHLCV CSV
    ↓
C++ CsvBarReader
    ↓
HistoricalDataFeed
    ↓
BacktestEngine
```

No relational database is required for the MVP.

---

# 47. MVP Raw Layer

For the MVP:

```text
data/raw/
```

stores provider-originated files.

Raw files should be preserved when generated locally.

Large raw datasets are not committed to Git.

The raw file should be sufficient to reproduce the normalization step where practical.

---

# 48. MVP Canonical Layer

For the MVP:

```text
data/processed/
```

may hold provider-independent canonical datasets.

The canonical daily-bar schema is defined in Task 1.1.

The C++ engine consumes only the canonical format.

It must not understand yfinance-specific fields.

---

# 49. MVP Instrument Identity

The MVP contains one instrument:

```text
SPY
```

The implementation does not require a complete instrument-master database.

However, Task 1.1 must avoid making the ticker itself the permanent conceptual identity of the entire future system.

Possible MVP approaches include:

- use a simple symbol field while documenting that future canonical storage will use `instrument_id`;
- introduce a lightweight internal identifier immediately if it improves clarity without increasing scope.

The final choice belongs to Task 1.1.

---

# 50. MVP Corporate Actions

The MVP does not implement a corporate-action database or adjustment engine.

However, the acquisition pipeline must explicitly document:

- whether input prices are raw or adjusted;
- how yfinance adjustment behavior is configured;
- whether splits or dividends affect the canonical series;
- what assumptions the backtest makes.

The canonical bar contract must not hide vendor-specific adjustment behavior.

---

# 51. MVP Calendar

The MVP does not require a full exchange-calendar database.

The canonical dataset should contain actual observed trading sessions.

Data validation should detect:

- duplicate dates;
- non-monotonic dates;
- malformed timestamps.

Full holiday and session-exception validation is deferred.

---

# 52. MVP Quality Pipeline

The MVP implements deterministic validation sufficient for the current daily SPY dataset.

At minimum:

```text
required columns
valid timestamps
chronological ordering
duplicate timestamps
missing required values
finite prices
OHLC consistency
non-negative volume
```

Suspicious anomalies may be logged or surfaced rather than automatically corrected.

---

# 53. MVP Dataset Versioning

The MVP does not require a dataset-version database.

However, reproducibility should be preserved through:

- raw-data traceability;
- canonical-file naming;
- code commit;
- configuration;
- documented source;
- deterministic transformations.

A lightweight manifest may be introduced if it materially improves reproducibility.

---

# 54. MVP Testing Data

Small deterministic fixtures belong in:

```text
data/fixtures/
```

or the relevant test fixture directory.

Fixtures are version-controlled.

They are separate from real provider datasets.

The manual-oracle dataset must remain small and human-verifiable.

---

# 55. Post-MVP Roadmap

The long-term data platform should be implemented incrementally.

---

# 56. Post-MVP Stage I — Reference and Metadata Foundation

Introduce a relational database, likely PostgreSQL.

Initial entities:

```text
instrument
instrument_identifier
symbol_history
listing
venue
currency
data_source
ingestion_run
dataset
dataset_version
schema
schema_version
```

Primary objectives:

- permanent identity;
- source registration;
- metadata;
- reproducibility;
- historical symbology.

---

# 57. Post-MVP Stage II — Data Quality Platform

Add:

```text
quality_check
quality_issue
quarantine
pipeline_run
audit_event
manual_override
lineage
```

Implement:

- automated validation;
- issue tracking;
- correction audit;
- pipeline observability;
- dataset publication workflow.

---

# 58. Post-MVP Stage III — Corporate Actions and Multi-Source Reconciliation

Add:

```text
corporate_action
corporate_action_observation
adjustment_factor
reconciliation_case
```

Introduce:

- multiple data sources;
- source hierarchy;
- cross-source comparison;
- split/dividend handling;
- canonical corporate actions;
- versioned adjusted datasets.

---

# 59. Post-MVP Stage IV — Historical Universe and Survivorship Control

Add:

```text
universe
universe_membership
delisting history
instrument status history
```

Objectives:

- historical index reconstruction;
- delisted securities;
- survivorship-bias control;
- point-in-time asset universes.

---

# 60. Post-MVP Stage V — Scalable Research Storage

Introduce:

```text
Parquet
object storage
DuckDB
partitioned historical datasets
```

Objectives:

- larger universes;
- longer histories;
- intraday data;
- efficient local quantitative research;
- separation of metadata from bulk time-series storage.

---

# 61. Post-MVP Stage VI — Automated Daily Data Platform

Automate:

```text
scheduled ingestion
validation
normalization
reconciliation
quarantine
dataset publication
monitoring
alerting
```

The system should be capable of updating trusted datasets without manual intervention under normal conditions.

---

# 62. Post-MVP Stage VII — Live Data Integration

Add infrastructure for:

```text
live market-data ingestion
stream normalization
instrument resolution
market-status handling
real-time quality monitoring
broker/live interfaces
```

Historical and live paths should reuse the same instrument identities and domain semantics.

---

# 63. Decisions Frozen Now

The following architectural decisions are intentionally established before Task 1.1:

- internal instrument identity must eventually be permanent and independent of ticker;
- mutable identifiers must preserve history;
- raw data and canonical data are separate;
- raw data are preserved rather than overwritten;
- provider-specific data do not define the core domain model;
- corporate actions are separate from bars;
- adjusted prices are derived rather than the only stored truth;
- point-in-time availability matters;
- delisted instruments and historical universes must eventually be preserved;
- multiple sources use explicit source hierarchy and reconciliation;
- quality issues are recorded rather than silently erased;
- datasets and schemas are versioned;
- lineage and audit are first-class long-term concerns;
- bulk historical market data are not assumed to belong in PostgreSQL;
- research and live systems should share identity and schema semantics.

---

# 64. Decisions Deferred

The following decisions should not be frozen yet:

- exact SQL schema;
- exact database technology beyond the likely PostgreSQL metadata role;
- exact internal ID type;
- exact Parquet partitioning;
- exact object-storage technology;
- exact live streaming platform;
- exact vendor hierarchy;
- exact corporate-action adjustment methodology;
- exact retention policy;
- exact reconciliation thresholds;
- exact data-quality severity framework.

These should be resolved when implementation requirements justify them.

---

# 65. Immediate Next Step

With the long-term data architecture defined, Phase 1 can proceed to:

```text
Task 1.1 — Define Canonical Daily Bar Contract
```

Task 1.1 must explicitly decide:

- canonical fields;
- C++ types;
- timestamp/date semantics;
- price representation;
- volume representation;
- instrument identity representation for the MVP;
- canonical CSV schema;
- raw versus adjusted price convention;
- Python versus C++ validation responsibilities;
- required invariants.

The contract should fit the MVP while remaining compatible with this broader architecture.

---

# 66. Final Data Principle

The project should treat data as a versioned, traceable, time-aware system rather than as a collection of CSV files.

The long-term philosophy is:

```text
Permanent Identity
+
Immutable Raw Evidence
+
Historical Metadata
+
Point-in-Time Correctness
+
Explicit Corporate Actions
+
Validation and Reconciliation
+
Versioning
+
Lineage
+
Auditability
+
Automation
```

The MVP implements only a small subset.

The architecture ensures that this small subset can evolve into a professional quantitative data platform without forcing the backtester to depend on vendor-specific or historically incorrect assumptions.
