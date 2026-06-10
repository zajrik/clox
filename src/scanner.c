#include <stdbool.h>
#include <string.h>

#include "scanner.h"
#include "common.h"

/// Global token scanner instance.
Scanner scanner;

/// Initialize the global token scanner with the given [source] text.
void initScanner(const char* source) {
  scanner.start = source;
  scanner.current = source;
  scanner.line = 1;
}

/// Returns whether the scanner has reached the end of the loaded source text.
static bool isAtEnd() {
  return *scanner.current == '\0';
}

/// Advance the scanner by one character, consuming and returning the scanned char.
char advance() {
  return *scanner.current++;
}

/// Peek at the next character to be scanned.
static char peek() {
  return *scanner.current;
}

/// Peek at the character *after* the next character to be scanned.
static char peekNext() {
  if (isAtEnd()) return '\0';
  return *(scanner.current + 1);

  // This should be the same as the above, right?
  // return scanner.current[1];
}

/// Returns whether the next character matches the given [expected] char.
///
/// If the character matched, it will be consumed.
static bool match(const char expected) {
  if (isAtEnd()) return false;
  if (peek() != expected) return false;

  advance();
  return true;
}

/// Returns whether the given [character] is a numerical digit.
static bool isDigit(const char character) {
  return character >= '0' && character <= '9';
}

/// Returns whether the given [character] is an alphabetical character, or `_`.
static bool isAlpha(const char character) {
  return (character >= 'a' && character <= 'z')
    || (character >= 'A' && character <= 'Z')
    || character == '_';
}

/// Returns whether the given [character] is an alphanumeric character.
static bool isAlphaNumeric(const char character) {
  return isAlpha(character) || isDigit(character);
}

/// Advances the scanner until we see a non-whitespace character.
static void skipWhitespace() {
  loop {
    switch (peek()) {
      case ' ':
      case '\r':
      case '\t':
        advance();
        break;

      case '\n':
        advance();
        scanner.line++;
        break;

      // Skip comments (or return on single slash)
      case '/':
        if (peekNext() == '/') {
          while (peek() != '\n' && !isAtEnd()) advance();
        } else {
          return;
        }
        break;

      default: return;
    }
  }
}

/// Determines if the currently-scanned identifier is a keyword.
///
/// Returns the keyword type if so, otherwise returns [TOKEN_IDENTIFIER].
static TokenType checkKeyword(
  const int start,
  const int length,
  const char* rest,
  const TokenType type
) {
  const int tokenLength = (int)(scanner.current - scanner.start);
  const int keywordLength = start + length;

  if (tokenLength != keywordLength) return TOKEN_IDENTIFIER;

  // Compare remainder of token string
  return memcmp(scanner.start + start, rest, length) == 0 ? type : TOKEN_IDENTIFIER;
}

/// Determines the token type of the currently-scanned identifier.
static TokenType identifierType() {
  const int tokenLength = (int)(scanner.current - scanner.start);

  switch (*scanner.start) {
    case 'a': return checkKeyword(1, 2, "nd", TOKEN_AND);

    case 'c':
      if (tokenLength > 1) {
        switch (*(scanner.start + 1)) {
          case 'l': return checkKeyword(2, 3, "ass", TOKEN_CLASS);
          case 'a': return checkKeyword(2, 2, "se", TOKEN_CASE);
          default: break;
        }
      }
      break;

    case 'd': return checkKeyword(1, 6, "efault", TOKEN_DEFAULT);
    case 'e': return checkKeyword(1, 3, "lse", TOKEN_ELSE);
    case 'i': return checkKeyword(1, 1, "f", TOKEN_IF);

    case 'f':
      if (tokenLength > 1) {
        switch (*(scanner.start + 1)) {
          case 'a': return checkKeyword(2, 3, "lse", TOKEN_FALSE);
          case 'o': return checkKeyword(2, 1, "r", TOKEN_FOR);
          case 'u': return checkKeyword(2, 1, "n", TOKEN_FUN);
          default: break;
        }
      }
      break;

    case 'n': return checkKeyword(1, 2, "il", TOKEN_NIL);
    case 'o': return checkKeyword(1, 1, "r", TOKEN_OR);
    case 'p': return checkKeyword(1, 4, "rint", TOKEN_PRINT);
    case 'r': return checkKeyword(1, 5, "eturn", TOKEN_RETURN);

    case 's':
      if (tokenLength > 1) {
        switch (*(scanner.start + 1)) {
          case 'u': return checkKeyword(2, 3, "per", TOKEN_SUPER);
          case 'w': return checkKeyword(2, 4, "itch", TOKEN_SWITCH);
          default: break;
        }
      }
      break;

    case 't':
      if (tokenLength > 1) {
        switch (*(scanner.start + 1)) {
          case 'h': return checkKeyword(2, 2, "is", TOKEN_THIS);
          case 'r': return checkKeyword(2, 2, "ue", TOKEN_TRUE);
          default: break;
        }
      }
      break;

    case 'v': return checkKeyword(1, 2, "ar", TOKEN_VAR);
    case 'w': return checkKeyword(1, 4, "hile", TOKEN_WHILE);
    default: break;
  }

  return TOKEN_IDENTIFIER;
}

/// Consumes an identifier token from source.
static Token identifier() {
  while (isAlphaNumeric(peek())) advance();
  return makeToken(identifierType());
}

/// Consumes a numerical token from source.
static Token number() {
  while (isDigit(peek())) advance();

  if (peek() == '.' && isDigit(peekNext())) {
    advance();
    while (isDigit(peek())) advance();
  }

  return makeToken(TOKEN_NUMBER);
}

/// Consumes a string literal token from source.
static Token string() {
  while (peek() != '"' && !isAtEnd()) {
    if (peek() == '\n') scanner.line++;
    advance();
  }

  if (isAtEnd()) return errorToken("Unterminated string.");

  // Skip closing quote
  advance();

  return makeToken(TOKEN_STRING);
}

/// Scans the next token from the source text loaded into the scanner.
Token scanToken() {
  skipWhitespace();

  scanner.start = scanner.current;

  if (isAtEnd()) return makeToken(TOKEN_EOF);

  const char c = advance();

  if (isAlpha(c)) return identifier();
  if (isDigit(c)) return number();

  switch (c) {
    case '(': return makeToken(TOKEN_LEFT_PAREN);
    case ')': return makeToken(TOKEN_RIGHT_PAREN);
    case '{': return makeToken(TOKEN_LEFT_BRACE);
    case '}': return makeToken(TOKEN_RIGHT_BRACE);
    case ':': return makeToken(TOKEN_COLON);
    case ';': return makeToken(TOKEN_SEMICOLON);
    case ',': return makeToken(TOKEN_COMMA);
    case '.': return makeToken(TOKEN_DOT);

    case '*': return makeToken(match('=') ? TOKEN_STAR_EQUAL : TOKEN_STAR);
    case '/': return makeToken(match('=') ? TOKEN_SLASH_EQUAL : TOKEN_SLASH);
    case '-': return makeToken(match('=') ? TOKEN_MINUS_EQUAL : TOKEN_MINUS);
    case '+': return makeToken(match('=') ? TOKEN_PLUS_EQUAL : TOKEN_PLUS);
    case '?':
      return makeToken(
        match('?')
          ? (match('=') ? TOKEN_NILISH_EQUAL : TOKEN_NILISH)
          : TOKEN_QUESTION
      );

    case '!': return makeToken(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
    case '<': return makeToken(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
    case '>': return makeToken(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
    case '=':
      switch (peek()) {
        case '=': return advance(), makeToken(TOKEN_EQUAL_EQUAL);
        case '>': return advance(), makeToken(TOKEN_ARROW);
        default: return makeToken(TOKEN_EQUAL);
      }

    case '"': return string();

    default: break;
  }

  return errorToken("Unexpected character");
}

/// Create and return a token with token type [type].
Token makeToken(const TokenType type) {
  Token token;
  token.type = type;
  token.start = scanner.start;
  token.length = (int)(scanner.current - scanner.start);
  token.line = scanner.line;
  return token;
}

/// Create and return an error token with the given [err].
///
/// The token's lexeme will be the given error string.
Token errorToken(const char* err) {
  Token token;
  token.type = TOKEN_ERROR;
  token.start = err;
  token.length = (int)strlen(err);
  token.line = scanner.line;
  return token;
}
