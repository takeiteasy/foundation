# Foundation — Roadmap

Working plan from design through optimization. Exit criteria are cumulative: each milestone assumes all prior ones hold.

## M0 — Frontend

Goal: source text to surface AST and back, with tests.

- [x] Project scaffold: `nimble` setup, `src/foundation/` layout, test runner wiring
- [x] Lexer: token stream with spans; identifiers/keywords; string literals with escapes; numeric literals (int, hex, float, exponent); `#` comments (block comment syntax deferred)
  - [x] Arithmetic-expression slice: ints/floats with exponents and `_` separators, identifiers, `+ - * /`, parens, unary minus, `#` comments, Go-style semicolon insertion; AST + printer + roundtrip tests; `foundation parse` CLI
  - [x] Literals-and-keywords slice: string literals (basic C escapes `\n \t \r \\ \"`), hex int literals with `_` separators, reserved words (`var const function if else while return break continue true false null throw try catch finally`), semicolon insertion after strings/literals/`return`/`break`/`continue`; bool/null literal nodes
- [x] Go-style semicolon insertion in the lexer (design.md decision #12); same-line brace grammar
  - [x] Insertion after identifiers/literals/`return`/`break`/`continue` and closers `)]}`; same-line brace rule enforced through the parser
  - [x] Insertion after postfix `++`/`--`
- [x] Parser: precedence-climbing expressions; statements; function declarations; object/array literals
  - [x] Statements-and-control-flow slice: full operator set (`|| && == != < <= > >= + - * / % //`, prefix `!`), `var`/`const` declarations, assignment as a statement plus augmented forms, blocks, `if`/`else`/else-if, `while`, `return`/`break`/`continue`; surface AST split into spanned `Stmt`/`Expr` types
  - [x] Functions-and-calls slice: named function declarations, anonymous function expressions, call postfix (chained calls, full-expression args), duplicate-param rejection; C-style prefix/postfix `++`/`--` expressions completing the design #12 insertion rules
  - [x] Objects-and-arrays slice: array literals, object literals with identifier/string keys, member `.name` and index `[e]` access at call precedence; lvalues widen to fields/indexes for assignment, augmented assignment, and `++`/`--`; statement-position `{` remains a block
- [x] Surface AST types with span attachment
- [x] Pretty-printer (AST dump for debugging and tests)
- [x] Roundtrip tests: parse → print → parse produces identical trees
- [x] Wrap-up: hardening suite (`t_diagnostics.nim`) — truncation of valid programs, seeded fuzzing, unbalanced delimiters, nesting-depth cap producing spanned diagnostics instead of stack exhaustion; tree-style AST dump in the CLI; same-line brace diagnostics name their rule
- [x] Polish: declared names (declarations, function names, parameters) carry their own spans for future name-targeted diagnostics; canonical `render` layout — one statement per line with two-space indented blocks

Exit criteria: roundtrip suite green; malformed input produces spanned diagnostics, never crashes. **Met.**

## M1 — Core IR + Interpreter

Goal: a usable interpreted language from the REPL.

- [x] Core IR form set defined (see design.md) with its own node types
- [x] Resolver/desugarer: surface AST → core IR; scope binding; derived-form lowering (compound assign, `++`/`--`, method-call sugar; for-in and interpolation deferred until their surface syntax exists)
- [x] `Value` model: tagged union, boxing/conversion helpers
- [x] Object system: property storage, prototype/metatable delegation (`getField`/`setField`)
- [x] Tree-walk evaluator: `eval(core, env) → Value`; call frames, closures, control flow
- [x] Error model: `throw`/`catch`/`finally` over `Value`s (design.md decision #11); handler-stack unwinding incl. break/continue/return through `finally`
- [x] REPL: read line, expand frontend pipeline, eval, print result
- [x] CLI skeleton: `foundation run file` via interpreter

Slices:

- [x] S1 — Value & object runtime (`value.nim`): tagged `Value`, truthiness, numeric-tower equality/conversion helpers; `Object` with property table + prototype pointer and prototype-walking `getField`/`setField`. Tested directly against the runtime API.
- [x] S2 — Core IR & resolver (`ir.nim`, `resolver.nim`): core node types (spans kept); scope analysis with capture analysis producing shared-cell capture lists; const enforcement; duplicate-declaration detection via name spans; undeclared-name diagnostics (decision #15); lowerings — augmented assign (evaluate-once temps), pre/postfix `++`/`--`, else-if chains to nested `if`, short-circuit `&&`/`||` as dedicated core forms, groups dropped.
- [x] S3 — Evaluator straight-line (`interp.nim`): literals, declarations, assignment, arithmetic/comparison per the numeric tower (wrapping int, float `/`, floored `%`), `if`/`while`/blocks/jumps; `foundation run file` wired end-to-end.
- [x] S4 — Functions & closures: closure creation capturing defining frames, call frames with static parents, recursion/mutual recursion, shared-cell capture via retained frames (#14). Note: the tree-walk evaluator recurses in host frames, so `nim.cfg` raises Nim's debug call-depth cap until M3's explicit stack.
- [x] S5 — Objects & arrays end-to-end: literals, field/index get/set through the runtime, prototype delegation via provisional `getproto`/`setproto` natives, evaluate-once guarantees proven at runtime (including a `++` double-evaluation fix). Objects keep insertion order (`OrderedTable`) for deterministic rendering; `print` renders containers recursively with a cycle guard.
- [x] S6 — Errors: surface statements `try/catch/finally/throw` (parser additions), full unwind interplay incl. break/continue/return through `finally` with JS-style override semantics, rethrow by identity, canonical `Error` object (`name`/`message`), uncaught-error reporting (`Uncaught: <value>` on stderr, exit 1). Foundation traces (function names + spans) remain for the error-model milestone alongside span attachment.
- [x] S7 — REPL & polish: multi-line input buffering (brackets + open strings), persistent globals via prior-globals seeding, `repl` subcommand with echo/silence rules, demos under `demos/` guarded by smoke tests. Added mid-slice: method-call sugar `obj:m(a)` (receiver evaluated once via the new `ckLet` core form), provisional `tostring`, and a fix for augmented/logical assignment ignoring static levels on outer locals.

Exit criteria: demo programs (functions, closures, objects/metatables, loops, errors) run correctly under the REPL and CLI.

## M2 — Runtime, stdlib, conformance harness

Goal: builtins exist once, and a machine that will catch any future drift.

- [x] Core metamethods (decision #19): `__add/sub/mul/div/mod/idiv`, `__unm`, `__eq/lt/le` with derived `>`/`>=`, and top-level-only `__tostring`; dispatched left-then-right through prototype chains. `__index`/`__newindex` hooks remain future work.
- [x] Native builtin registry operating purely on `Value` API (`stdlib.nim`, decision #20)
- [x] Typed catch clauses: `as` reserved; `catch SomeError as e` matches by prototype chain or thrown-name text, tried in source order with rethrow-on-miss after `finally` (decision #21); bare `catch e` remains the any-catch
- [x] Core library: array, string, map/object utilities — including `len(v)`/`empty(v)` (truthiness deliberately excludes emptiness; these are the explicit check). Delivered: `len`, `empty`, `type`, `push`, `pop`, `keys`, `values`, the string set (`slice`/`indexOf`/`startsWith`/`endsWith`/`upper`/`lower`/`trim`/`split`/`join`/`replace`), object ops (`has`/`del`/`copy`), eager ranges (`range(a, b, step)` half-open Python-style; `irange` inclusive; plain int arrays with a documented materialization cap), and higher-order array utilities — `map`/`filter`/`reduce` (optional seed; empty-without-seed errors) via the native→Foundation `invoke` bridge, plus `sort` with an optional less-than predicate defaulting to `<`'s own dispatch (`__lt`, then numbers/strings); stable in-place insertion returning its array.
- [ ] Stdlib layers written in-language where sensible (shared free of charge by construction)
- [x] **Conformance harness** (`tests/t_conformance.nim`, decision: inline expectations): `.fn` programs under `tests/conformance/` declare expected stdout with `# expect:` comment lines; each registered pipeline (currently interp) runs every program and outputs are diffed exactly. Adding a backend later is one registry entry with zero harness changes. Self-checks verify marker extraction plus mismatch/uncaught-error detection; wired into `nimble test`.
- [ ] Polish: migrate embedded interpreter unit tests toward on-disk `.fn` programs driven by the harness (language-level specs become the single source of truth instead of Nim-side assertions)
- [ ] Spanned Foundation traces: function names + spans carried through unwinding onto uncaught-error reports (remainder of M1's error-model bullet)
- [ ] Golden diagnostics tests (span/message stability)

Language-surface completion pulled forward from "someday" by review: every item here gets exponentially dearer once two backends must agree on it.
- [x] Bitwise operators on int64: `&` `|` `^` `<<` `>>` `>>>` `~` with the matching augmented forms (flags/masks are table stakes for game embedding); C/JS precedence tiering, masked shift counts, no metamethod dispatch, non-int operands are errors. Covered by `tests/conformance/bitwise.fn`.
- [x] Conditional expression `cond ? a : b` — dedicated core form (`ckTernary`); condition evaluates once, only the taken branch runs; right-associative via the else branch; binds loosest (below `||`). Grammar note: `:` inside a ternary's middle branch closes it, so method-call sugar there needs parens. Covered by `tests/conformance/ternary.fn`.
- [x] Default and rest parameters (`function f(a, b = 2, ...rest)`) — defaults evaluate per call in the callee frame after earlier params bind; rest collects extras into a fresh array; arity range minArity..positional. Variadic Foundation functions now exist, which unblocks the in-language stdlib goal. Covered by `tests/conformance/params.fn`.
- [x] String interpolation `"hp: ${hp}"` — lowered in the parser to a concat chain of literal chunks and `tostring(expr)` calls, so rendering semantics (including `__tostring`) are shared with print for free. `${` interpolates, lone `$` is literal, `\$` escapes; inner strings inside interpolations are plain (no nesting). Covered by `tests/conformance/interp.fn`.
- [x] `for-in` loops over arrays, strings, and object keys (values via `o[k]`). Dedicated core form `csForEach`: source evaluated once, runtime dispatch on kind, key snapshot for objects. Loop variable is function-scoped per #18. Covered by `tests/conformance/forin.fn`.
- [x] Destructuring: patterns over arrays and objects in declarations (`var [a, ...t] = e;`, `const {x, y: renamed} = o;`), plain assignment (`[a, b] = [b, a];` — leaves may be any lvalue), and loop headers (`for ([k, v] in pairs(o))`). Lowered entirely in the resolver to ordinary decls/assignments plus frame temporaries: the initializer evaluates once, array reads keep out-of-bounds strictness (#15), object keys read null like field access. New core form `ckArrayRest` gathers `...rest`; new builtin `pairs(o)` snapshots `[key, value]` rows in insertion order. Object-key iteration semantics unchanged.

Exit criteria: harness runs interp-only suite green; adding a pipeline later requires zero harness changes.

## M2.5 — Syntax macros

Goal: third parties (and we ourselves) can define derived forms in-language without touching the frontend; the language surface freezes on purpose afterward. Procedural, modern-sweet.js-style: a macro is an ordinary Foundation function run once in an isolated compile-time VM; parser hooks drive expansion and splice surface AST, so both backends only ever see ordinary trees.

Slices:

- [x] S1 — Syntax declarations: `syntax` reserved; top-level `syntax name = function (ctx) { ... };` lifted from the token stream pre-parse, parsed by the normal frontend, then resolved and evaluated eagerly at the definition site (body binding errors surface immediately with real spans); isolated compile-time VM with full stdlib installed and its output discarded; macro names occupy their own namespace but read as keywords thereafter, with token origins recording sequential visibility (enforced once expansion lands); REPL threads one persistent table across entries; misplaced, malformed, duplicate, and unterminated definitions all produce spanned diagnostics.
- [x] S2 — Statement-position expansion: token cursor handed to macro bodies (`next`/`peek`/`atEnd`, grammar-driven `expand` via transactional sub-parsing); backtick syntax templates with `${...}` holes parse once at definition time and splice structurally at evaluation; returned statements splice at the invocation site; AST-boxing `syntax` value kind; wrong-return, thrown-error, and fragment-failure handling reporting spans at the invocation.
- [x] S2a — Control-flow body grammar groundwork: brace-less single-statement bodies for all control flow (same line or next line via one inserted-`;` skip; explicit `;` never forms a body), hard-error dangling-else policy (an `else` may not bind to an `if` sitting in a brace-less branch), and holes accepted directly in control-flow body slots (block values, bare statements, multi-statement forests). Functions keep mandatory same-line braces.
- [ ] S3 — Expression-position & composition: primary-expression hook; nested macro expansion; expansion budget cap; additional expand kinds; span re-pointing helpers.
- [ ] S4 — Hygiene: template-origin marking so introduced identifiers cannot silently capture; splice-time renaming of introduced declarations; policy documented in the decision log.
- [ ] S5 — Diagnostics, tooling & docs: `foundation expand` prints canonical post-expansion rendering; span-fidelity diagnostics through expansions; printer/roundtrip contract defined for macro-using programs.
- [ ] S6 — Dogfood & exit criteria: conformance `macros.fn` proves a macro-defined control form behaves indistinguishably from a native one (break/continue/return/finally interplay, typed catches inside bodies); demo + smoke test; stdlib derived-form dogfood assessed honestly.

Exit criteria: a macro-defined control form behaves indistinguishably from a native one under the conformance harness, including spanned diagnostics.

## M3 — Compiled backend (Nim emission)

Goal: ship mode; full dual-pipeline verification.

- [ ] Explicit value/frame stack replacing host recursion in the evaluator (retires the `nimCallDepthLimit` crutch; S4's deferred dependency)
- [ ] Coroutines/fibers: reifiable frames on that stack supporting yield/resume (review decision; impossible in tree-walk, scoped here so it is never lost)
- [ ] Nim AST emitter over core IR calling directly into the shared runtime functions
- [ ] Module/wrapper generation; `nim c` invocation from CLI
- [ ] CLI: `run` (interp) / `compile` / `check` / `repl` subcommands complete
- [ ] Conformance suite green through **both** pipelines
- [ ] Startup/compile-time benchmark baseline recorded

Exit criteria: byte-identical observable behaviour between `run` and `compile` outputs on the whole suite; compile latency documented.

## M4 — Embedding (amalgamated C)

Goal: hosts embed Foundation with no Nim toolchain.

- [ ] `Vm` object encapsulates all state (no globals); explicit lifetime API
- [ ] pcall-style boundary: exported API returns status codes; errors retrieved from `Vm`; no Nim exception crosses the C boundary
- [ ] `{.exportc.}` surface + generated header (`--header:`); host-callback registration API (script ↔ engine calls both directions)
- [ ] Amalgamated build: static lib + header artifacts; artifact consumed from a plain C test program
- [ ] Compiled-artifact format: emit/load precompiled program blobs

Exit criteria: a C-only sample app (no Nim installed beyond build time) embeds the VM, loads a blob, calls host functions from script.

## M5 — Performance pass (measured, gated)

Goal: faster execution without semantic change; every step proven by the conformance harness.

- [ ] Profile-driven: pick between bytecode dispatch loop behind the existing `eval` signature vs tree-walk tuning
- [ ] Value representation tuning (NaN-boxing or similar) — must preserve int64 integrity (see numeric tower decisions)
- [ ] Hidden-class/shape optimization for objects if profiling warrants
- [ ] Benchmarks: interp vs compiled vs embedded, tracked over time

Exit criteria: measured wins on real workload benchmarks with zero conformance regressions.

## Revisit later — 1.x surface non-goals

Recorded as deliberate exclusions (design.md decisions #23–24) so they are findable when someone asks "why not?" or priorities change. Cheap to reconsider while only the tree-walk exists; expensive after.

- Class syntax sugar — prototypes + colon methods + metamethods cover the ground
- `switch`/`match` — if-chains plus typed catches suffice
- Labeled `break`/`continue`
- Destructuring (`var [a, b] = pair`)
- Multiple return values / tuple assignment
- `do-while` / `repeat-until`
- Syntax macros beyond M2.5's scope (hygiene variants, reader extensions)
- Optional-chaining `?.` and non-null assertion `!.` operators
