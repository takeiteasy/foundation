# Steak's build: cccc is the compiler. The comptime pass in
# vendor/buffalo/src/buf_comptime.c reads spec/steak.bflo and lowers it to
# the DFA tables + buf_next() wrapper linked into the binary -- there is no
# .bflo -> .c step and no checked-in generated code.
#
# Plain make rather than `cccc --build`: the embedded build system compiles
# target sources with the host cc, which cannot run the comptime pass; the
# one-shot `cccc -c=native` invocation below is the same path buffalo's own
# native examples use.
CCCC  ?= cccc
BFL   := vendor/buffalo
SRCS  := src/main.c $(BFL)/src/buf_comptime.c $(BFL)/runtime/buf_rt.c
FLAGS := -I$(BFL)/include/buffalo -I$(BFL)/src -I$(BFL)/runtime -Ispec \
         -D BUF_SPEC='"spec/steak.bflo"' -D BUF_STOP_AFTER=5

.PHONY: all smoke

all: bin/steak

bin/steak: $(SRCS) spec/steak.bflo spec/steak_tokens.h
	@mkdir -p bin
	$(CCCC) -c=native $(SRCS) $(FLAGS) -o $@

# Golden smoke: lex every tracked example and diff against its .expected.
smoke: bin/steak
	@for f in examples/*.fn; do \
	    ./bin/steak "$$f" > "$$f.out" || { echo "FAIL $$f"; exit 1; }; \
	    diff -u "$${f%.fn}.expected" "$$f.out" || { echo "FAIL $$f"; exit 1; }; \
	    echo "ok   $$f"; \
	done
