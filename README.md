**YeCC** is an educational compiler designed to serve as a clean and approachable learning resource. Its source code is carefully organized so that all modules follow a logical and chronological development path, reflecting the natural progression of compiler construction.

### Core Design Principles

* **No Circular Dependencies**

  * Each module is strictly independent of any modules that depend on it.
  * This ensures a clear and linear flow of data and control, making the compiler easier to understand, maintain, and extend.
  * By avoiding circular dependencies, the project is simpler for beginners to reason about, as they can follow the chronological development of the compiler.

* **Separation of Concerns**

  * Each module focuses on a single stage of compilation or a single utility to be used by the rest of the compiler.
  * This modularity encourages a deeper understanding of each phase in isolation before seeing how they fit together.

* **Readable and Minimal Code**

  * Code is intentionally kept simple and idiomatic, avoiding clever tricks or overly abstract patterns.
  * Naming is descriptive, and internal APIs are documented to reduce cognitive load for newcomers.

* **Explicit Data Flow**

  * The data transformations between compiler stages is made simple but not over-simplified and traceable.
  * The source processing can be described simply as:
> `source` -> *(lexer)* -> `tokens` -> *(cpp)* -> `tokens (expanded)` -> *(parser)* -> `AST` -> *(semantic analysis)* -> `AST (typed)` -> *(IRGen)* -> `IR` -> *(optimizer)* -> `IR (optimized)` -> *(backend)* -> `assembly`
  * This promotes a better mental model of how source code becomes executable output.

* **Educational Transparency**

  * All major structures (symbol tables, error reporting, type system) and internal utilities (hashmaps, streamers, interners) are implemented from scratch and exposed in the codebase.
  * Internals are not hidden behind libraries or opaque abstractions, making the learning process hands-on.

* **Error-Tolerant by Design**

  * The compiler attempts to recover from common syntax and semantic errors where appropriate.
  * This helps users understand not only how to build a compiler, but also how to make one that is user-friendly and robust.

Chronological and logical order of modules of the compiler:
