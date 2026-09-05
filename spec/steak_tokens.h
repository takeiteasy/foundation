/*
 * steak_tokens.h -- steak's token kinds, mirroring spec/steak.bflo's %tokens
 * list (same names, same order). Validated at compile time by buffalo's
 * comptime pass against the spec; TOK_EOF = 0 and TOK_ERROR = 1 are
 * reserved by buf_rt.h.
 *
 * Hand-written for now; buffalo #25 (--emit-tokens) will generate it once
 * that lands upstream. Update the spec's %tokens line and this enum
 * together.
 */
#ifndef STEAK_TOKENS_H
#define STEAK_TOKENS_H

enum {
    TOK_EOF = 0, TOK_ERROR = 1,
    TOK_KW_TRUE, TOK_KW_FALSE, TOK_KW_NULL,
    TOK_KW_VAR, TOK_KW_CONST, TOK_KW_FUNCTION,
    TOK_KW_IF, TOK_KW_ELSE, TOK_KW_WHILE, TOK_KW_FOR, TOK_KW_IN,
    TOK_KW_RETURN, TOK_KW_BREAK, TOK_KW_CONTINUE,
    TOK_KW_THROW, TOK_KW_TRY, TOK_KW_CATCH, TOK_KW_FINALLY,
    TOK_KW_AS, TOK_KW_SYNTAX,
    TOK_IDENT,
    TOK_NEWLINE,
    TOK_INT, TOK_FLOAT, TOK_STRING,
    TOK_SEMI,
    TOK_LPAREN, TOK_RPAREN,
    TOK_LBRACE, TOK_RBRACE,
    TOK_LBRACKET, TOK_RBRACKET,
    TOK_COMMA, TOK_COLON, TOK_DOT, TOK_ELLIPSIS, TOK_QUESTION,
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_SLASHSLASH, TOK_PERCENT,
    TOK_INC, TOK_DEC,
    TOK_ASSIGN,
    TOK_PLUSASSIGN, TOK_MINUSASSIGN, TOK_STARASSIGN, TOK_SLASHASSIGN,
    TOK_SLASHSLASHASSIGN, TOK_PERCENTASSIGN,
    TOK_AMPASSIGN, TOK_PIPEASSIGN, TOK_CARETASSIGN,
    TOK_SHLASSIGN, TOK_SHRASSIGN, TOK_USHRASSIGN,
    TOK_AMPAMPASSIGN, TOK_PIPEPIPEASSIGN,
    TOK_EQEQ, TOK_NOTEQ, TOK_LT, TOK_LE, TOK_GT, TOK_GE,
    TOK_SHL, TOK_SHR, TOK_USHR,
    TOK_AMP, TOK_PIPE, TOK_CARET, TOK_TILDE,
    TOK_AMPAMP, TOK_PIPEPIPE,
    TOK_BANG
};

#endif /* STEAK_TOKENS_H */
