/*
 * main.c -- steak driver, lexer-slice bootstrap.
 *
 * Reads the file named by argv[1] (or stdin for "-"), lexes it through the
 * buffalo-generated DFA via buf_next(), and prints one line per token:
 *
 *     KIND "lexeme" line:col
 *
 * TOK_ERROR stops the run with a diagnostic on stderr and exit status 1.
 * The parser (buffalo %grammar) and the ASI filter pass (design.md #12)
 * slot in between the lexer loop and the printer later.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buf_rt.h"
#include "steak_tokens.h"

static const char *tok_names[] = {
    "EOF", "ERROR",
    "KW_TRUE", "KW_FALSE", "KW_NULL",
    "KW_VAR", "KW_CONST", "KW_FUNCTION",
    "KW_IF", "KW_ELSE", "KW_WHILE", "KW_FOR", "KW_IN",
    "KW_RETURN", "KW_BREAK", "KW_CONTINUE",
    "KW_THROW", "KW_TRY", "KW_CATCH", "KW_FINALLY",
    "KW_AS", "KW_SYNTAX",
    "IDENT",
    "NEWLINE",
    "INT", "FLOAT", "STRING",
    "SEMI",
    "LPAREN", "RPAREN",
    "LBRACE", "RBRACE",
    "LBRACKET", "RBRACKET",
    "COMMA", "COLON", "DOT", "ELLIPSIS", "QUESTION",
    "PLUS", "MINUS", "STAR", "SLASH", "SLASHSLASH", "PERCENT",
    "INC", "DEC",
    "ASSIGN",
    "PLUSASSIGN", "MINUSASSIGN", "STARASSIGN", "SLASHASSIGN",
    "SLASHSLASHASSIGN", "PERCENTASSIGN",
    "AMPASSIGN", "PIPEASSIGN", "CARETASSIGN",
    "SHLASSIGN", "SHRASSIGN", "USHRASSIGN",
    "AMPAMPASSIGN", "PIPEPIPEASSIGN",
    "EQEQ", "NOTEQ", "LT", "LE", "GT", "GE",
    "SHL", "SHR", "USHR",
    "AMP", "PIPE", "CARET", "TILDE",
    "AMPAMP", "PIPEPIPE",
    "BANG"
};

#define TOK_NAME_COUNT (sizeof(tok_names) / sizeof(tok_names[0]))

static const char *tok_name(int kind)
{
    if (kind < 0 || (size_t)kind >= TOK_NAME_COUNT) return "?";
    return tok_names[kind];
}

/* Print a lexeme (not NUL-terminated) with \n \t \r \\ and " escaped. */
static void print_lexeme(FILE *out, const char *lexeme, int length)
{
    int i;
    fputc('"', out);
    for (i = 0; i < length; i++) {
        switch (lexeme[i]) {
        case '\n': fputs("\\n", out); break;
        case '\t': fputs("\\t", out); break;
        case '\r': fputs("\\r", out); break;
        case '\\': fputs("\\\\", out); break;
        case '"':  fputs("\\\"", out); break;
        default:   fputc(lexeme[i], out); break;
        }
    }
    fputc('"', out);
}

/* Read a whole file (or stdin for "-") into a malloc'd NUL-terminated
 * buffer; the lexer only needs [src, src+len) but NUL costs one byte. */
static char *read_source(FILE *f, int *out_len)
{
    size_t cap = 1 << 16, len = 0, n;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    while ((n = fread(buf + len, 1, cap - len - 1, f)) > 0) {
        len += n;
        if (cap - len < 2) {
            char *tmp = realloc(buf, cap * 2);
            if (!tmp) { free(buf); return NULL; }
            buf = tmp;
            cap *= 2;
        }
    }
    buf[len] = '\0';
    *out_len = (int)len;
    return buf;
}

int main(int argc, char **argv)
{
    FILE *f = stdin;
    char *src;
    int len;
    BufLexer lx;

    if (argc > 2) {
        fprintf(stderr, "usage: steak [file|-]\n");
        return 2;
    }
    if (argc == 2 && strcmp(argv[1], "-") != 0) {
        f = fopen(argv[1], "rb");
        if (!f) {
            fprintf(stderr, "steak: cannot open %s\n", argv[1]);
            return 2;
        }
    }
    src = read_source(f, &len);
    if (f != stdin) fclose(f);
    if (!src) {
        fprintf(stderr, "steak: out of memory\n");
        return 2;
    }

    buf_lexer_init(&lx, src, len);
    for (;;) {
        BufToken tok = buf_next(&lx);
        if (tok.kind == TOK_EOF) break;
        if (tok.kind == TOK_ERROR) {
            fprintf(stderr, "steak: unexpected character at %d:%d\n",
                    tok.line, tok.col);
            free(src);
            return 1;
        }
        printf("%s ", tok_name(tok.kind));
        print_lexeme(stdout, tok.lexeme, tok.length);
        printf(" %d:%d\n", tok.line, tok.col);
    }

    free(src);
    return 0;
}
