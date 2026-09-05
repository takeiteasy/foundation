# Foundation — Design

Foundation is a small dynamic language with a JS-flavoured surface syntax, designed to be **both interpreted and compiled from a single shared pipeline**, and embeddable as a portable interpreter for games.

## Goals

- One frontend, two execution modes: tree-walk interpreter for development, Nim-targeting compiler for release builds.
- Uniform semantics: a program behaves identically in both modes, enforced by a conformance harness rather than convention.
- Embeddable: hosts (game engines) link the runtime as plain C with no Nim toolchain, Lua/Wren-style.
- Small core: objects with metatable-style delegation, first-class functions, a tiny set of core forms; everything else desugars or is written in-language.

## Decision log

| # | Decision | Choice | Rationale |
|---|----------|--------|-----------|
| 1 | Overall architecture | Core IR + dual backends | Reader/parser/desugarer shared 100%; only `eval` vs code emission fork. Bytecode can later slot behind the same `eval` signature. |
| 2 | Type system | Dynamic | Tagged-union `Value` everywhere keeps both pipelines honest; no inference machinery to drift. |
| 3 | Compiled value representation | Uniform boxed `Value`s | Compiled code still manipulates `Value`s; wins come from eliminating dispatch overhead. Builtins, object model, and stdlib are single-implementation because of this. |
| 4 | Macros | None initially; later compile-time AST construction | Not a Lisp; no runtime quote/unquote. Comptime macro forms construct AST directly (sweet.js-ish) — and must reuse this frontend's frontend/IR types rather than introducing a parallel AST representation. Delivered as M2.5 per decision #24. |
| 5 | Primary mode | Dev = interpreter, ship = compiled | Fast iteration without compiler latency; semantics defined by the core IR so neither mode is "more correct". |
| 6 | C story | Amalgamated C runtime + data artifacts | Runtime+VM compiled once from Nim to a static C lib (`--mm:orc`, generated header); "compiled" programs ship as loadable artifacts. Hosts need zero Nim knowledge. |
| 7 | Surface syntax | JS-like | C-family operators and punctuation, familiar block syntax. |
| 8 | Name | Foundation | Repo name is the language name. |
| 9 | Numeric tower | Dual int64 + float64 | Lua 5.3-style: distinct integer and float types under one "number" umbrella with automatic widening; full 64-bit ints for IDs/flags/indices in game workloads. |
| 10 | Int overflow / division | Wraparound; float `/`, floor `//` | Two's-complement wraparound is free and C-interop friendly; `/` always yields float so no silent truncation (`6/3 == 2.0`), `//` floor-divides within int64. |
| 11 | Error handling | Exceptions over `Value`s | `throw`/`catch` non-local exit is essential for game scripting; single mechanism maps to a VM handler stack (interp) and one internal Nim exception type (compiled); builtins throw identically in both modes because they are shared code. |
| 12 | Statement termination | Go-style lexer insertion | Lexer emits a virtual semicolon at newline after identifiers, literals, `return`/`break`/`continue`, `++`/`--`, and closers `)]}`. Deterministic, tiny spec, zero parser coupling; deliberately not JS ASI. |
| 13 | `finally` | Included from M1 | Full try/catch/finally unwind paths (including break/continue/return interplay) built and tested together rather than retrofitted. |
| 14 | Closure capture | Shared mutable cells | Captured locals become cells shared by all closures over them (Lua upvalues / JS environments): mutations through one closure are visible everywhere. Matches scripting intuition; costs cell-tracking in the resolver's capture analysis. |
| 15 | Undefined names | Hard errors, resolve-time where possible | Reading or assigning an undeclared name is a spanned diagnostic; globals exist only via top-level `var`/`function`. Typos become errors instead of silent nulls — embedder-friendly. |
| 16 | Declaration visibility | Scope-wide, execution-ordered | Names are visible throughout their declaring scope (mutual recursion works regardless of declaration order); actual reads still follow execution order — reading a declared-but-uninitialized name is a spanned runtime error. Function expressions are ordinary `var`-bound values. |
| 17 | M1 object depth | Prototype delegation only | `getField`/`setField` walk the prototype chain; metamethods (`__index` functions, operator overloading) extend the same two entry points in M2 beside builtins. |
| 18 | Variable scope | Function-scoped | Blocks never introduce scope (JS `var`-style); redeclaring a name within one function — parameters included — is a resolve-time error reported at the second declaration's name span. Simplest model for embedders reading scripts; catches accidental shadowing. |
| 22 | Program organization | One shared global namespace | A `Vm` hosts any number of scripts loaded in order into one global table - Lua's model. Hosts control isolation by using multiple `Vm`s, not by module plumbing. No `import`/`export` form exists by design; scripts coordinate through globals and functions they define. |
| 23 | Surface non-goals (1.x) | Recorded deliberately | No class syntax (prototypes + colon methods + metamethods cover it), no `switch`/`match` (if-chains + typed catches), no labeled breaks, destructuring, multiple returns, or `do-while`. Each is cheap to reconsider while only the tree-walk exists and expensive after; this entry is the standing answer to "why not?". |
| 24 | Syntax macros | Procedural, in-language, parser-integrated (M2.5) | Modern-sweet.js style: `syntax name = function (ctx) { ... };` declares a macro as an ordinary Foundation function. Definitions lift out of the token stream before parsing proper and evaluate once, eagerly, in an isolated compile-time VM carrying the full stdlib (its output discarded). Expansion is driven by parser hooks and splices surface AST, so both backends consume only ordinary trees — compilation never sees macro machinery. Macro names live in their own namespace but read as keywords thereafter; visibility is sequential (occurrences before the definition's token origin stay plain identifiers). Hygiene policy, error reporting through expansions, and the printer/fidelity interaction are the slice's remaining design work. Rationale: reach feature-completeness and stability of the *language* surface before AOT/backend effort locks it down. |
| 21 | Typed catches | Two-tier matcher dispatch | `try` accepts any number of catch clauses, tried in source order; the first match handles the payload and an unmatched payload rethrows after `finally` runs. A clause is `catch [Name [as binding]]`: a bare identifier stays the any-catch binding (`catch e`), so typed matching always goes through `as`. Dispatch per clause: if `Name` resolves to an object global, the payload must be an object whose prototype chain contains it (Lua-style error classes via `setproto`); otherwise the payload's `name` field must equal `Name` as plain text, so `{name: "NetDown"}` throws work with no declaration. Runtime errors arrive as `Error` objects, so `catch Error as e` matches them specifically; non-object payloads never match typed clauses. `as` is reserved everywhere. |
| 20 | Builtin registry | One `installStdlib` entry point | `stdlib.nim` installs every builtin against the `Value` API alone — no interpreter internals — so a future compiled runtime shares them verbatim. Embedders control exposure by deleting globals; the CLI and REPL install the full set. Current members: `print`, `tostring`, `getproto`, `setproto`, `len`, `empty`, `type`, `push`, `pop`, `keys`, `values`, `slice`, `indexOf`, `startsWith`, `endsWith`, `upper`, `lower`, `trim`, `split`, `join`, `replace`, `has`, `del`, `copy`. `len` measures arrays and strings (bytes) and counts object fields; `empty` is its zero check; `push` mutates in place and returns the array; `keys`/`values` project insertion order; `pairs` returns `[key, value]` rows in that order for destructuring loops. Strings measure in bytes with ASCII-only casing for now. `slice` is half-open `[start, stop)` — negatives count from the end and out-of-bounds clamps (unlike index reads, which error). `join` accepts string elements only (`tostring()` first for anything else); `split`/`replace` reject empty separators/sources. Object utilities follow field semantics exactly: `has` walks the prototype chain (present-but-null counts), `del` removes own fields only and silently tolerates misses, `copy` is shallow at the top level and keeps the prototype. |
| 19 | Metamethods | Core Lua-style set on prototypes | `__add/sub/mul/div/mod/idiv`, `__unm`, `__eq/lt/le`, and `__tostring` are ordinary fields dispatched through the prototype chain (left operand first, then right; operands keep source order). `>`/`>=` derive from `__lt`/`__le` with swapped operands. Equality coerces the metamethod's result through truthiness; without `__eq`, identity rules as before. `__tostring` serves top-level rendering only (`print`, REPL echo, `tostring`) — never nested container elements — and must return a string. `__index`/`__newindex` hooks stay future work; reads/writes remain pure delegation. |

## Architecture

```
              FRONTEND — shared 100%
source ─▶ lexer ─▶ parser ─▶ Surface AST(+spans) ─▶ resolver/desugar ─▶ Core IR
                                                                    │
                                    ┌───────────────────────────────┴──────────────┐
                              INTERPRETER                                   COMPILED
                        tree-walk eval(core) → Value                  core → Nim AST → nim c
                              ▲                                              │
                    doubles as the embeddable VM                same Value runtime, linked in
```

Everything above the backend is identical for both modes. The backend fork is deliberately narrow:

- `interp/` — recursive evaluator over core IR with environment frames. Also serves as the embedded VM and the REPL engine.
- `backends/nimemit/` — emits Nim AST for `nim c`. Compiled code operates on the same boxed `Value`s as the interpreter, calling into the same runtime functions.

## Value model and object system

```nim
type
  ValueKind* = enum vkNil, vkBool, vkInt, vkFloat, vkString,
                      vkArray, vkObject, vkFunction, vkNative
  Value* = ref object
    case kind*: ValueKind
    of vkNil: discard
    of vkBool: boolVal*: bool
    of vkInt: intVal*: int64
    of vkFloat: floatVal*: float64
    of vkString: strVal*: string
    of vkArray: arrVal*: seq[Value]
    of vkObject: objVal*: Object
    of vkFunction: fnVal*: Function
    of vkNative: nativeVal*: NativeFn
```

The entire object/metatable machinery lives in the runtime module as plain functions over `Value`:

```nim
proc getField*(v: Value; key: string): Value   # prototype/metatable walk lives here, once
proc setField*(v: Value; key: string; val: Value)
proc callValue*(vm: var Vm; callee: Value; args: openArray[Value]): Value
proc truthy*(v: Value): bool
proc binop*(vm: var Vm; op: BinOp; a, b: Value): Value
```

Because the compiled backend emits calls into these same functions, metatable dispatch, builtins (`array.push`, `str.format`, ...), and any stdlib written in-language are automatically identical across modes. The compiler's job is limited to translating core-IR control flow into native loops/calls — removing interpretation overhead and nothing else. This is what makes semantic drift structurally difficult rather than merely discouraged.

- **Truthiness**: only null and false are falsy; `0`, `0.0`, `""`, and empty containers are truthy (the Lua/Wren rule — no zero/empty-string gotchas).
- **Equality**: int/float cross-compare through the numeric tower (`1 == 1.0`); strings by content; arrays, objects, and functions by identity; null equals only null.

Performance expectation: uniform-boxed compiled mode lands ~3–10x over naive tree-walking, not near-native. Hot paths in games go through host APIs anyway. Unboxing/type inference is explicitly out of scope for v1; if pursued later it is a gated optimization behind conformance tests, not a design change.

## Core IR

The resolver/desugarer reduces surface forms to a minimal form set (sketch):

```
literal · local-var · assign · field-get · field-set · index-get · index-set
if · while · block · break · continue · return
call · let (expression temp) · closure(params, captures, body) · try/catch
```

Derived surface constructs (for-in, compound assignment, optional chaining, string interpolation, classes) desugar here. Both backends consume only core forms, which is where the language's semantics are normatively defined.

- **Binding**: names resolve through enclosing function boundaries, then to globals; undeclared names are resolve-time errors (#15). Globals bind at execution order — reading a declared-but-uninitialized global is a spanned runtime error. Variables are function-scoped (#18).
- **Outer access**: cross-function reads/writes carry static levels; closures retain their defining frame, so captured mutations are shared by construction (#14).
- **Evaluate-once**: augmented and logical assignments lower to plain assignment plus `if` with fresh frame temporaries — container/index subexpressions evaluate exactly once, before the right-hand side. `++`/`--` become an atomic core form (no temporaries needed). `&&`/`||` remain binary forms whose short-circuit evaluation is normative for both backends. Method-call sugar lowers through `let`, the expression-level temporary: `obj:m(a)` evaluates `obj` once into a slot, then calls its `m` field with `obj` prepended to the arguments.

## Embedding and the C story (tier 2)

Distribution model follows Lua/Wren:

- Build the runtime + VM once from Nim into an amalgamated C static library: `nim c --mm:orc --header:foundation.h -d:release`.
- All VM state lives in an explicit `Vm` object (à la `lua_State`) — no globals, reentrant by construction.
- Nim exceptions never cross the embedding boundary; the exported API is pcall-style: entry points return status codes with the error `Value` retrievable from the `Vm`.
- "Compiling" a program produces a loadable artifact consumed by the embedded VM; hosts link `libfoundation.a` + `foundation.h` with no Nim toolchain.

Direct-to-C transpilation of whole programs (hand-written C runtime) is rejected for now: maximum portability but a second runtime implementation to keep in parity.

## Error handling

Single mechanism: exceptions carrying arbitrary `Value`s (JS parity — any value may be thrown).

```js
try {
  throw Error("boom");
} catch (e) {
  // e is the thrown Value
} finally {
  // runs on every exit path: normal, throw, break/continue/return through it
}
```

- **Canonical error**: `Error` is an ordinary metatable-based object (`name`, `message`, span fields), so error behaviour exercises and validates the object system itself.
- **M1 status**: `try`/`catch`/`finally`/`throw` are surface statements lowered to `csTry`/`csThrow`. The catch binding is optional (`catch (e)` or bare `catch`) and function-scoped like every variable, but declaration-idempotent — sequential `catch (e)` blocks are idiomatic and never collide (deliberately outside #18's strictness). Runtime errors surface to catch clauses as `{name: "Error", message: ...}` objects; span fields join when spans attach. `finally` follows JS completion semantics: signals (break/continue/return) pass through uncatchable but trigger it, and an abrupt finally replaces any pending exit.
- **Rethrow**: bare `throw` inside `catch` re-raises the in-flight error.
- **Traces**: best-effort Foundation-level stack (function names + spans) captured at throw; Nim or C traces never leak.
- **Uncaught**: `Error: <message>` plus the Foundation stack on stderr, nonzero exit code.
- **Pipeline mapping**: interpreter keeps a handler stack on the VM; compiled mode raises one internal Nim exception type wrapping the `Value`, caught by emitted try/catch. `finally` cleanup paths are identical logic in both.
- **Layering**: Result-style helpers may later be added as stdlib written in-language; they are ergonomics over this mechanism, not a second one.
- **Embedding**: uncaught errors convert to pcall status codes at the C boundary (see M4).

## Surface syntax notes

- Statement termination via Go-style lexer insertion: a newline becomes a virtual semicolon when the previous token is an identifier, numeric/string literal, `return`/`break`/`continue`, `++`/`--`, or a closer `) ] }`. Lines continue only by ending with an operator, comma, dot, or open bracket; a line may not begin with `.` or `(`. Insertion is suppressed while a `(` or `[` remains unclosed unless braces are also open (statements inside a function body sitting in an argument list still terminate) — multi-line collection literals therefore never break across element lines.
- Consequence, enforced by grammar rather than style: braces are same-line (1TBS) — Allman-style `{` on its own line would terminate the header.
- Comments: `#` line comments running to end of line. Block comment syntax is deferred (undecided).
- Statements: `var x = e;` / `var x;` (binds null) / `const k = e;` (initializer required); destructuring declarations `var pattern = e;` / `const pattern = e;` where a pattern is an array (`[a, [b], ...rest]` — rest last only) or an object (`{a, b: renamed, "k k": nested}`); assignment is a statement, not an expression; plain assignment accepts an array-literal pattern on the left (`[a, b] = [b, a];`, leaves may be any lvalue); augmented assignments `+= -= *= /= %= //= &&= ||= &= |= ^= <<= >>= >>>=` preserve their target node and lower in the resolver (targets evaluate once); braced blocks; `if (cond) { } else { }` with `else if` chains; `while (cond) { }`; `for (x in source) { }` with break/continue — the source evaluates once and iteration dispatches on its runtime kind (array elements, one-char strings over bytes, object keys in insertion order from a snapshot); the loop variable is function-scoped (#18), so it declares into the enclosing scope, outlives the loop, and closures over it share one cell; `return [e];`; `break;` / `continue;`.
- Destructuring lowers entirely in the resolver to ordinary declarations/assignments plus frame temporaries: the initializer runs exactly once per statement, every element read observes the same snapshot, and the language's own access semantics apply unchanged — array positions too short fail like out-of-bounds indexing (#15), missing object keys read null. Pattern names declare exactly like `var` names of the same constness; duplicates within one pattern are redeclarations. Loop headers accept patterns and bind per iteration into the enclosing function scope (#18). `pairs(o)` snapshots `[key, value]` rows in insertion order for `for ([k, v] in pairs(o))`.
- Increment/decrement: C-style `++e`/`e--` in both prefix and postfix forms, usable as expressions (prefix yields the updated value, postfix the original); targets must be assignable.
- Functions: named declarations at statement level (`function f(a, b) { ... }`); anonymous function expressions in expression position (statement position always means declaration — IIFEs need parentheses, JS-style); calls `f(a, b)` postfix-bind tighter than every operator. Parameters may declare defaults (`b = 2`) and a trailing rest (`...rest`): provided arguments bind left to right into positional slots; missing defaulted params evaluate their defaults per call in the callee frame after earlier params are bound (so defaults may read them); the rest parameter collects every extra argument into a fresh array. Arity must land between the required prefix and the positional count — unbounded when a rest parameter exists.
- Objects and arrays: array literals `[a, b]`; object literals `{key: value}` with identifier or string keys sharing one namespace; member access `.name` and indexing `[e]` postfix-bind at call level, left to right. Statement-position `{` is always a block — object literals in expression-statement position need parentheses. Assignment/augmented-assignment/`++`/`--` targets are any of identifier, field access, or index access (`isLvalue`).
- Errors: `try { ... } catch [Name [as b]]? { ... } finally { ... }` — clauses glue to the closing brace like `else`, may follow on the next line despite semicolon insertion, and at least one clause is required. Multiple catch arms are allowed, tried in order (decision #21). `throw expr;` carries any value.
- Methods: `obj:method(args)` passes the receiver as the first argument; methods declare it explicitly, conventionally named `self` (a normal identifier, no keyword). Chains compose: `make():render()`. Comments start with `#`; there are no block comments yet.
- Indexing: arrays and strings take integer indices — negatives count from the end, integral-valued floats normalize to their integer, anything non-integral is an error; out-of-bounds access on either direction is a runtime error (typo strictness per #15). Strings are immutable: indexing reads single-character strings, index assignment is rejected. Object keys are strings or integers (integral floats normalize), one namespace with field names; other key types are errors. Missing fields read as null; duplicate literal keys keep the last value.
- Container subexpression discipline: the object/index expressions of a field or index target evaluate exactly once per statement — plain and augmented assignments hoist them into resolver temporaries, `++`/`--` into evaluator frame temps.
- Rendering (`print`): values render recursively on one line — arrays `[1, "two"]`, objects `{b: 1, a: 2}` in insertion order — with strings quoted inside containers but raw at top level; cycles render as `[...]`/`{...}` instead of hanging.
- Prototypes (M1): delegation walks the chain for reads; writes always land on the receiver. `getproto(o)` / `setproto(o, proto-or-null)` builtins manage chains (registry decision #20); cyclic chains terminate reads rather than looping. `tostring(v)` renders any value as a string for message building (no implicit coercion exists). Operators dispatch metamethods per decision #19: arithmetic, unary minus, comparisons (with derived `>`/`>=`), equality, and top-level `__tostring`; augmented assignment inherits overloading automatically because it lowers to plain operators.
- REPL (`foundation repl`): entries run the full pipeline against one persistent `Vm`. Trailing expression statements echo their value with strings quoted; plain null stays silent. Entries buffer while brackets or string quotes remain open. Each entry is its own compilation unit seeded by prior globals, so top-level catch bindings persist like `var` and vars may be rebound across entries — only `const` is protected.
- Conditions are parenthesized; bodies are `{}` blocks opened on the header's line, or a single brace-less statement on the same line as the header or the next one (exactly one inserted `;` after the header is skipped, so an explicit `;` never forms a body). An `else` binding to an `if` that is itself the brace-less branch of another `if` is rejected as ambiguous — add braces to say which `if` owns it. `else` follows the closing `}` on the same line. Function bodies stay mandatory-braced with the same-line rule.
- Control-flow bodies (`if`/`else`/`while`/`for-in`/`catch`/`finally`, not functions) additionally accept a syntax-template hole in the body slot: block values, bare statements, and multi-statement forests all splice into the slot (see syntax macros below).
- Expression operators, loosest to tightest binding: `?:`, `||`, `&&`, `|`, `^`, `&`, `== !=`, `< <= > >=`, `<< >> >>>`, `+ -`, `* / // %`, prefix `- ! ~`. All binary operators left-associate; `?:` right-associates through its else branch (`a ? b : c ? d : e` nests in the else) and its condition evaluates once with only the taken branch running. Inside a ternary's middle branch `:` always closes the branch — method-call sugar there needs parens. `/` vs `//` semantics per the numeric tower below. `&&`/`||` short-circuit and yield the deciding operand itself (Lua-style), not a coerced boolean. `+` concatenates two strings; no other implicit coercions exist anywhere.
- Parser fidelity contract: the surface parser preserves derived forms verbatim (augmented assignments, `++`/`--`, explicit groups, object key spellings) and performs no lowering — the resolver owns all desugaring. Nodes are plain data (ref objects, public fields, spans) meant to be constructed and matched directly by compile-time macro forms (decision #4), which therefore reuse these exact types.
- Declared names carry their own spans: declaration identifiers (`var`/`const` names, function names, parameters each as name+span) are separately spanned so diagnostics can point at the specific name rather than the whole statement.
- Canonical rendering: `render` reprints programs in the grammar's own house style — one statement per line, two-space indented braced bodies, empty blocks `{}`, `else` glued to the closing `}` (a line break there would trigger semicolon insertion). Output always reparses to an identical tree.

## Syntax macros (M2.5)

Macros let programs define derived forms without touching the frontend. The model is procedural (modern sweet.js): a macro is an ordinary Foundation function, invoked at compile time with a cursor over the invocation-site tokens, returning surface AST that splices into the tree being parsed (see decision #24).

- **Declaration**: top level only — `syntax name = function (ctx) { ... };`. Definitions are lifted from the token stream before parsing proper (`syntax` is reserved everywhere else), then resolved and evaluated eagerly in an isolated compile-time VM: fresh globals, full stdlib installed, its output discarded, and no access to program runtime state. Evaluation goes through the ordinary resolver, so body errors surface immediately at the definition with real spans.
- **Namespaces and visibility**: macro names occupy their own namespace (a variable may share the spelling) but read as keywords thereafter — once defined, later occurrences of the name where a form can begin belong to the macro. Visibility is sequential: token origins recorded at definition time ensure occurrences before the definition stay plain identifiers, so earlier code cannot change meaning under a later definition. A REPL session threads one persistent macro table across entries, seeded like prior globals; redefining a macro in a later entry collides rather than silently replacing.
- **Pipeline position**: lex → lift/evaluate definitions → parse-with-expansion → resolve → backends. Both execution modes consume only expanded trees.
- **Syntax templates**: macro bodies build output with backtick templates — ordinary Foundation source carrying `${...}` holes (`\${` escapes a literal opener; other backslashes pass through verbatim; templates may span lines; single-backtick templates do not nest backticks in literal text — use the triple-backtick form for that). A template parses **once at definition time** as one token stream: literal chunks relex with spans shifted onto their real file positions and grammar binds across holes naturally, so template text errors surface immediately with real spans. Holes become placeholder nodes substituted structurally at each evaluation: an expression-position hole must carry exactly one expression; a statement-position hole splices its whole forest (several statements wrap in a block per #18; none splice away). Templates may also be delimited by triple backticks ```` ```…``` ````: identical grammar and hole semantics, but single or doubled backticks are ordinary literal text inside (the closing ` ``` ` remains unrepresentable). Structural tokens stay literal — write `if (${c}) { ${b} }`, not `if (${c}) ${b}` — except that control-flow body slots accept holes directly (block, bare statement, or multi-statement forest). One boundary follows from per-statement expansion: an `else` after an invocation can never bind to an `if` produced by the expansion, because the caller's statement ends at each `expand` — a macro that owns an if/else must take both branches as holes.
- **Expansion contract**: the parser fires the statement hook wherever a statement can begin. When the leading identifier names a defined macro visible at that token position, the body runs once in the compile-time VM with a `ctx` object: `peek()`/`next()` return `{kind, text, line, col}` token descriptors (or null at the end), `atEnd()` reports exhaustion, and `expand(kind)` consumes grammar-driven input via transactional sub-parsing — `"expr"`, `"stmt"`, or `"block"` — advancing past exactly what it consumed. The cursor starts just past the invocation name; non-consuming macros cannot loop. The macro must return syntax: its forest splices at the invocation site (empty returns splice nothing); anything else raises `syntax 'name' must return syntax, got <type>` at the invocation span. A thrown value surfaces as `syntax 'name' threw an error: <message>`; any other failure as `syntax 'name' failed: ...` — both at the invocation span. Fragment errors inside `expand` carry their real spans into the caller's source and abort compilation.
- **Spans policy**: expanded output keeps truthful origin spans — hole content points into the invocation's real tokens; template skeleton points at its definition site. Canonical post-expansion rendering and printer/roundtrip fidelity for macro-using programs are specified in S5.
- **Current limitations** (revisited by later slices): macro calls inside template literals do not recursively expand (S3 adds nested expansion); `${}` interpolation fragments inside strings bypass expansion; holes cannot fill declaration-name slots; expansion depth is capped to guard runaway recursion until the formal budget lands (S3).
- Expansion mechanics beyond statement position are specified slice by slice in [roadmap.md](roadmap.md) M2.5 and documented here as they land.

## Numeric tower

Two number types — `int64` and `float64` — sharing one "number" concept (Lua 5.3 model):

- **Promotion**: mixed arithmetic widens int→float automatically; no implicit narrowing anywhere.
- **Overflow**: int arithmetic wraps (two's complement). Documented, not detected; matches C host interop.
- **Division**: `/` always produces float (`6/3 == 2.0`); `//` floor-divides and stays int when both operands are ints.
- **Modulo**: floored (`%` sign follows divisor), consistent with `//`.
- **Bitwise**: operate on int64; both arithmetic `>>` and logical `>>>` shifts provided. No promotion — float, string, bool, or object operands are errors; bitwise takes no part in metamethod dispatch. Shift counts mask to 0..63 so every count is deterministic (JS-style masking generalized to 64 bits). Precedence follows the C/JS tiering: `| ^ &` between `&&` and `==`, shifts between relational and additive.
- **Equality & keys**: `1 == 1.0` is true; as map/object keys, integral-valued floats normalize to their integer value so they hash to one entry.
- **Literals**: `42` int · `42.0`, `4.2e1` float · `0x` hex, `0b` binary, `0o` octal (ints only) · `_` digit separators · `'c'` character literals — exactly one character after escape decoding (`\n \t \r \' \\ \$`), valued as an ordinary one-character string; longer content is a lex error pointing at the second character. Triple-quoted `"""..."""` strings are raw multi-line text: no escape processing and no `${}` interpolation (a literal `${` is unrepresentable — use a regular string), the newline immediately after the opening delimiter is consumed, and the closing `"""` cannot appear in the content.
- **String interpolation**: `"hp: ${hp}"` lowers in the parser to a concat chain of literal chunks and `tostring(expr)` calls, so rendering (including `__tostring` on objects) matches print exactly. `${` opens an interpolation, a lone `$` is literal, and `\$` writes a literal dollar (so `\${` escapes the opener). Expressions inside may nest braces and plain string literals; nested interpolation is not recognized. Fragment errors report at their real source positions.
- **Higher-order builtins**: `map`/`filter`/`reduce` run user callbacks through one native→Foundation bridge (`invoke`), so defaults/rest behave as in ordinary calls; `reduce` takes an optional seed and errors on empty-without-seed. `sort(arr, cmp?)` takes a less-than predicate (truthy = first belongs earlier), defaulting to `<`'s own dispatch — `__lt` through prototype chains, then numbers and strings; it is a stable in-place insertion returning its array.
- **Conversion**: explicit `int(v)` / `float(v)` builtins.
- **Ranges**: `range(b)` / `range(a, b)` / `range(a, b, step)` are half-open like Python; `irange` is the inclusive form (`irange(5)` is `0..5`). Both build plain int arrays eagerly — `type()` says "array", and len/index/slice/for-in apply — with a documented element cap (1,000,000) so runaway bounds raise a clean error instead of exhausting the host. Bounds require integers; a zero step is an error; counting uses uint64 differences so extreme bounds (full int64 spans) fail correctly rather than overflowing.

M5 note: NaN-boxing cannot inline a full int64 in a double's payload; the plan is SMI-style unboxed small ints with heap-boxed wide ints, or an alternative tag layout. This affects only box internals — the semantics above are representation-independent.

## Conformance strategy

The conformance harness is the project's correctness backbone: every test program runs through **both** pipelines and stdout/results are diffed. Any behavioural difference between interpreter and compiled output is a bug in one of them. The harness exists from M2 onward, before the compiled backend, so emission never outruns verification.

Mechanics: programs live under `tests/conformance/*.fn` and declare expected output inline with `# expect:` comment lines (one per expected stdout line, matched in order); each pipeline registers once in `tests/t_conformance.nim` and runs every program with output collected instead of written. A printed newline inside a string cannot be expressed by the marker — such output is asserted indirectly (lengths, single-line fragments) rather than growing the marker syntax.

## Roadmap

See [roadmap.md](roadmap.md) for the milestone plan (M0 frontend → M5 performance pass) with deliverables and exit criteria.

## Relation to nfl

Foundation borrows nfl's proven spine — reader → expand/lower → emit-as-Nim — but generalizes it: nfl lowers Lisp to Nim *inside* Nim's compile-time VM (with attendant restrictions like unavailable `getCurrentDir`), whereas Foundation runs its frontend natively in its own process and hands Nim only final emitted code. nfl's `NflDatum` quoted-data type and its hidden `defmacro-proc` evaluator were early sketches of Foundation's `Value` model and interpreter respectively.
