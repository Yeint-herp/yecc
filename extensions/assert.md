# YECC Preprocessor Assertions (`#assert` / `#unassert`)

**Status:** YECC extension
**Since:** YECC 1.0 (2025-08-28)
**Guard:** enabled unless in pedantic mode.
**Goal:** structured, namespaced feature flags.

## 1. Model

* A **predicate** is a **dot-qualified identifier path** with arbitrary depth:

  ```
  machine.wordsize
  sys.cpu.vendor
  vendor.nvidia.gpu.sm
  ```

* Each predicate holds **exactly one** **value-token-seq** (a sequence of preprocessing tokens).

* Re-asserting the same predicate **overwrites** the previous value.

### Equality rule

* Values are compared by **preprocessing token sequence equality**, **not** by source whitespace.
* Comments and runs of spaces are irrelevant.
* The value **ends at the first `)`**; **no nesting** of parentheses inside the value.

Examples (equal unless tokens differ):

```
( x + y )   ==  (x + y)   ==  (x/*c*/+y)
(x<<y)      !=  (x < < y)
```

## 2. Syntax

### 2.1 Directives

```c
#assert   qualified_predicate '(' value-token-seq ')'
#unassert qualified_predicate
```

* `qualified_predicate` = `identifier ('.' identifier)*`
* `value-token-seq` = raw tokens up to the first `)` (no nesting).
* `#assert` **sets/replaces** the value of that predicate.
* `#unassert` **removes** the predicate (if absent: no-op).

### 2.2 Queries (in `#if` / `#elif`)

Primary (preferred) form:

```c
#if qualified_predicate(value)   // true if value equals the predicate's current value
#if qualified_predicate          // true if the predicate currently has any value
```

Examples:

```c
#if machine.wordsize(64)
#elif machine.wordsize(32)
#endif

#if sys.os(linux)
#endif

#if tool.compiler               // any value set for tool.compiler?
#endif
```

### 2.3 Simple function form (portable/explicit)

Two operators are recognized **only inside preprocessor expressions**:

```c
#if __assert(qualified_predicate, value)   // 1 if equal, else 0
#if __assert_any(qualified_predicate)      // 1 if predicate exists (has a value), else 0
```
The value **may** be wrapped in a pair of parentheses, as no nesting is allowed, `__assert(sys.os, linux)` and `__assert(sys.os, (linux))` are considered identical.

* These are **preprocessor operators**, not ordinary macros; users cannot redefine/undef them.

## 3. Diagnostics

* `error: missing ')' in #assert value`
* `error: nested '(' in #assert value`
* `warning: #assert/#unassert are a YECC extension` (under `-Wpedantic`)
* `note: reasserting 'ns.path' replaces previous value` (under `-Wreassert`)

## 4. Command-line hooks

* `-A qualified.predicate(value)`     → pre-assert before preprocessing
* `-Uassert qualified.predicate`      → pre-unassert (remove)

## 5. Examples

### 5.1 Word size gating

```c
#assert machine.wordsize(64)

#if machine.wordsize(64)
  typedef unsigned long word_t;
#elif machine.wordsize(32)
  typedef unsigned int  word_t;
#else
  #error "unknown word size"
#endif
```

### 5.2 OS and compiler

```c
#assert sys.os(linux)
#assert tool.compiler(yecc 3)

#if __assert(sys.os, linux) && __assert(tool.compiler, (yecc 3))
  // linux + yecc 3
#endif
```

### 5.3 Existence check

```c
#assert build.profile(release)

#if build.profile
  #define NDEBUG 1
#endif
```
