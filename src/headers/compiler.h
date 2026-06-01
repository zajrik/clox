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

typedef struct Local {
  Token identifier;
  int depth;
} Local;

typedef struct Compiler {
  Local locals[UINT8_COUNT];
  int localCount;
  int scopeDepth;
} Compiler;

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

/// Represents a pointer to a void function accepting a single `bool` argument,
/// to be used for parsing an expression grammar production from source tokens.
typedef void (*ExprFn)(bool);

/// Represents a pointer to a void function accepting no arguments, to be used
/// for parsing a statement grammar production from source tokens.
typedef void (*StmtFn)();

/// Represents a single row in the parser rule table.
typedef struct ParseRule {
  /// Pointer to a prefix-position parse function.
  ExprFn prefix;

  /// Pointer to an infix-position parse function.
  ExprFn infix;

  /// The expression precedence with which to apply this rule when parsing.
  Precedence precedence;
} ParseRule;

bool compile(const char* source, Chunk* chunk);

static void endCompilation();

static void advance();
static void consume(TokenType type, const char* msg);
static TokenType currentType();
static bool currentIs(TokenType type);
static TokenType nextType();
static bool nextIs(TokenType type);
static bool nextMatches(TokenType type);
static bool identifiersEqual(const Token* a, const Token* b);

static void pushScope();
static void popScope();
static void scope(StmtFn rule);

static void declaration();
static void variableDeclaration();
static void statement();
static void expressionStatement();
static void printStatement();
static void ifStatement();
static void block();

static void expr(Precedence precedence);

static void expression();
static void unary(bool);
static void binary(bool);
static void and(bool);
static void or(bool);
static void literal(bool);
static void number(bool);
static void string(bool);
static void variable(bool canAssign);
static void namedVariable(Token token, bool canAssign);
static void grouping(bool);

static ParseRule* getRule(TokenType type);

static uint8_t parseVariableIdent(const char* expect);
static void defineVariable(uint8_t identConstOffset);
static void declareLocal();
static int resolveLocal(const Compiler* cmp, const Token* identifier);
static void addLocal(Token identifier);
static void markDefined();

static uint8_t makeConstant(Value value);
static uint8_t makeIdentConstant(const Token* token);
static void emitByte(uint8_t byte);
static void emitBytes(uint8_t a, uint8_t b);
static void emitConstant(Value value);
static void emitReturn();
static int emitJump(OpCode opcode);
static void patchJump(int offset);

static void errorAtNext(const char* msg);
static void error(const char* msg);
static void synchronize();

#endif
