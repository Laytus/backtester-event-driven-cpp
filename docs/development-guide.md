# Development Guide

> **Purpose**
>
> This document is the operational guide for implementing the Event-Driven Backtester MVP.
> It defines the development phases, the order of work, the expected deliverables,
> the validation required in each phase, and the criteria for moving forward.
>
> It does **not** define the MVP scope, bibliography, or permanent project principles.
> Those responsibilities belong to their dedicated documents.

---

# 1. Authoritative Project Documents

The development process is governed by the following documents:

| Topic | Authoritative Document |
|---|---|
| Permanent development principles | `project-principles.md` |
| MVP inclusions, exclusions, assumptions, and Definition of Done | `MVP-scope.md` |
| Bibliography and phase reading requirements | `references.md` |
| Conceptual map of the project | `knowledge-guide.md` |
| Technical architecture | `architecture.md` |
| Operational implementation plan | `development-guide.md` |

If two documents conflict, the document that owns the topic takes precedence.

This guide must remain synchronized with the others, but should avoid duplicating their full contents.

---

# 2. Development Mode

The MVP is implemented under an execution-first workflow.

The objective is not to complete study material before coding.

The objective is to:

```text
resolve the next implementation question
        ↓
make the smallest necessary design decision
        ↓
implement
        ↓
validate deterministically
        ↓
document only what matters
        ↓
commit
```

Development should remain focused on producing a correct, explainable, portfolio-ready MVP as efficiently as possible.

---

# 3. Working Rules

The following rules apply throughout all phases.

## 3.1 Stay Inside MVP Scope

Before adding behavior, ask:

> Is this required by `MVP-scope.md`?

If not, defer it unless it is required for:

- correctness;
- deterministic validation;
- reproducibility;
- resolving a serious architectural flaw.

Post-MVP ideas should be recorded without being implemented.

---

## 3.2 Read on Demand

The required bibliography is defined in `references.md`.

Do not repeat theory that is already understood.

Read only when a concrete implementation or design question requires clarification.

The normal pattern is:

```text
Implementation question
→ consult minimum relevant source
→ resolve question
→ return to code
```

---

## 3.3 Keep Design Small

Before implementing a component, define only:

- responsibility;
- inputs;
- outputs;
- owned state;
- invariants;
- dependencies;
- important failure modes.

Do not design hypothetical post-MVP behavior unless a clean boundary genuinely depends on it.

---

## 3.4 Implement in Stable Increments

Prefer small increments that:

- compile;
- have a clear purpose;
- include the relevant tests;
- preserve deterministic behavior;
- can be committed independently.

Avoid leaving multiple unfinished subsystems open simultaneously.

---

## 3.5 Validate Before Expanding

A component is not complete because it compiles.

Before building on top of it, validate the behavior that downstream components will rely on.

Use deterministic fixtures whenever possible.

---

## 3.6 Keep Documentation Lightweight

During the MVP, documentation should capture only information that improves:

- correctness;
- reproducibility;
- maintainability;
- interview explainability.

Do not create documentation as a substitute for implementation.

---

# 4. Standard Task Workflow

Every meaningful implementation task follows this sequence.

## Step 1 — Define the Task

State:

- what is being implemented;
- why it is needed now;
- what is explicitly outside the task.

The task should be small enough to complete and validate coherently.

---

## Step 2 — Resolve Required Knowledge

Identify only the concepts necessary for the task.

If additional reading is needed, use `references.md`.

Do not expand the reading scope unless the implementation reveals a concrete need.

---

## Step 3 — Define the Contract

Before coding, determine:

- inputs;
- outputs;
- state;
- invariants;
- errors;
- component boundaries.

For financial components, also state the financial meaning and timing assumptions.

---

## Step 4 — Define Validation First

Before implementation, know what evidence will demonstrate correctness.

Possible validation includes:

- exact parser expectations;
- deterministic event sequences;
- manually calculated accounting states;
- known-result metric calculations;
- expected failure cases.

---

## Step 5 — Implement the Smallest Correct Version

Implement only the behavior required by the current task.

Do not add speculative flexibility.

---

## Step 6 — Test

Add or update tests for:

- important behavior;
- state transitions;
- quantitative calculations;
- failure modes;
- invariants.

Tests should focus on observable behavior rather than implementation details.

---

## Step 7 — Integrate

Verify that the new component works with the already completed system.

Avoid postponing integration until the end of a phase when an earlier integration test is possible.

---

## Step 8 — Document Decisions

Update documentation only when the implementation introduces:

- an important assumption;
- a durable design decision;
- an architectural change;
- a known limitation;
- a reproducibility requirement.

---

## Step 9 — Commit

Create a small coherent commit once the task is stable.

The repository should remain in a usable state after each meaningful milestone.

---

# 5. Phase Structure

The MVP is divided into six phases:

```text
Phase 0 — Project Foundation
Phase 1 — Market Data Infrastructure
Phase 2 — Event-Driven Engine
Phase 3 — Trading and Portfolio Layer
Phase 4 — Strategy and Performance
Phase 5 — MVP Integration
```

The phases define dependency order, not a time schedule.

A phase may contain multiple small implementation tasks and commits.

---

# 6. Phase 0 — Project Foundation

## Purpose

Freeze the minimum project definition required to begin implementation without ambiguity.

This phase is primarily architectural and documentary.

---

## Deliverables

- [ ] `MVP-scope.md`
- [ ] `project-principles.md`
- [ ] `references.md`
- [ ] `knowledge-guide.md`
- [ ] initial `architecture.md`
- [ ] repository/build baseline verified
- [ ] MVP temporal model confirmed
- [ ] core component responsibilities confirmed

---

## Questions That Must Be Settled

Before Phase 1 starts, the project must have explicit answers to:

- What exactly is included in the MVP?
- What is explicitly excluded?
- What is the complete trading-event lifecycle?
- What information does the strategy observe?
- When is a signal generated?
- When is an order submitted?
- When and at what assumed price does the order fill?
- Which component owns cash and positions?
- Which component owns execution assumptions?
- How is future-data access prevented?
- Which components are required for the first end-to-end backtest?

---

## Validation

Phase 0 requires conceptual consistency rather than runtime validation.

Verify that:

- the scope and architecture do not contradict each other;
- terminology is consistent across documents;
- the event lifecycle is unambiguous;
- the temporal model is unambiguous;
- the bibliography is aligned with the actual implementation phases;
- no required MVP component is missing from the architecture;
- no excluded post-MVP feature is accidentally required by the design.

---

## Exit Criteria

Phase 0 is complete when:

- [ ] all core project documents exist;
- [ ] the MVP scope is frozen;
- [ ] the bibliography is frozen for the MVP;
- [ ] the baseline architecture is documented;
- [ ] the system lifecycle can be explained without ambiguity;
- [ ] implementation can begin without additional project-planning work.

Once these conditions are satisfied, return to Phase 0 only if implementation exposes a genuine contradiction or missing architectural decision.

---

# 7. Phase 1 — Market Data Infrastructure

## Purpose

Build the historical-data path required by the simulation.

The result of this phase must be a validated, provider-independent sequence of daily bars that C++ can consume deterministically.

---

## Required Reading

Use the Phase 1 section of `references.md`.

Do not duplicate the reading plan here.

---

## Core Deliverables

- [ ] canonical daily-bar schema
- [ ] raw-data acquisition workflow
- [ ] provider-specific normalization
- [ ] canonical CSV dataset
- [ ] data validation
- [ ] C++ `Bar` domain type
- [ ] CSV reader/parser
- [ ] `HistoricalDataFeed`
- [ ] deterministic parser fixtures
- [ ] sequential-delivery tests

---

## Recommended Implementation Order

### 1. Define the Canonical Bar Contract

Define:

- timestamp representation;
- open;
- high;
- low;
- close;
- volume;
- canonical CSV column order and format.

The core C++ domain must remain provider-independent.

---

### 2. Build the Python Acquisition and Normalization Path

Implement only what is needed to:

```text
provider data
→ raw file
→ validation
→ normalized canonical CSV
```

Raw data and canonical data must remain distinguishable.

---

### 3. Implement Data Validation

Validate at least:

- required fields;
- timestamp parsing;
- chronological ordering;
- duplicate timestamps;
- missing values;
- obviously invalid OHLCV relationships where appropriate.

Do not silently repair ambiguous data.

---

### 4. Implement the C++ `Bar`

The type should represent one canonical daily observation.

Keep it small and explicit.

---

### 5. Implement the CSV Reader

The parser must:

- read the canonical format;
- reject malformed input;
- preserve chronological semantics;
- expose explicit errors.

Provider-specific parsing does not belong here.

---

### 6. Implement `HistoricalDataFeed`

The feed must:

- own or control historical-bar delivery;
- expose bars sequentially;
- prevent arbitrary access to future observations;
- produce deterministic behavior.

The exact interface belongs in `architecture.md`.

---

## Required Validation

### Parser

Test:

- valid rows;
- malformed numbers;
- missing columns;
- invalid timestamps;
- missing values;
- duplicate timestamps;
- non-monotonic timestamps.

### Data Feed

Test:

- first bar;
- sequential progression;
- end-of-data behavior;
- correct chronological order;
- no duplicate delivery;
- no future-bar access through the public interface.

---

## Phase Integration Test

Using a very small fixture:

```text
canonical CSV
→ parser
→ HistoricalDataFeed
→ bars delivered in exact expected order
```

The fixture should be small enough to inspect manually.

---

## Exit Criteria

Phase 1 is complete when:

- [ ] canonical data can be produced reproducibly;
- [ ] malformed canonical data is rejected appropriately;
- [ ] C++ parses valid canonical data correctly;
- [ ] the feed exposes historical bars sequentially;
- [ ] future observations are inaccessible through normal use;
- [ ] deterministic tests pass;
- [ ] the component contracts are reflected in `architecture.md`.

---

# 8. Phase 2 — Event-Driven Engine

## Purpose

Implement the deterministic event protocol and simulation loop.

At the end of the phase, historical bars must enter the engine as `MarketEvent`s and be processed in exact chronological order.

---

## Required Reading

Use the Phase 2 section of `references.md`.

---

## Core Deliverables

- [ ] explicit event model
- [ ] `MarketEvent`
- [ ] `SignalEvent`
- [ ] `OrderEvent`
- [ ] `FillEvent`
- [ ] event queue
- [ ] deterministic event dispatch
- [ ] `BacktestEngine`
- [ ] integration with `HistoricalDataFeed`

Not every event needs full business behavior yet.

The objective is to establish the protocol and engine structure required by later phases.

---

## Recommended Implementation Order

### 1. Define Event Semantics

For every event type, define:

- who produces it;
- who consumes it;
- what information it carries;
- what timestamp it represents.

Avoid ambiguous timestamps.

---

### 2. Choose the C++ Event Representation

Use the simplest representation that provides:

- explicit event types;
- type-safe dispatch;
- deterministic behavior;
- understandable ownership.

Do not reproduce QuantStart's Python inheritance hierarchy automatically.

---

### 3. Implement the Event Queue

The queue must have explicit deterministic ordering.

Concurrency is not required.

---

### 4. Implement the Engine Loop

The engine coordinates:

- requesting the next available market observation;
- creating or receiving `MarketEvent`s;
- processing queued events;
- dispatching them to the appropriate component.

At this phase, downstream trading behavior may still be minimal or stubbed where necessary.

---

## Required Validation

Test:

- event creation;
- event type preservation;
- event ordering;
- queue behavior;
- one `MarketEvent` per delivered bar;
- no duplicate events;
- chronological processing;
- deterministic repeated runs;
- clean termination at end of data.

---

## Phase Integration Test

Using a small historical fixture:

```text
HistoricalDataFeed
→ MarketEvent
→ Event Queue
→ BacktestEngine
→ deterministic observed event sequence
```

The exact expected event sequence must be asserted.

---

## Exit Criteria

Phase 2 is complete when:

- [ ] all MVP event types have explicit semantics;
- [ ] the queue behaves deterministically;
- [ ] the engine consumes market data chronologically;
- [ ] each bar produces the expected market event;
- [ ] repeated runs produce the same event sequence;
- [ ] engine behavior is covered by deterministic tests;
- [ ] architecture documentation matches the implementation.

---

# 9. Phase 3 — Trading and Portfolio Layer

## Purpose

Produce the first complete end-to-end backtest.

This is the most important functional milestone of the MVP.

The target lifecycle is:

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

---

## Required Reading

Use the Phase 3 section of `references.md`.

---

## Core Deliverables

- [ ] strategy contract
- [ ] Buy-and-Hold strategy
- [ ] signal semantics
- [ ] deterministic position-sizing policy
- [ ] order generation
- [ ] position representation
- [ ] portfolio accounting
- [ ] execution-handler contract
- [ ] next-available-open execution policy
- [ ] fill processing
- [ ] portfolio valuation history
- [ ] manual deterministic oracle
- [ ] complete end-to-end test

---

## Recommended Implementation Order

### 1. Strategy Contract

Define how a strategy:

- receives observable market information;
- owns strategy-specific state;
- emits trading intent.

The strategy must not mutate the portfolio.

---

### 2. Implement Buy-and-Hold

Use Buy-and-Hold as the first strategy because it minimizes strategy complexity while exercising the complete event flow.

Do not introduce the moving-average strategy yet.

---

### 3. Define Signal Semantics

The MVP requires at least:

- `LONG`;
- `EXIT`.

Signals express intent only.

They must not contain hidden execution behavior.

---

### 4. Define Position Sizing

Implement a simple deterministic policy sufficient to convert a valid signal into an order quantity.

The policy must be explicit and reproducible.

Do not implement advanced sizing.

---

### 5. Implement Order Generation

Orders must explicitly carry the information required by `MVP-scope.md`.

Signal-to-order conversion belongs outside the strategy.

---

### 6. Implement Portfolio State

The portfolio owns:

- cash;
- position quantity;
- market value;
- total equity;
- valuation history.

State transitions must be easy to inspect and test.

---

### 7. Implement Execution

Implement the fixed MVP rule:

> An order generated after a completed daily bar fills at the next available daily open.

The execution layer determines the fill timing and simulated execution price.

The strategy does not.

---

### 8. Implement Fill Processing

A fill is the event that changes holdings and cash.

Portfolio accounting after a fill must be deterministic and testable.

---

### 9. Build the Manual Oracle

Create a tiny artificial dataset with manually calculated expected values for:

- bars;
- signal timing;
- order timing;
- fill timing;
- fill price;
- quantity;
- cash;
- position;
- market value;
- equity.

This oracle becomes the most important integration fixture in the project.

---

## Required Validation

Test independently:

### Strategy

- initial state;
- correct signal creation;
- no unintended repeated entry signals.

### Position Sizing

- deterministic quantity;
- edge conditions required by the MVP.

### Portfolio

- initial capital;
- fill-driven quantity updates;
- cash changes;
- market value;
- equity.

### Execution

- order is not filled on the signal bar;
- fill occurs at the next available open;
- correct fill timestamp;
- correct fill price;
- deterministic full fill.

### End-to-End

The complete backtest must reproduce the manual oracle exactly.

---

## Exit Criteria

Phase 3 is complete when:

- [ ] Buy-and-Hold runs through the complete event lifecycle;
- [ ] signal, order, and fill remain separate concepts;
- [ ] the next-open execution rule is enforced;
- [ ] cash and position accounting are correct;
- [ ] portfolio equity matches the manual oracle;
- [ ] the full deterministic end-to-end test passes;
- [ ] the complete lifecycle can be explained clearly.

At this point, the project has a functioning event-driven backtesting engine.

---

# 10. Phase 4 — Strategy and Performance

## Purpose

Add the second MVP strategy and validated quantitative performance analysis.

This phase should extend the functioning engine rather than change its core architecture.

---

## Required Reading

Use the Phase 4 section of `references.md`.

---

## Core Deliverables

- [ ] moving-average calculation
- [ ] Moving-Average Crossover strategy
- [ ] configurable short and long windows
- [ ] return series
- [ ] total return
- [ ] annualized return
- [ ] annualized volatility
- [ ] Sharpe ratio
- [ ] maximum drawdown
- [ ] benchmark comparison
- [ ] deterministic metric tests

---

## Recommended Implementation Order

### 1. Moving-Average Primitive

Implement and validate the rolling calculation required by the strategy.

Do not build a generic technical-analysis framework.

---

### 2. Moving-Average Crossover Strategy

Implement the smallest strategy that satisfies the MVP contract.

Explicitly define:

- warm-up behavior;
- crossover condition;
- long-entry condition;
- exit condition;
- behavior when already long or already flat.

Parameters must be configurable.

Optimization is outside the MVP.

---

### 3. Build the Return Series

Define the exact return convention before calculating higher-level metrics.

Document the convention if it materially affects interpretation.

---

### 4. Implement Metrics Individually

Implement and validate each metric separately:

- total return;
- annualized return;
- annualized volatility;
- Sharpe ratio;
- maximum drawdown.

Do not implement a large analytics framework.

---

### 5. Implement Benchmark Comparison

Use the MVP benchmark defined in the project scope.

Benchmark calculation must use compatible data and conventions.

---

## Required Validation

### Strategy

Use a synthetic price sequence where moving-average behavior can be calculated manually.

Verify:

- warm-up;
- crossover;
- entry;
- exit;
- no duplicate unintended transitions.

### Metrics

Each metric must have a known-result deterministic test.

Test edge cases that are relevant to the chosen formula conventions.

### Integration

Verify that both MVP strategies can run through the same engine without changing core engine behavior.

---

## Exit Criteria

Phase 4 is complete when:

- [ ] Moving-Average Crossover runs end to end;
- [ ] strategy signals are validated;
- [ ] all required metrics are implemented;
- [ ] metric conventions are explicit;
- [ ] all quantitative calculations have known-result tests;
- [ ] benchmark comparison works;
- [ ] both strategies use the same core engine and accounting path.

---

# 11. Phase 5 — MVP Integration

## Purpose

Turn the functioning engine into a reproducible, portfolio-ready repository.

This phase should focus on integration, usability, verification, and presentation.

It should not introduce new trading functionality.

---

## Required Reading

No new finance reading is required.

Use technical documentation only when necessary, as defined in `references.md`.

---

## Core Deliverables

- [ ] minimal CLI
- [ ] reproducible configuration
- [ ] result CSV export
- [ ] equity-curve export
- [ ] performance summary export
- [ ] Python visualization
- [ ] benchmark visualization where useful
- [ ] clean Debug build
- [ ] clean Release build
- [ ] complete automated test suite
- [ ] final architecture update
- [ ] portfolio-quality README
- [ ] documented example run
- [ ] final reproducibility check

---

## Recommended Implementation Order

### 1. Minimal CLI and Configuration

Support only the configuration required by the MVP.

Avoid building a general configuration framework.

---

### 2. Result Export

Export the data required to:

- inspect a backtest;
- reproduce metrics;
- generate plots;
- document example results.

Keep output formats simple.

---

### 3. Python Visualization

Generate the minimum useful visual output.

At least:

- equity curve;
- benchmark comparison where applicable.

Do not build an interactive dashboard.

---

### 4. Final Build and Test Verification

Verify:

- clean Debug build;
- clean Release build;
- required warnings;
- complete test execution;
- deterministic end-to-end behavior.

---

### 5. Reproducibility Pass

Perform the project workflow as if using a fresh environment.

Verify that documentation contains everything needed to:

```text
obtain data
→ normalize data
→ configure
→ build
→ test
→ run
→ export
→ plot
```

---

### 6. README and Portfolio Presentation

The README should explain clearly:

- what the project is;
- why it is event-driven;
- architecture;
- event lifecycle;
- temporal model;
- execution assumptions;
- strategies;
- performance metrics;
- how to build;
- how to test;
- how to run;
- example outputs;
- limitations;
- post-MVP roadmap.

The README should demonstrate engineering judgment rather than oversell strategy performance.

---

## Exit Criteria

Phase 5 is complete when the full Definition of Done in `MVP-scope.md` is satisfied.

This guide should not duplicate that checklist.

The final verification must include:

- [ ] build reproducibility;
- [ ] test reproducibility;
- [ ] data reproducibility;
- [ ] backtest reproducibility;
- [ ] output reproducibility;
- [ ] documentation completeness;
- [ ] architecture consistency;
- [ ] interview explainability.

---

# 12. Phase Transition Rule

Phases represent dependency order.

Do not begin downstream work that depends on unvalidated upstream behavior.

However, the process should remain practical:

- documentation cleanup may occur alongside implementation;
- a small downstream prototype may be used to expose an interface problem;
- non-blocking polish does not need to delay the next phase.

The rule is:

> Do not build important downstream behavior on top of upstream behavior that is not yet trusted.

---

# 13. What Must Not Delay the MVP

The following activities should not become independent workstreams during MVP development:

- additional strategy research;
- advanced market-microstructure study;
- limit-order modeling;
- short-selling design;
- transaction-cost optimization;
- generic multi-asset architecture;
- concurrency;
- database design;
- live-trading preparation;
- parameter optimization;
- extensive documentation polishing;
- premature performance optimization;
- unnecessary abstraction layers.

If one of these topics appears necessary, first verify whether the immediate problem can be solved within the documented MVP.

---

# 14. Definition of a Good Implementation Task

A good task is:

- small;
- necessary now;
- bounded;
- testable;
- explainable;
- easy to review;
- easy to commit.

Examples:

```text
Define canonical Bar representation and tests
```

```text
Implement canonical CSV parser with invalid-row handling
```

```text
Add deterministic HistoricalDataFeed iteration
```

```text
Define MVP event representation and dispatch tests
```

```text
Implement next-open execution for one pending market order
```

Avoid tasks such as:

```text
Build portfolio subsystem
```

when they can be decomposed into smaller validated increments.

---

# 15. Validation Hierarchy

Use the strongest practical validation for each behavior.

Preferred order:

```text
manual deterministic expectation
        ↓
unit test
        ↓
component integration test
        ↓
end-to-end deterministic test
        ↓
real-data experiment
        ↓
stable regression test where useful
```

Real-market outputs should be interpreted only after deterministic correctness is established.

---

# 16. Documentation Update Rules

During implementation:

## Update `architecture.md` when:

- component responsibilities change;
- ownership changes;
- interfaces change materially;
- event flow changes;
- a new durable architectural decision appears.

## Update `MVP-scope.md` only when:

- a genuine scope correction is explicitly accepted.

Routine implementation decisions must not silently change scope.

## Update `references.md` only when:

- a new source becomes genuinely authoritative or required;
- a reading assignment is shown to be incorrect.

## Update `knowledge-guide.md` only when:

- the conceptual model materially changes.

## Update `project-principles.md` rarely.

It should remain stable.

---

# 17. Completion Principle

The MVP should advance through functioning evidence, not accumulated plans.

The development loop is:

```text
small requirement
→ explicit contract
→ code
→ deterministic evidence
→ integration
→ commit
```

The project is ready to move forward when the current behavior is trusted,
not when every related topic has been studied or every future extension has been designed.

---

# 18. Final Development Principle

During the MVP:

> Build the shortest path to a correct, deterministic, reproducible, and explainable end-to-end backtester.

Preserve clean boundaries.

Validate every important state transition.

Defer everything else.
