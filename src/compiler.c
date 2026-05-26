#include "headers/compiler.h"
#include "headers/object.h"
#include "headers/chunk.h"
#include "headers/common.h"
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

  expression();
  consume(TOKEN_EOF, "Expected end of expression.");

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

  for (;;) {
    parser.next = scanToken();
    if (parser.next.type != TOKEN_ERROR) break;

    errorAtNext(parser.next.start);
  }
}

/// Consume a token of the given [type] from scanned tokens.
///
/// Emits an error [msg] if the token type is not consumed.
static void consume(TokenType type, const char* msg) {
  if (parser.next.type == type) {
    advance();
    return;
  }

  errorAtNext(msg);
}

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

/// Parses an expression from scanned tokens following the given [precedence],
/// emitting bytecode for all operations in the parsed expression.
static void expr(Precedence precedence) {
  advance();

  // Start with a prefix rule to parse/compile the first expression. The first token
  // will always start a prefix expression. It may end up nested as an operand inside
  // one or more infix expressions, but it always starts with a prefix expression.
  //
  // 1 + 1;   // number is a prefix rule
  // foo + 1; // identifier is a prefix rule
  // (1 + 1); // grouping is a prefix rule
  // -1 + 1); // unary minus is a prefix rule

  const ParseFn prefixRule = getRule(parser.current.type)->prefix;
  if (prefixRule == NULL) {
    error("Expected expression.");
    return;
  }
  prefixRule();

  // If the precedence of the next token is too low, we stop here. This indicates
  // that it is impossible for the next token to be an infix operator that can
  // continue the expression because that would violate the order of operations
  // dictated by the precedence rules.
  //
  // Otherwise, we continue parsing infix expressions until hitting a token with
  // too low precedence.

  while (precedence <= getRule(parser.next.type)->precedence) {
    advance();
    getRule(parser.current.type)->infix();
  }
}

/// Parses an expression, emitting compiled expression bytecode.
static void expression() {
  expr(PREC_ASSIGNMENT);
}

/// Parses a keyword-literal expression, emitting the relevant opcode.
static void literal() {
  switch (parser.current.type) {
    case TOKEN_NIL:
      emitByte(OP_NIL);
      break;

    case TOKEN_TRUE:
      emitByte(OP_TRUE);
      break;

    case TOKEN_FALSE:
      emitByte(OP_FALSE);
      break;

    default: return;
  }
}

/// Parses a string constant, emitting bytecode for a constant string value.
static void string() {
  const char* stringStart = parser.current.start + 1;
  const int stringLength = parser.current.length - 2;
  emitConstant(OBJ_VAL(copyString(stringStart, stringLength)));
}

/// Parses a number constant, emitting bytecode for a constant number value.
static void number() {
  const double value = strtod(parser.current.start, NULL);
  emitConstant(NUMBER_VAL(value));
}

/// Parses a unary expression, emitting bytecode for the operand expression
/// and operator.
static void unary() {
  const TokenType operatorType = parser.current.type;

  // Compile the operand expression
  expr(PREC_UNARY);

  // Emit the operator opcode
  switch (operatorType) {
    case TOKEN_MINUS:
      emitByte(OP_NEGATE);
      break;

    case TOKEN_BANG:
      emitByte(OP_NOT);
      break;

    default: return;
  }
}

/// Parses a binary expression, emitting bytecode for the operand expressions
/// and the operator.
///
/// The left-hand operand of the binary expression has already been compiled to
/// the chunk at this point.
static void binary() {
  const TokenType operatorType = parser.current.type;
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

    default: return;
  }
}

/// Parses a grouping expression, emitting bytecode for the grouped expression.
static void grouping() {
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expected ')' after expression");
}

// @formatter:off
/// Table holding token parse rules.
static ParseRule rules[] = {
  // Single character token rules
  // [TokenType]        = {prefix,   infix,  Precedence      },
  [TOKEN_LEFT_PAREN]    = {grouping, NULL,   PREC_NONE       },
  [TOKEN_RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE       },
  [TOKEN_LEFT_BRACE]    = {NULL,     NULL,   PREC_NONE       },
  [TOKEN_RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE       },
  [TOKEN_COMMA]         = {NULL,     NULL,   PREC_NONE       },
  [TOKEN_DOT]           = {NULL,     NULL,   PREC_NONE       },
  [TOKEN_SEMICOLON]     = {NULL,     NULL,   PREC_NONE       },

  // One/two character token rules
  [TOKEN_MINUS]         = {unary,    binary, PREC_TERM       },
  [TOKEN_PLUS]          = {NULL,     binary, PREC_TERM       },
  [TOKEN_SLASH]         = {NULL,     binary, PREC_FACTOR     },
  [TOKEN_STAR]          = {NULL,     binary, PREC_FACTOR     },

  [TOKEN_BANG]          = {unary,    NULL,   PREC_NONE       },
  [TOKEN_BANG_EQUAL]    = {NULL,     binary, PREC_EQUALITY   },
  [TOKEN_EQUAL]         = {NULL,     NULL,   PREC_NONE       },
  [TOKEN_EQUAL_EQUAL]   = {NULL,     binary, PREC_EQUALITY   },
  [TOKEN_GREATER]       = {NULL,     binary, PREC_COMPARISON },
  [TOKEN_GREATER_EQUAL] = {NULL,     binary, PREC_COMPARISON },
  [TOKEN_LESS]          = {NULL,     binary, PREC_COMPARISON },
  [TOKEN_LESS_EQUAL]    = {NULL,     binary, PREC_COMPARISON },

  // Literal token rules
  [TOKEN_IDENTIFIER]    = {NULL,     NULL,   PREC_NONE       },
  [TOKEN_STRING]        = {string,   NULL,   PREC_NONE       },
  [TOKEN_NUMBER]        = {number,   NULL,   PREC_NONE       },

  // Keyword token rules
  [TOKEN_AND]           = {NULL,     NULL,   PREC_NONE       },
  [TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE       },
  [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE       },
  [TOKEN_FALSE]         = {literal,  NULL,   PREC_NONE       },
  [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE       },
  [TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE       },
  [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE       },
  [TOKEN_NIL]           = {literal,  NULL,   PREC_NONE       },
  [TOKEN_OR]            = {NULL,     NULL,   PREC_NONE       },
  [TOKEN_PRINT]         = {NULL,     NULL,   PREC_NONE       },
  [TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE       },
  [TOKEN_SUPER]         = {NULL,     NULL,   PREC_NONE       },
  [TOKEN_THIS]          = {NULL,     NULL,   PREC_NONE       },
  [TOKEN_TRUE]          = {literal,  NULL,   PREC_NONE       },
  [TOKEN_VAR]           = {NULL,     NULL,   PREC_NONE       },
  [TOKEN_WHILE]         = {NULL,     NULL,   PREC_NONE       },

  // Misc token rules
  [TOKEN_ERROR]         = {NULL,     NULL,   PREC_NONE       },
  [TOKEN_EOF]           = {NULL,     NULL,   PREC_NONE       },
};
// @formatter:on

/// Returns the [ParseRule] for the given [type].
static ParseRule* getRule(TokenType type) {
  return &rules[type];
}

// Chunk writing ==============================================================

/// Write the given [byte] to the chunk currently being compiled.
static void emitByte(uint8_t byte) {
  writeChunk(currentChunk(), byte, parser.current.line);
}

/// Write the given bytes ([a] and [b]) to the chunk currently being compiled.
static void emitBytes(uint8_t a, uint8_t b) {
  emitByte(a);
  emitByte(b);
}

/// Write [OP_CONSTANT] and the constant offset for [value] to the chunk currently
/// being compiled.
static void emitConstant(Value value) {
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
