# steak

steak is a small dynamic language with JS-flavoured syntax — closures, prototype delegation, method-call sugar, a wrapping numeric tower, try/catch/finally — implemented in C to the spec in `docs/design.md`. Built with [cccc](https://git.sr.ht/~takeiteasy/cccc), whose comptime pass runs [buffalo](https://git.sr.ht/~takeiteasy/buffalo) to lower `spec/steak.bflo` into the DFA tables linked into the binary: no `.bflo → .c` step, no checked-in generated code.

## Quick start

```sh
git submodule update --init   # vendor/buffalo
make                          # bin/steak
bin/steak examples/hello.fn   # token dump
make smoke                    # golden-diff every examples/*.fn
```

## Layout

- `spec/steak.bflo` — token vocabulary (buffalo spec); `spec/steak_tokens.h` is the matching checked-in token header the comptime pass validates
- `src/main.c` — driver: file → `buf_next()` → token dump
- `vendor/buffalo` — submodule; supplies the comptime pipeline (`src/buf_comptime.c`) and the lexer runtime (`runtime/buf_rt.c`)
- `docs/` — language design and roadmap (the spec this implementation follows)

Status: bootstrap — lexer slice only (keywords, identifiers, int/float/hex literals, plain strings, operators, comments, NEWLINE). Parser, ASI, and the rest of the pipeline are tracked in the ticket tracker.
