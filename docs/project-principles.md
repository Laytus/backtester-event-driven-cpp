# Project Principles

> **Purpose**
>
> This document defines the permanent development principles of the Event-Driven Backtester project.
> It establishes how design, implementation, validation, documentation, and scope decisions should be made.
>
> It is intentionally independent of the detailed MVP scope, phase plan, architecture, and bibliography.
> Those topics belong to their dedicated project documents.

---

# 1. Project Vision

The project is an event-driven backtesting engine written primarily in modern C++, with Python used where it is better suited for data acquisition, validation, analysis, and visualization.

The project serves two complementary purposes:

1. **Portfolio value** — demonstrate the technical and quantitative skills expected from an entry-level Quant Developer.
2. **Long-term learning value** — provide a structured platform for progressively studying and implementing quantitative finance, trading systems, market microstructure, research methodology, and production-oriented software design.

The MVP is the first major milestone, not the final form of the project.

---

# 2. Primary Development Objective

Build a backtesting system that is:

- correct;
- deterministic;
- testable;
- reproducible;
- explainable;
- maintainable;
- extensible through clean component boundaries.

During the MVP, every task must contribute directly to producing a convincing and defensible portfolio version of the project.

Correctness and clarity take priority over feature count.

---

# 3. MVP First

The MVP has strict priority over post-MVP development.

During MVP development:

- implement only what is required by `MVP-scope.md`;
- defer features that do not materially improve correctness, reproducibility, validation, or portfolio value;
- avoid speculative abstractions;
- avoid designing complete solutions for requirements that do not yet exist;
- preserve only the architectural boundaries needed to prevent obvious future rewrites.

When uncertain, prefer:

> The smallest correct implementation that satisfies the current MVP requirement.

The MVP scope is governed by `MVP-scope.md`.

---

# 4. Learning by Building

The project is a learning system as well as a codebase.

Every important component should be understood at three levels:

1. **What is it?**
2. **Why does it exist?**
3. **How does it interact with the rest of the system?**

Implementation should reinforce understanding rather than replace it.

The objective is not to memorize terminology or reproduce reference implementations.

The objective is to understand the financial meaning, software responsibility, assumptions, and consequences of each component.

---

# 5. Minimum Necessary Theory Before Implementation

A component should not be implemented without understanding the minimum theory required to implement it correctly.

Before coding an important component, the following should be clear:

- the problem it solves;
- its inputs and outputs;
- its financial or technical meaning;
- the state it owns or modifies;
- its main assumptions;
- its interaction with other components;
- the main errors, biases, or failure modes it may introduce.

This principle does **not** require exhaustive study.

During the MVP, reading and theory must be driven by implementation needs.

Preferred workflow:

```text
Required knowledge
        ↓
Minimal design
        ↓
Implementation
        ↓
Deterministic validation
        ↓
Documentation
        ↓
Commit
```

If an implementation question can be resolved without additional study, development should continue.

---

# 6. Incremental Development

Development must proceed in small, stable increments.

The project should not accumulate several unfinished subsystems simultaneously.

Each meaningful increment should ideally produce:

- a clear responsibility;
- a compilable implementation;
- appropriate tests;
- deterministic validation where applicable;
- a coherent commit.

The preferred sequence is:

```text
Understand
→ Define assumptions
→ Design
→ Implement
→ Validate
→ Document
→ Commit
→ Continue
```

A phase or subsystem should not be treated as complete merely because its code exists.

---

# 7. Clear Component Boundaries

The system must preserve clear separation of responsibilities.

Examples of permanent boundaries include:

- market-data components provide observable market information;
- strategies generate trading intent;
- strategies do not directly modify portfolio state;
- signals are not orders;
- orders are not fills;
- portfolio components own cash, positions, and valuation;
- execution components determine simulated execution timing and price;
- performance components evaluate results but do not generate trading decisions;
- reporting components serialize or visualize outputs but do not alter simulation behavior;
- provider-specific data logic remains outside the core domain model.

These boundaries should be preserved even when the implementation remains deliberately small.

---

# 8. Explicit Time Semantics

Backtesting correctness depends on time.

The system must explicitly distinguish where relevant:

- market time;
- data availability time;
- signal generation time;
- order submission time;
- fill time;
- portfolio valuation time.

No component may implicitly access information that would not have been available at the simulated point in time.

Look-ahead bias must be prevented by design rather than by convention alone.

The exact temporal assumptions of the MVP are defined in `MVP-scope.md`.

---

# 9. Strategy and Execution Must Remain Separate

A strategy expresses trading intent.

It must not determine execution assumptions that belong to the simulated market or broker layer.

In particular:

- the strategy should not choose the execution price;
- the strategy should not assume that an order has already filled;
- the strategy should not directly alter cash or positions.

Execution behavior must remain replaceable without requiring the strategy itself to be redesigned.

---

# 10. Determinism Before Realism

The MVP simulation should favor deterministic behavior over unnecessary market realism.

Determinism improves:

- debugging;
- testing;
- reproducibility;
- comparison with manual calculations;
- interview explainability.

A simplified deterministic assumption is preferable to a more realistic model that cannot be validated confidently.

More realistic execution, costs, order lifecycles, and market microstructure may be introduced after the core engine is correct.

---

# 11. Validation Over Plausibility

A result is not correct because it looks reasonable.

Important calculations and state transitions must be validated against known expected results.

Preferred validation hierarchy:

1. manually calculated deterministic example;
2. unit test;
3. integration test;
4. end-to-end deterministic test;
5. real-data experiment;
6. regression test for stable outputs where useful.

Large real-world datasets are not substitutes for deterministic validation.

When a quantitative formula is implemented, its convention and assumptions must be explicit.

---

# 12. Reproducibility

Another developer should be able to reproduce the project from documented inputs and instructions.

Reproducibility includes:

- dependency requirements;
- build configuration;
- data provenance;
- normalization steps;
- simulation configuration;
- strategy parameters;
- initial capital;
- execution assumptions;
- output generation;
- plotting instructions.

A result that depends on undocumented manual steps is incomplete.

---

# 13. Provider-Independent Core Domain

External market-data providers are implementation details.

The core C++ domain must not depend on:

- provider-specific field names;
- `yfinance`;
- pandas objects;
- provider-specific adjustment conventions;
- raw download formats.

The general data boundary is:

```text
Provider Data
        ↓
Provider-Specific Acquisition / Normalization
        ↓
Canonical Dataset
        ↓
Core Backtesting Domain
```

Raw provider data should be retained when useful for traceability.

---

# 14. Professional Boundaries, Minimal Initial Scope

The project should preserve professional software boundaries without overengineering the MVP.

Good examples include:

- explicit domain types where ambiguity would otherwise exist;
- explicit event types;
- explicit timestamps;
- explicit order side and type;
- signed position quantities;
- replaceable execution behavior;
- isolated provider-specific code;
- separated reporting and simulation logic.

Bad examples include:

- implementing unused order-book infrastructure;
- designing generic multi-asset abstractions before multiple assets exist;
- introducing concurrency without a requirement;
- creating deep inheritance hierarchies for hypothetical future features;
- adding dependencies without a concrete need.

The project should remain small enough to understand completely.

---

# 15. Modern C++ With Restraint

The project uses modern C++, but modern language features are tools rather than goals.

Preferred techniques may include, when they improve correctness or clarity:

- RAII;
- value semantics;
- `enum class`;
- `std::optional`;
- `std::variant`;
- `std::visit`;
- `std::expected`;
- `std::span`;
- `std::ranges`;
- `std::chrono`;
- standard algorithms;
- strong ownership semantics.

Do not use advanced language features solely to make the code appear sophisticated.

Prefer the simplest design that communicates ownership, invariants, and intent clearly.

---

# 16. Ownership and State Must Be Clear

Every important piece of mutable state must have an obvious owner.

For each component, it should be possible to answer:

- What state does it own?
- Who is allowed to modify that state?
- What inputs can affect it?
- What invariants must remain true?
- How can its state transitions be tested?

Hidden shared state should be avoided.

Global mutable state should not be used unless a compelling requirement appears.

---

# 17. Invariants Must Be Explicit

Important domain and simulation invariants should be identified during design.

Examples may include:

- historical observations are processed chronologically;
- future observations are inaccessible;
- event ordering is deterministic;
- portfolio equity is consistent with cash and marked position value;
- fill processing is the mechanism that changes holdings;
- invalid market data is rejected before simulation;
- strategy logic does not directly mutate portfolio accounting.

Where practical, invariants should be enforced through types, component boundaries, or tests rather than comments alone.

---

# 18. Error Handling Must Be Deliberate

Errors should be handled according to their nature.

The implementation should distinguish where useful between:

- invalid input data;
- parsing failures;
- invalid configuration;
- violated invariants;
- impossible internal states;
- recoverable runtime errors.

Do not silently accept malformed data or impossible financial state.

Do not introduce complex error infrastructure unless it improves correctness or clarity.

---

# 19. Quality Over Feature Count

A small set of correct and explainable features is more valuable than a larger set of incomplete ones.

During MVP development, prioritize:

- correct accounting;
- explicit timing;
- deterministic behavior;
- readable implementation;
- meaningful tests;
- honest limitations;
- reproducible outputs;
- clear documentation.

A feature should not be included merely because it is impressive in isolation.

---

# 20. Dependencies Must Be Minimal

Every dependency must have a concrete purpose.

General rules:

- prefer the standard library where appropriate;
- keep C++ dependencies minimal;
- isolate third-party behavior behind clear boundaries where useful;
- avoid adding libraries for functionality that can be implemented safely and simply;
- constrain or pin versions where reproducibility requires it.

Provider-specific dependencies must not leak into the core domain model.

---

# 21. Build Quality Is Part of Correctness

The repository should remain buildable throughout development.

The project uses:

- C++23;
- target-based CMake;
- Ninja;
- Catch2 v3.

Compiler warnings should include at least:

```text
-Wall
-Wextra
-Wpedantic
```

Debug and Release builds should complete without warnings.

Build-system complexity should remain proportional to the project.

---

# 22. Testing Is Part of Implementation

Testing is not a final cleanup stage.

Tests should be added alongside the behavior they validate.

Important categories include:

- parsing;
- validation;
- quantitative calculations;
- component state transitions;
- event ordering;
- integration boundaries;
- complete deterministic backtests.

Not every implementation detail requires a test.

Tests should focus on observable behavior, invariants, calculations, and failure modes that matter.

---

# 23. Documentation Must Remain Useful

Documentation exists to support:

- implementation;
- reproducibility;
- maintenance;
- learning;
- interview discussion.

Documentation should remain concise during the MVP.

Do not create documentation simply because a topic exists.

A new document should be introduced only when it has a distinct responsibility that cannot be represented clearly in an existing one.

The core project documentation is:

- `project-principles.md` — permanent development principles;
- `MVP-scope.md` — fixed MVP inclusions, exclusions, assumptions, and Definition of Done;
- `development-guide.md` — phase-by-phase implementation workflow;
- `knowledge-guide.md` — conceptual map of the project domain;
- `references.md` — authoritative project bibliography and phase reading map;
- `architecture.md` — evolving technical architecture.

Important design decisions may later be recorded separately when they genuinely require durable justification.

---

# 24. Single Source of Truth

Project information should not be duplicated unnecessarily across documents.

Each topic has one primary home:

| Topic | Primary Document |
|---|---|
| Permanent development principles | `project-principles.md` |
| MVP inclusions, exclusions, and completion criteria | `MVP-scope.md` |
| Development phases and operational workflow | `development-guide.md` |
| Conceptual relationships | `knowledge-guide.md` |
| Bibliography and required reading | `references.md` |
| Technical architecture and component design | `architecture.md` |

Other documents may reference these sources but should avoid restating them in full.

When two documents conflict, the document that owns the topic is authoritative.

---

# 25. Scope Changes Must Be Explicit

The MVP must not grow implicitly during implementation.

If a new requirement appears, classify it as one of:

1. required for correctness;
2. required for validation;
3. required for reproducibility;
4. required to resolve a serious architectural flaw;
5. useful but deferrable.

Only the first four categories should normally change the MVP.

Useful but deferrable improvements belong in the post-MVP roadmap.

---

# 26. Git History Should Reflect Development Logic

Git history should help explain how the system was built.

Preferred practice:

- small and coherent commits;
- stable commits at meaningful milestones;
- tests committed with the behavior they validate;
- no unrelated changes bundled together;
- no unfinished post-MVP features mixed into MVP work;
- generated large datasets and outputs excluded unless intentionally used as small fixtures.

Branches and commits should support development rather than create process overhead.

---

# 27. Interview Explainability

The developer should be able to explain every important design choice in the MVP.

For any major component, it should be possible to explain:

- why the component exists;
- why its responsibility is separate;
- what state it owns;
- what assumptions it makes;
- how it interacts with other components;
- how its correctness was validated;
- what limitations remain;
- how it could evolve after the MVP.

Complexity that cannot be defended clearly is usually unnecessary.

---

# 28. Long-Term Extension Principle

Post-MVP development should extend the architecture rather than casually replace it.

However, the MVP must not implement future functionality preemptively.

The correct balance is:

```text
Preserve clean extension points
without
building features before they are required
```

If future development reveals that an MVP abstraction was too simple, refactoring is acceptable.

Avoiding every future refactor is not a valid reason to overengineer the MVP.

---

# 29. Final Principle

The project must continuously balance:

```text
Finish a convincing MVP quickly
while
preserving correctness, understanding, and clean architecture
```

When these goals conflict during MVP development:

> Prefer the smallest correct, deterministic, testable, and explainable solution that satisfies the documented MVP scope.
