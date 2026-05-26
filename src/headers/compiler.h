#ifndef CLOX_COMPILER_H
#define CLOX_COMPILER_H

#include "headers/chunk.h"
#include "headers/scanner.h"

/// Holds data for parsing scanned tokens and compiling into lox bytecode.
typedef struct Parser {
  /// The next token to be parsed/compiled.
  Token next;

  /// The token currently being parsed/compiled.
  Token current;

  /// Whether we encountered an error while parsing.
  bool hadError;

  /// Whether we are in panic mode while parsing.
  bool panicMode;
} Parser;

/// Indicates the level of precedence with which to parse and compile an expression.
///
/// Enum member order is significant here and reflects the actual grammar rule
/// precedence.
typedef enum Precedence {
  PREC_NONE,
  PREC_ASSIGNMENT,
  PREC_OR,
  PREC_AND,
  PREC_EQUALITY,
  PREC_COMPARISON,
  PREC_TERM,
  PREC_FACTOR,
  PREC_UNARY,
  PREC_CALL,
  PREC_PRIMARY,
} Precedence;

/// Represents a pointer to a void function accepting no arguments, to be used
/// for parsing a grammar production from source tokens.
typedef void (*ParseFn)();

/// Represents a single row in the parser rule table.
typedef struct ParseRule {
  /// Pointer to a prefix-position parse function.
  ParseFn prefix;

  /// Pointer to an infix-position parse function.
  ParseFn infix;

  /// The expression precedence with which to apply this rule when parsing.
  Precedence precedence;
} ParseRule;

bool compile(const char* source, Chunk* chunk);

static void endCompilation();

static void advance();
static void consume(TokenType type, const char* msg);

static uint8_t makeConstant(Value value);

static void expr(Precedence precedence);

static void expression();
static void literal();
static void number();
static void unary();
static void binary();
static void grouping();

static ParseRule* getRule(TokenType type);

static void emitByte(uint8_t byte);
static void emitBytes(uint8_t a, uint8_t b);
static void emitConstant(Value value);
static void emitReturn();

static void errorAtNext(const char* msg);
static void error(const char* msg);

#endif
