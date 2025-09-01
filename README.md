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

* **No typedef for structs**

Chronological and logical order of modules of the compiler:
- Utilities module `utils.h`
- Arena allocator module: `arena.h` `arena.c`
- Hashmap module: `map.h`
- Vector (dynamic array) module: `vector.h`
- Deque module: `deque.h`
- Compiler global state context module `context.h` `context.c`
- String interner module: `string_intern.h` `string_intern.c`
- File streamer module: `streamer.c` `streamer.h`
- Error diagnostics module: `diag.h` `diag.c`
- Token module: `token.h`
- Lexer module `lexer.h` `lexer.c`
- Debug printing module `print.h` `print.c`

The formatting module `print.h` is special as it provides formatting functionality for all other modules and can be called into by any when human readable serialization is needed.