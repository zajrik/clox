#ifndef CLOX_SCANNER_H
#define CLOX_SCANNER_H

/// Holds data for scanning tokens from source text.
typedef struct Scanner {
  /// Pointer to the beginning of the lexeme being scanned.
  const char* start;

  /// Points to the current character being scanned.
  const char* current;

  /// Tracks the current line number within the source text.
  int line;
} Scanner;

void initScanner(const char* source);

/// Represents types of tokens found in lox source text.
typedef enum TokenType {
  // Single-character tokens
  TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,
  TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
  TOKEN_COMMA, TOKEN_DOT, TOKEN_MINUS, TOKEN_PLUS,
  TOKEN_SLASH, TOKEN_STAR,
  TOKEN_COLON, TOKEN_SEMICOLON,

  // One or two character tokens
  TOKEN_BANG, TOKEN_BANG_EQUAL,
  TOKEN_EQUAL, TOKEN_EQUAL_EQUAL,
  TOKEN_GREATER, TOKEN_GREATER_EQUAL,
  TOKEN_LESS, TOKEN_LESS_EQUAL,

  // Literals
  TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_NUMBER,

  // Keywords
  TOKEN_AND, TOKEN_CASE, TOKEN_CLASS, TOKEN_DEFAULT, TOKEN_ELSE,
  TOKEN_FALSE, TOKEN_FOR, TOKEN_FUN, TOKEN_IF, TOKEN_NIL, TOKEN_OR,
  TOKEN_PRINT, TOKEN_RETURN, TOKEN_SUPER, TOKEN_SWITCH,
  TOKEN_THIS, TOKEN_TRUE, TOKEN_VAR, TOKEN_WHILE,

  TOKEN_ERROR, TOKEN_EOF
} TokenType;

/// A token scanned from lox source text.
///
/// The token serves as a lens to its lexeme within the source text from
/// [start] to [length].
typedef struct Token {
  /// The type of this token.
  TokenType type;

  /// Pointer to the start of this token's lexeme in the source text.
  const char* start;

  /// The length of this token's lexeme.
  int length;

  /// The line in the source text on which this token is found.
  int line;
} Token;

Token scanToken();
Token makeToken(TokenType type);
Token errorToken(const char* err);

#endif
