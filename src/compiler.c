#include "headers/compiler.h"

#include <string.h>

#include "headers/chunk.h"
#include "headers/common.h"
#include "headers/object.h"
#include "headers/scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "headers/debug.h"
#endif

/// Global parser instance.
Parser parser;

/// Pointer to the current compiler (?)
Compiler* compiler = NULL;

/// Initialize the given [compiler] instance.
static void initCompiler(Compiler* cmp) {
  cmp->localCount = 0;
  cmp->scopeDepth = 0;
  compiler = cmp;
}

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

  Compiler cmp;
  // ReSharper disable once CppDFALocalValueEscapesFunction
  initCompiler(&cmp);

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

/// Consume a token of the given [type] from scanned tokens.
///
/// Emits an error [msg] if the token type is not consumed.
static void consume(const TokenType type, const char* msg) {
  if (nextIs(type)) return advance();
  errorAtNext(msg);
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

/// Returns whether the given identifier tokens are equal.
static bool identifiersEqual(const Token* a, const Token* b) {
  if (a->length != b->length) return false;
  return memcmp(a->start, b->start, a->length) == 0;
}

/// Begin a new scope.
void pushScope() {
  compiler->scopeDepth++;
}

/// End the current scope.
void popScope() {
  compiler->scopeDepth--;
  uint8_t poppedLocals = 0;

  while (compiler->localCount > 0
    && compiler->locals[compiler->localCount - 1].depth > compiler->scopeDepth) {
    compiler->localCount--;
    poppedLocals++;
  }

  if (poppedLocals > 0) emitBytes(OP_POP_N, poppedLocals);
}

/// Execute the given statement rule within a new scope.
///
/// The scope will be popped when the rule completes.
void scope(const StmtFn rule) {
  pushScope();
  rule();
  popScope();
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
  const uint8_t identOffset = parseVariableIdent("Expected variable name.");

  nextMatches(TOKEN_EQUAL) ? expression() : emitByte(OP_NIL);
  consume(TOKEN_SEMICOLON, "Expected ';' after variable declaration.");

  defineVariable(identOffset);
}

/// Parses/compiles a statement from scanned tokens.
///
/// statement := exprStmt | printStmt | ifStmt | block ;
static void statement() {
  if (nextMatches(TOKEN_PRINT)) {
    printStatement();
  } else if (nextMatches(TOKEN_IF)) {
    ifStatement();
  } else if (nextMatches(TOKEN_LEFT_BRACE)) {
    scope(block);
  } else {
    expressionStatement();
  }
}

/// Parses/compiles an expression statement from scanned tokens.
///
/// exprStmt := expression ";" ;
static void expressionStatement() {
  expression();
  consume(TOKEN_SEMICOLON, "Expected ';' after expression.");
  emitByte(OP_POP);
}

/// Parses/compiles a print statement from scanned tokens.
///
/// printStmt := "print" exprStmt ;
static void printStatement() {
  expression();
  consume(TOKEN_SEMICOLON, "Expected ';' after value.");
  emitByte(OP_PRINT);
}

/// Parses/compiles an if statement from scanned tokens.
///
/// ifStmt := "if" "(" expression ")" statement ( "else" statement )? ;
static void ifStatement() {
  consume(TOKEN_LEFT_PAREN, "Expected '(' after 'if'.");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expected ')' after expression.");

  // Emit OP_JUMP_IF_FALSE to jump over then-branch if condition is false. We emit
  // an OP_POP to pop the value left on the stack by the condition expression and
  // then compile the then-branch statement. These will be executed if the condition
  // is not false
  const int thenJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);
  statement();

  // Emit OP_JUMP to jump over else-branch if we reach this instruction. This keeps
  // the then-branch from falling through to the else-branch. We then patch thenJump
  // to jump over the OP_JUMP, followed by an OP_POP for the else-branch to pop
  // the value left on the stack by the condition expression if the then-branch
  // was jumped over
  const int elseJump = emitJump(OP_JUMP);
  patchJump(thenJump);
  emitByte(OP_POP);

  if (nextMatches(TOKEN_ELSE)) statement();

  // Patch elseJump to the end of the if-statement bytecode
  patchJump(elseJump);
}

/// Parses/compiles a block statement from scanned tokens.
///
/// block := "{" declaration* "}" ;
static void block() {
  while (!nextIs(TOKEN_RIGHT_BRACE) && !nextIs(TOKEN_EOF)) {
    declaration();
  }
  consume(TOKEN_RIGHT_BRACE, "Expected '}' after block.");
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

  const ExprFn prefixRule = getRule(currentType())->prefix;
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
  uint8_t getOp, setOp;

  int varOffset = resolveLocal(compiler, &token);
  if (varOffset != -1) {
    getOp = OP_GET_LOCAL;
    setOp = OP_SET_LOCAL;
  } else {
    varOffset = makeIdentConstant(&token);
    getOp = OP_GET_GLOBAL;
    setOp = OP_SET_GLOBAL;
  }

  if (canAssign && nextMatches(TOKEN_EQUAL)) {
    expression();
    return emitBytes(setOp, (uint8_t)varOffset);
  }

  emitBytes(getOp, (uint8_t)varOffset);
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
  // [TokenType]        = {prefix,     infix,    Precedence      },
  [TOKEN_LEFT_PAREN]    = {grouping,   NULL,     PREC_NONE       },
  [TOKEN_RIGHT_PAREN]   = {NULL,       NULL,     PREC_NONE       },
  [TOKEN_LEFT_BRACE]    = {NULL,       NULL,     PREC_NONE       },
  [TOKEN_RIGHT_BRACE]   = {NULL,       NULL,     PREC_NONE       },
  [TOKEN_COMMA]         = {NULL,       NULL,     PREC_NONE       },
  [TOKEN_DOT]           = {NULL,       NULL,     PREC_NONE       },
  [TOKEN_SEMICOLON]     = {NULL,       NULL,     PREC_NONE       },

  // One/two character token rules
  [TOKEN_MINUS]         = {unary,      binary,   PREC_TERM       },
  [TOKEN_PLUS]          = {NULL,       binary,   PREC_TERM       },
  [TOKEN_SLASH]         = {NULL,       binary,   PREC_FACTOR     },
  [TOKEN_STAR]          = {NULL,       binary,   PREC_FACTOR     },

  [TOKEN_BANG]          = {unary,      NULL,     PREC_NONE       },
  [TOKEN_BANG_EQUAL]    = {NULL,       binary,   PREC_EQUALITY   },
  [TOKEN_EQUAL]         = {NULL,       NULL,     PREC_NONE       },
  [TOKEN_EQUAL_EQUAL]   = {NULL,       binary,   PREC_EQUALITY   },
  [TOKEN_GREATER]       = {NULL,       binary,   PREC_COMPARISON },
  [TOKEN_GREATER_EQUAL] = {NULL,       binary,   PREC_COMPARISON },
  [TOKEN_LESS]          = {NULL,       binary,   PREC_COMPARISON },
  [TOKEN_LESS_EQUAL]    = {NULL,       binary,   PREC_COMPARISON },

  // Literal token rules
  [TOKEN_IDENTIFIER]    = {variable,   NULL,     PREC_NONE       },
  [TOKEN_STRING]        = {string,     NULL,     PREC_NONE       },
  [TOKEN_NUMBER]        = {number,     NULL,     PREC_NONE       },

  // Keyword token rules
  [TOKEN_AND]           = {NULL,       NULL,     PREC_NONE       },
  [TOKEN_CLASS]         = {NULL,       NULL,     PREC_NONE       },
  [TOKEN_ELSE]          = {NULL,       NULL,     PREC_NONE       },
  [TOKEN_FALSE]         = {literal,    NULL,     PREC_NONE       },
  [TOKEN_FOR]           = {NULL,       NULL,     PREC_NONE       },
  [TOKEN_FUN]           = {NULL,       NULL,     PREC_NONE       },
  [TOKEN_IF]            = {NULL,       NULL,     PREC_NONE       },
  [TOKEN_NIL]           = {literal,    NULL,     PREC_NONE       },
  [TOKEN_OR]            = {NULL,       NULL,     PREC_NONE       },
  [TOKEN_PRINT]         = {NULL,       NULL,     PREC_NONE       },
  [TOKEN_RETURN]        = {NULL,       NULL,     PREC_NONE       },
  [TOKEN_SUPER]         = {NULL,       NULL,     PREC_NONE       },
  [TOKEN_THIS]          = {NULL,       NULL,     PREC_NONE       },
  [TOKEN_TRUE]          = {literal,    NULL,     PREC_NONE       },
  [TOKEN_VAR]           = {NULL,       NULL,     PREC_NONE       },
  [TOKEN_WHILE]         = {NULL,       NULL,     PREC_NONE       },

  // Misc token rules
  [TOKEN_ERROR]         = {NULL,       NULL,     PREC_NONE       },
  [TOKEN_EOF]           = {NULL,       NULL,     PREC_NONE       },
};
// @formatter:on

/// Returns the [ParseRule] for the given [type].
static ParseRule* getRule(const TokenType type) {
  return &rules[type];
}

// Variable parse/compilation/management functions ============================

/// Parses a variable identifier, returning the offset to the constant string
/// for the variable's identifier in the current chunk's constants array.
///
/// If the variable identifier cannot be parsed, errors with message [expect].
static uint8_t parseVariableIdent(const char* expect) {
  consume(TOKEN_IDENTIFIER, expect);

  // Declare variable if we're in a local scope. We just return 0 if so since
  // locals are resolved by identifier tokens while global variables are resolved
  // by their runtime-constant identifier strings loaded into the VM before the
  // lox bytecode is executed
  if (compiler->scopeDepth > 0) {
    declareLocal();
    return 0;
  }

  // Create the constant identifier string for global variable
  return makeIdentConstant(&parser.current);
}

/// Declare a variable in the current scope with the current token as its identifier.
///
/// Does nothing if we're currently in the global scope. Global variables are declared
/// and defined separately from local variables.
static void declareLocal() {
  // This shouldn't be necessary if this function is only called in local scopes
  if (compiler->scopeDepth == 0) return;

  const Token* identifier = &parser.current;

  // Check for existing variable with this identifier in this scope
  for (int i = compiler->localCount - 1; i >= 0; i--) {
    const Local* local = &compiler->locals[i];
    if (local->depth != -1 && local->depth < compiler->scopeDepth) break;
    if (identifiersEqual(identifier, &local->identifier)) {
      error("A variable with this name already exists in this scope.");
    }
  }

  addLocal(*identifier);
}

/// Define a variable.
///
/// If we are in a local scope, the current local will be marked as defined. The
/// value lives on the stack and is modified on the stack when the variable is
/// modified so we don't need to do anything more for it.
///
/// If we're in the global scope, emit bytecode to define the global variable,
/// passing the offset of the global variable's constant identifier string as
/// the operand.
static void defineVariable(const uint8_t identConstOffset) {
  if (compiler->scopeDepth > 0) return markDefined();
  emitBytes(OP_DEFINE_GLOBAL, identConstOffset);
}

/// Walks down scoped locals array until locating the local variable with the given
/// [identifier], returning the offset within the locals array of that local.
///
/// Returns `-1` if it does not exist.
///
/// Because of the way the stack behaves with statements popping their stack values
/// when complete, the only values on the stack when a local is declared are the
/// values of other locals in scope. This means the offset of the local in the
/// locals array directly reflects its location in the stack.
static int resolveLocal(const Compiler* cmp, const Token* identifier) {
  for (int i = cmp->localCount - 1; i >= 0; i--) {
    const Local* local = &cmp->locals[i];
    if (identifiersEqual(identifier, &local->identifier)) {
      if (local->depth == -1) {
        error("Can't read local variable in its own initializer.");
      }
      return i;
    }
  }

  return -1;
}

/// Add a local in the current scope with the given [identifier].
///
/// The local will be considered uninitialized/undefined until [markDefined] is
/// called.
static void addLocal(const Token identifier) {
  if (compiler->localCount == UINT8_COUNT) {
    return error("Too many local variables in function.");
  }

  Local* local = &compiler->locals[compiler->localCount++];
  local->identifier = identifier;
  local->depth = -1;
}

/// Mark the most recently declared local as defined/ready for use.
static void markDefined() {
  compiler->locals[compiler->localCount - 1].depth = compiler->scopeDepth;
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

/// Push an identifier's string to the current chunk's constants array, returning
/// the offset of the added constant string.
static uint8_t makeIdentConstant(const Token* token) {
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

static int emitJump(const OpCode opcode) {
  emitByte(opcode);
  emitBytes(0xff, 0xff);
  return currentChunk()->count - 2;
}

static void patchJump(const int offset) {
  const int jumpOffset = currentChunk()->count - 2 - offset;
  if (jumpOffset > UINT16_MAX) {
    error("Jump offset is too large");
  }

  // Patch first and second bytes of the 2-byte jump instruction operand
  currentChunk()->instructions[offset] = jumpOffset >> 8 & 0xff;
  currentChunk()->instructions[offset + 1] = jumpOffset & 0xff;
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
