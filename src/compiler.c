#include <string.h>

#include "compiler.h"
#include "chunk.h"
#include "common.h"
#include "memory.h"
#include "object.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

/// Global parser instance.
Parser parser;

/// Pointer to the current compiler.
Compiler* compiler = NULL;

/// Pointer to the current class compiler for tracking when we're inside of
/// method bodies.
ClassCompiler* currentClass = NULL;

/// Initialize the given [compiler] instance.
static void initCompiler(Compiler* c, const FunctionType type) {
  // Link given compiler to the current compiler
  c->enclosing = compiler;

  c->function = NULL;
  c->type = type;
  c->localCount = 0;
  c->scopeDepth = 0;
  c->function = newFunction(type);

  // Set the given compiler as current compiler
  compiler = c;

  const bool isAnonymous = memcmp(parser.current.start, "fun", parser.current.length) == 0;

  if (type != TYPE_SCRIPT && !isAnonymous) {
    compiler->function->identifier = copyString(
      parser.current.start,
      parser.current.length
    );
  }

  // Reserve local slot 0 for root function, or for `this` in methods
  Local* local = &compiler->locals[compiler->localCount++];
  local->depth = 0;
  local->isCaptured = false;
  local->identifier.type = TOKEN_IDENTIFIER;
  if (type == TYPE_METHOD || type == TYPE_INITIALIZER) {
    local->identifier.start = "this";
    local->identifier.length = 4;
  } else {
    local->identifier.start = "";
    local->identifier.length = 0;
  }
}

/// Returns a pointer to the currently compiling chunk.
static Chunk* currentChunk() {
  return &compiler->function->chunk;
}

/// Compile the given [source] text into bytecode instructions, storing them in
/// a function object.
///
/// Returns a pointer to the compiled function object.
ObjFunction* compile(const char* source) {
  initScanner(source);

  Compiler c;
  initCompiler(&c, TYPE_SCRIPT);

  parser.hadError = false;
  parser.panicMode = false;

  // Scan first token into [Parser.next]
  advance();

  while (!nextMatches(TOKEN_EOF)) {
    declaration();
  }

  ObjFunction* fun = endCompilation();

  return !parser.hadError ? fun : NULL;
}

/// End the compilation process for the currently compiling function/script.
static ObjFunction* endCompilation() {
  // Emit nil return to allow functions to return nil by default if they do not
  // specify a return of their own
  emitDefaultReturn();

  ObjFunction* fun = compiler->function;

  #ifdef DEBUG_PRINT_CODE
  if (!parser.hadError) {
    disassembleChunk(
      currentChunk(),
      fun->identifier != NULL
        ? fun->identifier->chars
        : (fun->type == TYPE_SCRIPT ? "script" : "anon fun")
    );
  } else {
    printf("errors occurred\n");
  }
  #endif

  // Return to the enclosing compiler
  compiler = compiler->enclosing;
  return fun;
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

/// Switch-case wrapper to clean up branches that need an `advance()` call and
/// a break statement.
#define ADVANCE(stmt...) advance(), stmt; break

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

/// Returns whether the given [token] is a discard token (`_`).
static bool isDiscardToken(const Token token) {
  return token.length == 1 && *token.start == '_';
}

// Scope management functions =================================================

/// Begin a new scope.
void pushScope() {
  compiler->scopeDepth++;
}

/// End the current scope.
void popScope() {
  compiler->scopeDepth--;

  // Close upvalues and pop stack values at the end of scope
  while (
    compiler->localCount > 0
    && compiler->locals[compiler->localCount - 1].depth > compiler->scopeDepth
  ) {
    emitByte(
      compiler->locals[compiler->localCount - 1].isCaptured
        ? OP_CLOSE_UPVALUE
        : OP_POP
    );
    compiler->localCount--;
  }
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

/// Compiles a declaration statement from scanned tokens.
///
/// declaration := varDecl | funDecl | classDecl | statement ;
static void declaration() {
  switch (nextType()) {
    case TOKEN_CLASS: ADVANCE(classDeclaration());
    case TOKEN_FUN: ADVANCE(functionDeclaration());
    case TOKEN_VAR: ADVANCE(variableDeclaration());
    default: statement();
  }
  if (parser.panicMode) synchronize();
}

/// Compiles a class declaration from scanned tokens.
static void classDeclaration() {
  const uint8_t identOffset = variableIdentifier("Expected class name.");
  const Token identifier = parser.current;

  emitBytes(OP_CLASS, identOffset);
  defineVariable(identOffset);

  ClassCompiler classCompiler;
  classCompiler.enclosing = currentClass;

  // SAFETY: This value lives on the C stack and will be discarded at the end
  // of this function when currentClass is restored to the enclosing compiler
  // ReSharper disable once CppDFALocalValueEscapesFunction
  currentClass = &classCompiler;

  // Load the class object onto the stack for OP_METHOD instructions
  variableGetSet(identifier, false);

  // Compile method declarations in class body
  consume(TOKEN_LEFT_BRACE, "Expected '{' before class body.");
  while (!nextIs(TOKEN_RIGHT_BRACE) && !nextIs(TOKEN_EOF)) {
    methodDeclaration();
  }
  consume(TOKEN_RIGHT_BRACE, "Expected '}' after class body.");

  // Pop the class object from the stack
  emitByte(OP_POP);

  currentClass = currentClass->enclosing;
}

/// Compiles a method declaration from scanned tokens.
static void methodDeclaration() {
  const uint8_t identOffset = variableIdentifier("Expected method name.");
  const bool isInitializer = parser.current.length == 4
    && memcmp(parser.current.start, "init", 4) == 0;

  function(isInitializer ? TYPE_INITIALIZER : TYPE_METHOD, false);
  emitBytes(OP_METHOD, identOffset);
}

/// Compiles a function declaration from scanned tokens.
static void functionDeclaration() {
  const uint8_t identOffset = variableIdentifier("Expected function name.");

  // Mark function as defined so it can refer to itself.
  markDefined();

  function(TYPE_FUNCTION, false);
  defineVariable(identOffset);
}

/// Compiles a variable declaration statement from scanned tokens.
static void variableDeclaration() {
  const uint8_t identOffset = variableIdentifier("Expected variable name.");

  nextMatches(TOKEN_EQUAL) ? expression() : emitByte(OP_NIL);
  consume(TOKEN_SEMICOLON, "Expected ';' after variable declaration.");

  defineVariable(identOffset);
}

/// Compiles a statement from scanned tokens.
///
/// statement := exprStmt
///              | printStmt
///              | ifStmt
///              | switchStmt
///              | returnStmt
///              | whileStmt
///              | forStmt
///              | block ;
static void statement() {
  switch (nextType()) {
    case TOKEN_PRINT: ADVANCE(printStatement());
    case TOKEN_IF: ADVANCE(ifStatement());
    case TOKEN_SWITCH: ADVANCE(switchStatement());
    case TOKEN_RETURN: ADVANCE(returnStatement());
    case TOKEN_WHILE: ADVANCE(whileStatement());
    case TOKEN_FOR: ADVANCE(scope(forStatement));
    case TOKEN_LEFT_BRACE: ADVANCE(scope(block));
    default: expressionStatement();
  }
}

/// Compiles an expression statement from scanned tokens.
///
/// exprStmt := expression ";" ;
static void expressionStatement() {
  expression();
  consume(TOKEN_SEMICOLON, "Expected ';' after expression.");
  emitByte(OP_POP);
}

/// Compiles a print statement from scanned tokens.
///
/// printStmt := "print" exprStmt ";" ;
static void printStatement() {
  expression();
  consume(TOKEN_SEMICOLON, "Expected ';' after value.");
  emitByte(OP_PRINT);
}

/// Compiles an if statement from scanned tokens.
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

/// Parse/compile a switch statement.
///
/// switchStmt  := "switch" "(" expression ")" "{" switchCase* defaultCase? "}" ;
/// switchCase  := "case" ":" statement ;
/// defaultCase := "default" ":" statement ;
static void switchStatement() {
  consume(TOKEN_LEFT_PAREN, "Expected '(' after switch.");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expected ')' after expression.");
  consume(TOKEN_LEFT_BRACE, "Expected '{' after switch value.");

  bool defaultFound = false;

  Jump* exit = newJumps();

  while (!nextMatches(TOKEN_RIGHT_BRACE) && !nextMatches(TOKEN_EOF)) {
    if (!nextMatches(TOKEN_CASE) && !nextMatches(TOKEN_DEFAULT)) {
      error("Expected 'case' or 'default'.");
    }

    // Compile non-default cases
    if (!currentIs(TOKEN_DEFAULT)) {
      // Copy condition value on stack to compare condition and case expression
      // without popping the original value from the stack
      emitByte(OP_COPY);

      // Compile case expression and emit OP_EQUAL to compare to copied condition
      expression();
      emitByte(OP_EQUAL);
    }
    // Compile default case
    else {
      if (defaultFound) {
        error("A switch statement may only have a single 'default' case.");
      }
      defaultFound = true;
      emitByte(OP_TRUE);
    }

    consume(TOKEN_COLON, "Expected ':'.");

    const int caseJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    statement();

    emitJumps(&exit);
    patchJump(caseJump);
    emitByte(OP_POP);
  }

  patchJumps(&exit);

  // Pop condition value;
  emitByte(OP_POP);
}

/// Compiles a return statement from scanned tokens.
static void returnStatement() {
  if (compiler->type == TYPE_SCRIPT) {
    error("Can't return from top-level code.");
  }

  if (nextMatches(TOKEN_SEMICOLON)) {
    emitDefaultReturn();
  } else {
    if (compiler->type == TYPE_INITIALIZER) {
      error("Can't return value from class initializer.");
      return emitDefaultReturn();
    }

    expression();
    consume(TOKEN_SEMICOLON, "Expected ';' after return value.");
    emitByte(OP_RETURN);
  }
}

/// Compiles a while statement from scanned tokens.
///
/// whileStmt := "while" "(" expression ")" statement ;
static void whileStatement() {
  const int loopStart = currentChunk()->count;

  consume(TOKEN_LEFT_PAREN, "Expected '(' after 'while'.");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expected ')' after expression.");

  const int exit = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);
  statement();
  emitLoop(loopStart);

  patchJump(exit);
  emitByte(OP_POP);
}

/// Compiles a for statement from scanned tokens.
///
/// forStmt := "for" "(" ( varDecl | expression? ";" )
///            expression? ";" expression? ")" statement ;
static void forStatement() {
  consume(TOKEN_LEFT_PAREN, "Expected '(' after 'for'.");

  // Compile initializer clause
  if (nextMatches(TOKEN_VAR)) variableDeclaration();
  else if (!nextMatches(TOKEN_SEMICOLON)) expressionStatement();

  // Set loop start before condition expression. If we have an increment expression,
  // this will be changed so that the final loop instruction jumps to the increment
  // expression which will then jump back here after executing
  int loopStart = currentChunk()->count;

  int exit = -1;

  // Compile loop condition if present and add exit jump op
  if (!nextMatches(TOKEN_SEMICOLON)) {
    expression();
    consume(TOKEN_SEMICOLON, "Expected ';' after loop condition.");

    exit = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
  }

  // Compile increment expression if present
  if (!nextMatches(TOKEN_RIGHT_PAREN)) {
    const int incrementJump = emitJump(OP_JUMP);
    const int incrementStart = currentChunk()->count;

    expression();
    emitByte(OP_POP);
    consume(TOKEN_RIGHT_PAREN, "Expected ')' after for clauses.");

    // Loop back to start after executing increment expression, update loopStart
    // to point to the increment so the body loops back here after executing
    emitLoop(loopStart);
    loopStart = incrementStart;

    patchJump(incrementJump);
  }

  // Compile loop body, emit loop for loop start, or increment start if provided
  statement();
  emitLoop(loopStart);

  // If a loop condition was provided, patch exit jump and pop condition
  if (exit != -1) {
    patchJump(exit);
    emitByte(OP_POP);
  }
}

/// Compiles a block statement from scanned tokens.
///
/// block := "{" declaration* "}" ;
static void block() {
  while (!nextIs(TOKEN_RIGHT_BRACE) && !nextIs(TOKEN_EOF)) {
    declaration();
  }
  consume(TOKEN_RIGHT_BRACE, "Expected '}' after block.");
}

/// Compiles a function statement or expression of the given [type] from scanned
/// tokens.
static void function(const FunctionType type, const bool isExpr) {
  // If we're compiling a function expression, an identifier is optional but the
  // "fun" keyword will be the current scanned token so we'll need to take in the
  // identifier if we see one
  if (isExpr && nextIs(TOKEN_IDENTIFIER)) advance();

  Compiler c;
  initCompiler(&c, type);

  // Push scope for function. This does not need to be closed since the function
  // being compiled has its own compiler instance which intrinsically discards its
  // own scope when compilation ends and the compiler itself is discarded and freed
  pushScope();

  consume(TOKEN_LEFT_PAREN, "Expected '(' after function name.");

  if (!nextIs(TOKEN_RIGHT_PAREN)) {
    do {
      // Allow dangling comma
      if (nextIs(TOKEN_RIGHT_PAREN)) break;

      compiler->function->arity++;
      if (compiler->function->arity > 255) {
        errorAtNext("Can't have more than 255 parameters.");
      }
      const uint8_t param = variableIdentifier("Expected parameter identifier.");
      defineVariable(param);
    } while (nextMatches(TOKEN_COMMA));
  }

  consume(TOKEN_RIGHT_PAREN, "Expected ')' after function parameters.");

  if (nextMatches(TOKEN_LEFT_BRACE)) {
    block();
  } else if (nextMatches(TOKEN_EQUAL)) {
    expression();
    emitByte(OP_RETURN);
    if (!isExpr) consume(TOKEN_SEMICOLON, "Expected ';' after function body.");
  }

  ObjFunction* function = endCompilation();

  // Emit closure and upvalue bytecode
  emitBytes(OP_CLOSURE, makeConstant(OBJ_VAL(function)));
  for (int i = 0; i < function->upvalueCount; i++) {
    emitByte(c.upvalues[i].isLocal ? 1 : 0);
    emitByte(c.upvalues[i].offset);
  }
}

// Expression parse/compilation functions =====================================

/// Compiles an expression from scanned tokens following the given [precedence],
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

/// Compiles an expression, emitting compiled expression bytecode.
static void expression() {
  expr(PREC_ASSIGNMENT);
}

/// Compiles a unary expression, emitting bytecode for the operand expression
/// and operator.
static void unary(bool _) {
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

/// Compiles a binary expression, emitting bytecode for the operand expressions
/// and the operator.
///
/// The left-hand operand of the binary expression has already been compiled to
/// the chunk at the point this will be called.
static void binary(bool _) {
  const TokenType operatorType = currentType();
  const ParseRule* operatorRule = getRule(operatorType);

  // Compile the right-hand operand. A binary operator's right-hand operand
  // precedence is always one level higher than the operator's own precedence
  expr(operatorRule->precedence + 1);

  switch (operatorType) {
    case TOKEN_EQUAL_EQUAL: return emitByte(OP_EQUAL);
    case TOKEN_BANG_EQUAL: return emitBytes(OP_EQUAL, OP_NOT);
    case TOKEN_GREATER: return emitByte(OP_GREATER);
    case TOKEN_GREATER_EQUAL: return emitBytes(OP_LESS, OP_NOT);
    case TOKEN_LESS: return emitByte(OP_LESS);
    case TOKEN_LESS_EQUAL: return emitBytes(OP_GREATER, OP_NOT);
    case TOKEN_PLUS: return emitByte(OP_ADD);
    case TOKEN_MINUS: return emitByte(OP_SUBTRACT);
    case TOKEN_STAR: return emitByte(OP_MULTIPLY);
    case TOKEN_SLASH: return emitByte(OP_DIVIDE);
    default: break;
  }
}

/// Compile an `and` operator expression.
///
/// The left-hand operand of the `and` expression has already been compiled to
/// the chunk at the point this will be called.
static void andExpr(bool _) {
  // The left-hand operand will be on the stack already for this jump op
  const int end = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);
  expr(PREC_AND);
  patchJump(end);
}

/// Compile an `or` operator expression.
///
/// The left-hand operand of the `or` expression has already been compiled to
/// the chunk at the point this will be called.
static void orExpr(bool _) {
  // The left-hand operand will be on the stack already for this jump op
  const int end = emitJump(OP_JUMP_IF_TRUE);
  emitByte(OP_POP);
  expr(PREC_OR);
  patchJump(end);
}

/// Compile a nilish (??) operator expression.
///
/// The left-hand operand of the `??` expression has already been compiled to
/// the chunk at this point and will be called.
static void nilish(bool _) {
  const int end = emitJump(OP_JUMP_IF_NOT_NIL);
  emitByte(OP_POP);
  expr(PREC_OR);
  patchJump(end);
}

/// Compile a switch expression from source tokens.
///
/// switchExpr := "switch" "(" expression ")" "{" caseExpr* "}" ;
/// caseExpr   := ( expression | "_" ) "=>" expression ;
static void switchExpr(bool _) {
  consume(TOKEN_LEFT_PAREN, "Expected '(' after switch.");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expected ')' after expression.");
  consume(TOKEN_LEFT_BRACE, "Expected '{' after switch value.");

  bool defaultFound = false;

  Jump* exit = newJumps();

  while (!nextMatches(TOKEN_RIGHT_BRACE) && !nextMatches(TOKEN_EOF)) {
    // Compile expression cases
    if (!nextIs(TOKEN_IDENTIFIER) || !isDiscardToken(parser.next)) {
      emitByte(OP_COPY);
      expression();
      emitByte(OP_EQUAL);
    }
    // Compile wildcard cases
    else {
      defaultFound = true;
      advance();
      emitByte(OP_TRUE);
    }

    consume(TOKEN_ARROW, "Expected '=>'.");

    const int caseJump = emitJump(OP_JUMP_IF_FALSE);

    // Pop case expression AND switch input expression from stack
    emitBytes(OP_POP_N, 2);

    expression();

    emitJumps(&exit);
    patchJump(caseJump);
    emitByte(OP_POP);
  }

  // If we didn't find a wildcard/default case then we need to make sure the expression
  // returns `nil` in the event that no case matches the input. Any matching case will
  // jump over this, leaving its result on the stack
  if (!defaultFound) emitBytes(OP_POP, OP_NIL);

  patchJumps(&exit);
}

/// Compiles a function expression from scanned tokens.
static void funExpr(bool _) {
  function(TYPE_FUNCTION, true);
}

/// Compiles a keyword-literal expression, emitting the relevant opcode.
static void literal(bool _) {
  switch (currentType()) {
    case TOKEN_NIL: return emitByte(OP_NIL);
    case TOKEN_TRUE: return emitByte(OP_TRUE);
    case TOKEN_FALSE: return emitByte(OP_FALSE);
    default: break;
  }
}

/// Compiles a number constant, emitting bytecode for a constant number value.
static void number(bool _) {
  const double value = strtod(parser.current.start, NULL);
  emitConstant(NUMBER_VAL(value));
}

/// Compiles a string constant, emitting bytecode for a constant string value.
static void string(bool _) {
  const char* stringStart = parser.current.start + 1;
  const int stringLength = parser.current.length - 2;
  emitConstant(OBJ_VAL(copyString(stringStart, stringLength)));
}

/// Compiles a variable/identifier expression.
static void variable(const bool canAssign) {
  variableGetSet(parser.current, canAssign);
}

/// Compiles a variable/identifier get/set expression, emitting bytecode to fetch/set
/// the value for that variable/identifier.
static void variableGetSet(const Token identifier, const bool canAssign) {
  VariableKind kind;
  int varOffset = resolveLocal(compiler, &identifier);
  if (varOffset != -1) {
    kind = VAR_LOCAL;
  } else if ((varOffset = resolveUpvalue(compiler, &identifier)) != -1) {
    kind = VAR_UPVALUE;
  } else {
    varOffset = makeIdentConstant(&identifier);
    kind = VAR_GLOBAL;
  }

  // Handle valid assignment operators
  if (canAssign) {
    switch (nextType()) {
      case TOKEN_EQUAL:
        advance();
        expression();
        emitByte(OP_SET_VALUE);
        return emitBytes(kind, (uint8_t)varOffset);

      case TOKEN_PLUS_EQUAL:
        advance();
        emitBytes(OP_GET_VALUE, (uint8_t)varOffset);
        expression();
        emitByte(OP_ADD);
        emitByte(OP_SET_VALUE);
        return emitBytes(kind, (uint8_t)varOffset);

      case TOKEN_MINUS_EQUAL:
        advance();
        emitBytes(OP_GET_VALUE, (uint8_t)varOffset);
        expression();
        emitByte(OP_SUBTRACT);
        emitByte(OP_SET_VALUE);
        return emitBytes(kind, (uint8_t)varOffset);

      case TOKEN_STAR_EQUAL:
        advance();
        emitBytes(OP_GET_VALUE, (uint8_t)varOffset);
        expression();
        emitByte(OP_MULTIPLY);
        emitByte(OP_SET_VALUE);
        return emitBytes(kind, (uint8_t)varOffset);

      case TOKEN_SLASH_EQUAL:
        advance();
        emitBytes(OP_GET_VALUE, (uint8_t)varOffset);
        expression();
        emitByte(OP_DIVIDE);
        emitByte(OP_SET_VALUE);
        return emitBytes(kind, (uint8_t)varOffset);

      case TOKEN_NILISH_EQUAL:
        advance();
        emitByte(OP_GET_VALUE);
        emitBytes(kind, (uint8_t)varOffset);
        const int end = emitJump(OP_JUMP_IF_NOT_NIL);

        emitByte(OP_POP);
        expr(PREC_OR);
        emitByte(OP_SET_VALUE);
        emitBytes(kind, (uint8_t)varOffset);

        // Patch jump after set op so we're not wasting a cycle on setting a
        // value that hasn't changed
        return patchJump(end);

      default: break;
    }
  }

  emitByte(OP_GET_VALUE);
  emitBytes(kind, (uint8_t)varOffset);
}

/// Compiles a `this` expression.
static void thisExpr(bool _) {
  if (currentClass == NULL) {
    error("Can't use 'this' outside of a class body.");
    return;
  }

  // The `this` token will be the current token and can just be compiled as a
  // variable with the name "this". We set aside local slot 0 in each call frame
  // for the frame's root function which will instead be used for `this` in methods
  variable(false);
}

/// Compiles a grouping expression, emitting bytecode for the grouped expression.
static void grouping(bool _) {
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expected ')' after expression");
}

/// Compiles a function call expression.
static void call(bool _) {
  const uint8_t argCount = argumentList();
  emitBytes(OP_CALL, argCount);
}

/// Compiles an object field get/set expression.
static void dot(const bool canAssign) {
  consume(TOKEN_IDENTIFIER, "Expect property name after '.'.");
  const uint8_t identOffset = makeIdentConstant(&parser.current);

  // Handle property/field set expressions
  if (canAssign && nextMatches(TOKEN_EQUAL)) {
    expression();
    emitByte(OP_SET_VALUE);
    emitBytes(VAR_PROPERTY, identOffset);
  }

  // Handle property invocations
  else if (nextMatches(TOKEN_LEFT_PAREN)) {
    const uint8_t argCount = argumentList();
    emitByte(OP_INVOKE);
    emitBytes(identOffset, argCount);
  }

  // Otherwise compile as a property get expression
  else {
    emitByte(OP_GET_VALUE);
    emitBytes(VAR_PROPERTY, identOffset);
  }
}

/// Compiles a conditional/ternary operator expression from scanned tokens.
static void ternary(bool _) {
  // The condition is already on the stack and the '?' is already consumed
  // at this point so we can jump right in to some jumps
  const int thenBranch = emitJump(OP_JUMP_IF_FALSE);
  expression();
  const int exitJump = emitJump(OP_JUMP);

  consume(TOKEN_COLON, "Expect ':' after '?' expression.");

  patchJump(thenBranch);
  expression();
  patchJump(exitJump);
}

// @formatter:off
/// Table holding token parse rules.
static ParseRule rules[] = {
  // Single character token rules
  // [TokenType]        = {prefix,     infix,    Precedence      },
  [TOKEN_LEFT_PAREN]    = {grouping,   call,     PREC_CALL       },
  [TOKEN_RIGHT_PAREN]   = {NULL,       NULL,     PREC_NONE       },
  [TOKEN_LEFT_BRACE]    = {NULL,       NULL,     PREC_NONE       },
  [TOKEN_RIGHT_BRACE]   = {NULL,       NULL,     PREC_NONE       },
  [TOKEN_COMMA]         = {NULL,       NULL,     PREC_NONE       },
  [TOKEN_DOT]           = {NULL,       dot,      PREC_CALL       },
  [TOKEN_SEMICOLON]     = {NULL,       NULL,     PREC_NONE       },
  [TOKEN_QUESTION]      = {NULL,       ternary,  PREC_ASSIGNMENT },

  // Multiple-character token rules
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
  [TOKEN_PLUS_EQUAL]    = {NULL,       NULL,     PREC_NONE       },
  [TOKEN_MINUS_EQUAL]   = {NULL,       NULL,     PREC_NONE       },
  [TOKEN_STAR_EQUAL]    = {NULL,       NULL,     PREC_NONE       },
  [TOKEN_SLASH_EQUAL]   = {NULL,       NULL,     PREC_NONE       },
  [TOKEN_NILISH_EQUAL]  = {NULL,       NULL,     PREC_NONE       },

  [TOKEN_ARROW]         = {NULL,       NULL,     PREC_NONE       },
  [TOKEN_NILISH]        = {NULL,       nilish,   PREC_OR         },

  // Literal token rules
  [TOKEN_IDENTIFIER]    = {variable,   NULL,     PREC_NONE       },
  [TOKEN_STRING]        = {string,     NULL,     PREC_NONE       },
  [TOKEN_NUMBER]        = {number,     NULL,     PREC_NONE       },

  // Keyword token rules
  [TOKEN_AND]           = {NULL,       andExpr,  PREC_AND        },
  [TOKEN_CLASS]         = {NULL,       NULL,     PREC_NONE       },
  [TOKEN_CASE]          = {NULL,       NULL,     PREC_NONE       },
  [TOKEN_DEFAULT]       = {NULL,       NULL,     PREC_NONE       },
  [TOKEN_ELSE]          = {NULL,       NULL,     PREC_NONE       },
  [TOKEN_FALSE]         = {literal,    NULL,     PREC_NONE       },
  [TOKEN_FOR]           = {NULL,       NULL,     PREC_NONE       },
  [TOKEN_FUN]           = {funExpr,    NULL,     PREC_NONE       },
  [TOKEN_IF]            = {NULL,       NULL,     PREC_NONE       },
  [TOKEN_NIL]           = {literal,    NULL,     PREC_NONE       },
  [TOKEN_OR]            = {NULL,       orExpr,   PREC_OR         },
  [TOKEN_PRINT]         = {NULL,       NULL,     PREC_NONE       },
  [TOKEN_RETURN]        = {NULL,       NULL,     PREC_NONE       },
  [TOKEN_SUPER]         = {NULL,       NULL,     PREC_NONE       },
  [TOKEN_SWITCH]        = {switchExpr, NULL,     PREC_NONE       },
  [TOKEN_THIS]          = {thisExpr,   NULL,     PREC_NONE       },
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

/// Compiles a variable identifier, returning the offset to the constant string
/// for the variable's identifier in the current chunk's constants array.
///
/// If the variable identifier cannot be parsed, errors with message [expect].
static uint8_t variableIdentifier(const char* expect) {
  consume(TOKEN_IDENTIFIER, expect);

  // Declare variable if we're in a local scope. We just return 0 if so since
  // locals are resolved by identifier tokens while global variables are resolved
  // by their runtime-constant identifier strings loaded into the VM before the
  // lox bytecode is executed
  if (compiler->scopeDepth > 0) {
    declareVariable();
    return 0;
  }

  // Create the constant identifier string for global variable
  return makeIdentConstant(&parser.current);
}

/// Declare a variable in the current scope with the current token as its identifier.
///
/// Does nothing if we're currently in the global scope. Global variables are declared
/// and defined separately from local variables.
static void declareVariable() {
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
static int resolveLocal(const Compiler* c, const Token* identifier) {
  for (int i = c->localCount - 1; i >= 0; i--) {
    const Local* local = &c->locals[i];
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
  local->isCaptured = false;
}

/// Mark the most recently declared local as defined/ready for use.
///
/// Does nothing if we're currently in the global scope. Global variables are declared
/// and defined separately from local variables.
static void markDefined() {
  // Only mark non-global variables as defined.
  if (compiler->scopeDepth == 0) return;
  compiler->locals[compiler->localCount - 1].depth = compiler->scopeDepth;
}

/// Resolve an upvalue with the given [identifier].
static int resolveUpvalue(Compiler* c, const Token* identifier) {
  // If this is the root scope, the upvalue must be global
  if (c->enclosing == NULL) return -1;

  // Try to resolve an upvalue from locals in the enclosing scope. If a local is
  // resolved, mark it as captured by a closure
  const int local = resolveLocal(c->enclosing, identifier);
  if (local != -1) {
    c->enclosing->locals[local].isCaptured = true;
    return addUpvalue(c, (uint8_t)local, true);
  }

  // Try to resolve an upvalue from upvalues in the enclosing scope
  const int upvalue = resolveUpvalue(c->enclosing, identifier);
  if (upvalue != -1) return addUpvalue(c, (uint8_t)upvalue, false);

  return -1;
}

/// Add an upvalue to the compiled upvalues in the given compiler's scope.
///
/// Returns the offset for locating the upvalue at runtime.
static int addUpvalue(Compiler* c, const uint8_t offset, const bool isLocal) {
  const int upvalueCount = c->function->upvalueCount;

  // Check for existing compiled upvalue matching the given offset/locality
  for (int i = 0; i < upvalueCount; i++) {
    const Upvalue* upvalue = &compiler->upvalues[i];
    if (upvalue->offset == offset && upvalue->isLocal == isLocal) {
      return i;
    }
  }

  if (upvalueCount == UINT8_COUNT) {
    error("Too many closure variables in function.");
    return 0;
  }

  // Add new upvalue
  Upvalue* upvalue = &c->upvalues[upvalueCount];
  upvalue->isLocal = isLocal;
  upvalue->offset = offset;
  return c->function->upvalueCount++;
}

/// Compile a series of arguments for a call, returning the number of compiled arguments.
///
/// Argument expressions will be on the stack after to be consumed by a call expression.
static uint8_t argumentList() {
  uint8_t count = 0;
  if (!nextIs(TOKEN_RIGHT_PAREN)) {
    do {
      // Allow dangling comma
      if (nextIs(TOKEN_RIGHT_PAREN)) break;

      expression();

      if (count++ == 255) {
        error("A function call may only have up to 255 arguments.");
      }
    } while (nextMatches(TOKEN_COMMA));
  }
  consume(TOKEN_RIGHT_PAREN, "Expected ')' after arguments.");
  return count;
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

/// Write default return instructions to the chunk currently being compiled.
static void emitDefaultReturn() {
  // If we're compiling an initializer, put the instance on the stack to return
  if (compiler->type == TYPE_INITIALIZER) {
    emitByte(OP_GET_VALUE);
    emitBytes(VAR_LOCAL, 0);
    return emitByte(OP_RETURN);
  }

  // Otherwise emit a nil return
  emitBytes(OP_NIL, OP_RETURN);
}

/// Write a jump [opcode] followed by placeholder bytes for the jump offset operand.
///
/// Must be patched with [patchJump] to wire up the accompanying jump offset.
///
/// Returns the instruction offset of the operand to be patched.
static int emitJump(const OpCode opcode) {
  emitByte(opcode);
  emitBytes(0xff, 0xff);
  return currentChunk()->count - 2;
}

/// Patches the jump instruction operand at [offset] to the current instruction
/// offset.
static void patchJump(const int offset) {
  const int jumpOffset = currentChunk()->count - offset - 2;
  if (jumpOffset > UINT16_MAX) error("Jump offset is too large");

  // Patch first and second bytes of the 2-byte jump instruction operand
  currentChunk()->instructions[offset] = jumpOffset >> 8 & 0xff;
  currentChunk()->instructions[offset + 1] = jumpOffset & 0xff;
}

/// Write [OP_LOOP] followed by loop offset operand bytes to loop back to the given
/// instruction [offset].
static void emitLoop(const int offset) {
  emitByte(OP_LOOP);

  const int loopOffset = currentChunk()->count - offset + 2;
  if (loopOffset > UINT16_MAX) error("Loop body is too large.");

  emitBytes(loopOffset >> 8 & 0xff, loopOffset & 0xff);
}

/// Allocate a new [Jump] node.
///
/// This should be passed to [emitJumps] and [patchJumps] to emit and patch a batch
/// of jumps that will all point to the same endpoint when patched.
static Jump* newJumps() {
  Jump* jump = malloc(sizeof(Jump));
  jump->offset = -1;
  jump->next = NULL;
  return jump;
}

/// Emit [OP_JUMP], prepending a [Jump] entry for that jump to the given linked
/// jump list.
///
/// Don't forget to call [patchJumps]!
static void emitJumps(Jump** jumps) {
  Jump* jump = malloc(sizeof(Jump));
  jump->offset = emitJump(OP_JUMP);
  jump->next = *jumps;
  *jumps = jump;
}

/// Patch all jumps in the given linked list of jumps.
///
/// The allocated jumps will be freed after patching, including the root jump.
static void patchJumps(Jump** jumps) {
  while ((*jumps)->next != NULL) {
    Jump* current = *jumps;
    patchJump(current->offset);
    *jumps = current->next;
    free(current);
  }
  free(*jumps);
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
      case TOKEN_SWITCH:
      case TOKEN_WHILE:
      case TOKEN_PRINT:
      case TOKEN_RETURN:
        return;

      default: break;
    }

    advance();
  }
}

// Garbage Collection =========================================================

/// Mark compiler roots for the GC.
void markCompilerRoots() {
  for (const Compiler* c = compiler; c != NULL; c = c->enclosing) {
    markObject((Obj*)c->function);
  }
}
