#include "headers/compiler.h"
#include "headers/chunk.h"
#include "headers/common.h"
#include "headers/object.h"
#include "headers/scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "headers/debug.h"
#endif

/// Global parser instance.
Parser parser;

/// Stores a pointer to the currently compiling chunk.
Chunk* compilingChunk;

/// Returns a pointer to the currently compiling chunk.
static Chunk* currentChunk() {
  return compilingChunk;
}

/// Compile the given [source] text into bytecode instructions, storing them in
/// the given [chunk].
///
/// Returns whether compilation was successful.
bool compile(const char* source, Chunk* chunk) {
  initScanner(source);
  compilingChunk = chunk;

  parser.hadError = false;
  parser.panicMode = false;

  // Scan first token into [Parser.next]
  advance();

  while (!nextMatches(TOKEN_EOF)) {
    declaration();
  }

  endCompilation();

  return !parser.hadError;
}

/// End the compilation process.
static void endCompilation() {
  emitReturn();

  #ifdef DEBUG_PRINT_CODE
  if (!parser.hadError) {
    disassembleChunk(currentChunk(), "code");
  } else {
    printf("errors occurred\n");
  }
  #endif
}

/// Advance the parser, consuming the current token and scanning the next token.
static void advance() {
  parser.current = parser.next;

  loop {
    parser.next = scanToken();
    if (!nextIs(TOKEN_ERROR)) break;

    errorAtNext(parser.next.start);
  }
}

/// Returns the [TokenType] of the current token being parsed/compiled.
static TokenType currentType() {
  return parser.current.type;
}

/// Returns whether the current token being parsed/compiled is of [type].
static bool currentIs(const TokenType type) {
  return currentType() == type;
}

/// Returns the [TokenType] of the next token to be parsed/compiled.
static TokenType nextType() {
  return parser.next.type;
}

/// Returns whether the next token to be parsed/compiled is of [type].
static bool nextIs(const TokenType type) {
  return nextType() == type;
}

/// Returns whether the next token to be parsed/compiled is of [type]. If `true`,
/// the token will be consumed.
static bool nextMatches(const TokenType type) {
  if (!nextIs(type)) return false;
  return advance(), true;
}

/// Consume a token of the given [type] from scanned tokens.
///
/// Emits an error [msg] if the token type is not consumed.
static void consume(const TokenType type, const char* msg) {
  if (nextIs(type)) return advance();
  errorAtNext(msg);
}

/// Parses a variable identifier, returning the offset to the variable address.
///
/// If the variable identifier cannot be parsed, errors with message [expect].
static uint8_t parseVariable(const char* expect) {
  consume(TOKEN_IDENTIFIER, expect);
  return identifierConstant(&parser.current);
}

/// Emit bytecode to define a global variable.
///
/// The given [global] constant value index will be emitted as the operand.
static void defineVariable(const uint8_t global) {
  emitBytes(OP_DEFINE_GLOBAL, global);
}

// Statement parse/compilation functions ======================================

/// Parses/compiles a declaration statement from scanned tokens.
///
/// declaration := varDecl | statement ;
static void declaration() {
  if (nextMatches(TOKEN_VAR)) {
    variableDeclaration();
  } else {
    statement();
  }
  if (parser.panicMode) synchronize();
}

/// Parses/compiles a variable declaration statement from scanned tokens.
static void variableDeclaration() {
  const uint8_t global = parseVariable("Expected variable name.");

  nextMatches(TOKEN_EQUAL) ? expression() : emitByte(OP_NIL);
  consume(TOKEN_SEMICOLON, "Expected ';' after variable declaration.");

  defineVariable(global);
}

/// Parses/compiles a statement from scanned tokens.
///
/// statement := exprStmt | printStmt ;
static void statement() {
  if (nextMatches(TOKEN_PRINT)) return printStatement();
  expressionStatement();
}

/// Parses/compiles a print statement from scanned tokens.
///
/// printStmt := "print" exprStmt ;
static void printStatement() {
  expression();
  consume(TOKEN_SEMICOLON, "Expected ';' after value.");
  emitByte(OP_PRINT);
}

/// Parses/compiles an expression statement from scanned tokens.
///
/// exprStmt := expression ";" ;
static void expressionStatement() {
  expression();
  consume(TOKEN_SEMICOLON, "Expected ';' after expression.");
  emitByte(OP_POP);
}

// Expression parse/compilation functions =====================================

/// Parses an expression from scanned tokens following the given [precedence],
/// emitting bytecode for all operations in the parsed expression.
static void expr(const Precedence precedence) {
  advance();

  // Check if this expression is allowed to be treated as an assignment expression
  const bool canAssign = precedence <= PREC_ASSIGNMENT;

  // Start with a prefix rule to parse/compile the first expression. The first token
  // will always start a prefix expression. It may end up nested as an operand inside
  // one or more infix expressions, but it always starts with a prefix expression.
  //
  // 1 + 1;   // number is a prefix rule
  // foo + 1; // identifier is a prefix rule
  // (1 + 1); // grouping is a prefix rule
  // -1 + 1); // unary minus is a prefix rule

  const ParseFn prefixRule = getRule(currentType())->prefix;
  if (prefixRule == NULL) return error("Expected expression.");
  prefixRule(canAssign);

  // If the precedence of the next token is too low, we stop here. This indicates
  // that it is impossible for the next token to be an infix operator that can
  // continue the expression because that would violate the order of operations
  // dictated by the precedence rules.
  //
  // Otherwise, we continue parsing infix expressions until hitting a token with
  // too low precedence.

  while (precedence <= getRule(nextType())->precedence) {
    advance();
    getRule(currentType())->infix(canAssign);
  }

  // TOKEN_EQUAL should have been consumed by the variable prefix rule at this point
  // if assignment was allowed so we should error if we see `=`
  if (canAssign && nextMatches(TOKEN_EQUAL)) {
    error("Invalid assignment target.");
  }
}

/// Parses an expression, emitting compiled expression bytecode.
static void expression() {
  expr(PREC_ASSIGNMENT);
}

/// Parses a unary expression, emitting bytecode for the operand expression
/// and operator.
static void unary(const bool _) {
  const TokenType operatorType = currentType();

  // Compile the operand expression
  expr(PREC_UNARY);

  // Emit the operator opcode
  switch (operatorType) {
    case TOKEN_MINUS: return emitByte(OP_NEGATE);
    case TOKEN_BANG: return emitByte(OP_NOT);
    default: break;
  }
}

/// Parses a binary expression, emitting bytecode for the operand expressions
/// and the operator.
///
/// The left-hand operand of the binary expression has already been compiled to
/// the chunk at this point.
static void binary(const bool _) {
  const TokenType operatorType = currentType();
  const ParseRule* operatorRule = getRule(operatorType);

  // Compile the right-hand operand. A binary operator's right-hand operand
  // precedence is always one level higher than the operator's own precedence
  expr(operatorRule->precedence + 1);

  switch (operatorType) {
    case TOKEN_EQUAL_EQUAL: return emitByte(OP_EQUAL);
    case TOKEN_BANG_EQUAL: return emitBytes(OP_EQUAL, OP_NOT);
    case TOKEN_GREATER: return emitByte(OP_GREATER);
    case TOKEN_GREATER_EQUAL: return emitBytes(OP_GREATER, OP_NOT);
    case TOKEN_LESS: return emitByte(OP_LESS);
    case TOKEN_LESS_EQUAL: return emitBytes(OP_LESS, OP_NOT);
    case TOKEN_PLUS: return emitByte(OP_ADD);
    case TOKEN_MINUS: return emitByte(OP_SUBTRACT);
    case TOKEN_STAR: return emitByte(OP_MULTIPLY);
    case TOKEN_SLASH: return emitByte(OP_DIVIDE);

    default: break;
  }
}

/// Parses a keyword-literal expression, emitting the relevant opcode.
static void literal(const bool _) {
  switch (currentType()) {
    case TOKEN_NIL: return emitByte(OP_NIL);
    case TOKEN_TRUE: return emitByte(OP_TRUE);
    case TOKEN_FALSE: return emitByte(OP_FALSE);
    default: break;
  }
}

/// Parses a number constant, emitting bytecode for a constant number value.
static void number(const bool _) {
  const double value = strtod(parser.current.start, NULL);
  emitConstant(NUMBER_VAL(value));
}

/// Parses a string constant, emitting bytecode for a constant string value.
static void string(const bool _) {
  const char* stringStart = parser.current.start + 1;
  const int stringLength = parser.current.length - 2;
  emitConstant(OBJ_VAL(copyString(stringStart, stringLength)));
}

/// Parses/compiles a variable/identifier expression.
static void variable(const bool canAssign) {
  namedVariable(parser.current, canAssign);
}

/// Parses/compiles a variable/identifier get/set expression, emitting bytecode
/// to fetch/set the value for that variable/identifier.
static void namedVariable(const Token token, const bool canAssign) {
  const uint8_t identOffset = identifierConstant(&token);

  if (canAssign && nextMatches(TOKEN_EQUAL)) {
    expression();
    return emitBytes(OP_SET_GLOBAL, identOffset);
  }

  emitBytes(OP_GET_GLOBAL, identOffset);
}

/// Parses a grouping expression, emitting bytecode for the grouped expression.
static void grouping(const bool _) {
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expected ')' after expression");
}

// @formatter:off
/// Table holding token parse rules.
static ParseRule rules[] = {
  // Single character token rules
  // [TokenType]        = {prefix,     infix,  Precedence      },
  [TOKEN_LEFT_PAREN]    = {grouping,   NULL,   PREC_NONE       },
  [TOKEN_RIGHT_PAREN]   = {NULL,       NULL,   PREC_NONE       },
  [TOKEN_LEFT_BRACE]    = {NULL,       NULL,   PREC_NONE       },
  [TOKEN_RIGHT_BRACE]   = {NULL,       NULL,   PREC_NONE       },
  [TOKEN_COMMA]         = {NULL,       NULL,   PREC_NONE       },
  [TOKEN_DOT]           = {NULL,       NULL,   PREC_NONE       },
  [TOKEN_SEMICOLON]     = {NULL,       NULL,   PREC_NONE       },

  // One/two character token rules
  [TOKEN_MINUS]         = {unary,      binary, PREC_TERM       },
  [TOKEN_PLUS]          = {NULL,       binary, PREC_TERM       },
  [TOKEN_SLASH]         = {NULL,       binary, PREC_FACTOR     },
  [TOKEN_STAR]          = {NULL,       binary, PREC_FACTOR     },

  [TOKEN_BANG]          = {unary,      NULL,   PREC_NONE       },
  [TOKEN_BANG_EQUAL]    = {NULL,       binary, PREC_EQUALITY   },
  [TOKEN_EQUAL]         = {NULL,       NULL,   PREC_NONE       },
  [TOKEN_EQUAL_EQUAL]   = {NULL,       binary, PREC_EQUALITY   },
  [TOKEN_GREATER]       = {NULL,       binary, PREC_COMPARISON },
  [TOKEN_GREATER_EQUAL] = {NULL,       binary, PREC_COMPARISON },
  [TOKEN_LESS]          = {NULL,       binary, PREC_COMPARISON },
  [TOKEN_LESS_EQUAL]    = {NULL,       binary, PREC_COMPARISON },

  // Literal token rules
  [TOKEN_IDENTIFIER]    = {variable,   NULL,   PREC_NONE       },
  [TOKEN_STRING]        = {string,     NULL,   PREC_NONE       },
  [TOKEN_NUMBER]        = {number,     NULL,   PREC_NONE       },

  // Keyword token rules
  [TOKEN_AND]           = {NULL,       NULL,   PREC_NONE       },
  [TOKEN_CLASS]         = {NULL,       NULL,   PREC_NONE       },
  [TOKEN_ELSE]          = {NULL,       NULL,   PREC_NONE       },
  [TOKEN_FALSE]         = {literal,    NULL,   PREC_NONE       },
  [TOKEN_FOR]           = {NULL,       NULL,   PREC_NONE       },
  [TOKEN_FUN]           = {NULL,       NULL,   PREC_NONE       },
  [TOKEN_IF]            = {NULL,       NULL,   PREC_NONE       },
  [TOKEN_NIL]           = {literal,    NULL,   PREC_NONE       },
  [TOKEN_OR]            = {NULL,       NULL,   PREC_NONE       },
  [TOKEN_PRINT]         = {NULL,       NULL,   PREC_NONE       },
  [TOKEN_RETURN]        = {NULL,       NULL,   PREC_NONE       },
  [TOKEN_SUPER]         = {NULL,       NULL,   PREC_NONE       },
  [TOKEN_THIS]          = {NULL,       NULL,   PREC_NONE       },
  [TOKEN_TRUE]          = {literal,    NULL,   PREC_NONE       },
  [TOKEN_VAR]           = {NULL,       NULL,   PREC_NONE       },
  [TOKEN_WHILE]         = {NULL,       NULL,   PREC_NONE       },

  // Misc token rules
  [TOKEN_ERROR]         = {NULL,       NULL,   PREC_NONE       },
  [TOKEN_EOF]           = {NULL,       NULL,   PREC_NONE       },
};
// @formatter:on

/// Returns the [ParseRule] for the given [type].
static ParseRule* getRule(const TokenType type) {
  return &rules[type];
}

// Chunk writing ==============================================================

/// Add [value] to the constants in the chunk currently being compiled.
///
/// Returns the offset of the added constant.
static uint8_t makeConstant(const Value value) {
  const int offset = addConstant(currentChunk(), value);

  if (offset > UINT8_MAX) {
    error("Too many constants in one chunk.");
    return 0;
  }

  return offset;
}

/// Push an identifier to the constants array, returning the offset of the constant.
static uint8_t identifierConstant(const Token* token) {
  return makeConstant(OBJ_VAL(copyString(token->start, token->length)));
}

/// Write the given [byte] to the chunk currently being compiled.
static void emitByte(const uint8_t byte) {
  writeChunk(currentChunk(), byte, parser.current.line);
}

/// Write the given bytes ([a] and [b]) to the chunk currently being compiled.
static void emitBytes(const uint8_t a, const uint8_t b) {
  emitByte(a);
  emitByte(b);
}

/// Write [OP_CONSTANT] and the constant offset for [value] to the chunk currently
/// being compiled.
static void emitConstant(const Value value) {
  emitBytes(OP_CONSTANT, makeConstant(value));
}

/// Write [OP_RETURN] to the chunk currently being compiled.
static void emitReturn() {
  emitByte(OP_RETURN);
}

// Error management ===========================================================

/// Output error [msg] for the [Token] at address [token].
static void errorAt(const Token* token, const char* msg) {
  if (parser.panicMode) return;
  parser.panicMode = true;
  parser.hadError = true;

  fprintf(stderr, "[line %d] Error", token->line);

  if (token->type == TOKEN_EOF) {
    fprintf(stderr, " at end");
  } else if (token->type == TOKEN_ERROR) {
    // Do nothing
  } else {
    fprintf(stderr, " at '%.*s'", token->length, token->start);
  }

  fprintf(stderr, ": %s\n", msg);
}

/// Output error [msg] at the next scanned token to be parsed.
static void errorAtNext(const char* msg) {
  errorAt(&parser.next, msg);
}

/// Output error [msg] at the current scanned token being parsed.
static void error(const char* msg) {
  errorAt(&parser.current, msg);
}

/// Synchronize parser to the next valid statement after encountering an error.
static void synchronize() {
  parser.panicMode = false;

  while (!nextIs(TOKEN_EOF)) {
    if (currentIs(TOKEN_SEMICOLON)) return;

    switch (nextType()) {
      case TOKEN_CLASS:
      case TOKEN_FUN:
      case TOKEN_VAR:
      case TOKEN_FOR:
      case TOKEN_IF:
      case TOKEN_WHILE:
      case TOKEN_PRINT:
      case TOKEN_RETURN:
        return;

      default: break;
    }

    advance();
  }
}
