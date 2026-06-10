#ifndef CLOX_COMPILER_H
#define CLOX_COMPILER_H

#include "chunk.h"
#include "scanner.h"
#include "object.h"

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

/// Represents the identifier and scope depth of a local variable.
typedef struct Local {
  /// Identifier string for this local variable.
  Token identifier;

  /// Scope depth of this local variable.
  ///
  /// A scope depth of 0 is the innermost scope, 1 is the enclosing scope of the
  /// innermost scope, etc.
  int depth;

  /// Whether this local variable has been captured in a closure.
  bool isCaptured;
} Local;

/// Represents a surrounding variable captured in a function closure.
typedef struct Upvalue {
  uint8_t offset;
  bool isLocal;
} Upvalue;

typedef struct Compiler Compiler;

/// Holds data for compiling lox source code into bytecode.
struct Compiler {
  /// Enclosing compiler, forming a linked list of nested compilers, each compiling
  /// a single encapsulating function object.
  Compiler* enclosing;

  /// The currently compiling function object.
  ///
  /// This can be a lox program itself, or any functions defined therein.
  ///
  /// A compiled lox program will be wrapped in a function object to be invoked
  /// at runtime.
  ObjFunction* function;

  /// The type of the currently compiling function type.
  FunctionType type;

  /// Local variables tracked during compilation.
  Local locals[UINT8_COUNT];

  /// Current number of local variables in scope at a given point during compilation.
  int localCount;

  /// Array of upvalues in the currently compiling function.
  Upvalue upvalues[UINT8_COUNT];

  /// Current scope depth at a given point during compilation.
  int scopeDepth;
};

typedef struct ClassCompiler ClassCompiler;

/// Tracks compilation of nested classes.
struct ClassCompiler {
  /// Pointer to the enclosing [ClassCompiler].
  ClassCompiler* enclosing;
};

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

ObjFunction* compile(const char* source);

static ObjFunction* endCompilation();

static void advance();
static void consume(TokenType type, const char* msg);
static TokenType currentType();
static bool currentIs(TokenType type);
static TokenType nextType();
static bool nextIs(TokenType type);
static bool nextMatches(TokenType type);
static bool identifiersEqual(const Token* a, const Token* b);
static bool isDiscardToken(Token token);

static void pushScope();
static void popScope();
static void scope(StmtFn rule);

static void declaration();
static void classDeclaration();
static void methodDeclaration();
static void functionDeclaration();
static void variableDeclaration();
static void statement();
static void expressionStatement();
static void printStatement();
static void ifStatement();
static void switchStatement();
static void returnStatement();
static void whileStatement();
static void forStatement();
static void block();
static void function(FunctionType type, bool isExpr);

static void expr(Precedence precedence);

static void expression();
static void unary(bool);
static void binary(bool);
static void and(bool);
static void or(bool);
static void nilish(bool);
static void switchExpr(bool);
static void literal(bool);
static void number(bool);
static void string(bool);
static void variable(bool canAssign);
static void variableGetSet(Token identifier, bool canAssign);
static void thisExpr(bool);
static void grouping(bool);
static void call(bool);
static void dot(bool canAssign);

static ParseRule* getRule(TokenType type);

static uint8_t variableIdentifier(const char* expect);
static void declareVariable();
static void defineVariable(uint8_t identConstOffset);
static int resolveLocal(const Compiler* c, const Token* identifier);
static void addLocal(Token identifier);
static void markDefined();
static int resolveUpvalue(Compiler* c, const Token* identifier);
static int addUpvalue(Compiler* c, uint8_t offset, bool isLocal);
static uint8_t argumentList();

static uint8_t makeConstant(Value value);
static uint8_t makeIdentConstant(const Token* token);
static void emitByte(uint8_t byte);
static void emitBytes(uint8_t a, uint8_t b);
static void emitConstant(Value value);
static void emitNilReturn();

static int emitJump(OpCode opcode);
static void patchJump(int offset);
static void emitLoop(int offset);

/// Linked list node for resolving multiple jumps to the same endpoint.
typedef struct Jump Jump;

struct Jump {
  int offset;
  Jump* next;
};

static Jump* newJumps();
static void emitJumps(Jump** jumps);
static void patchJumps(Jump** jumps);

static void errorAtNext(const char* msg);
static void error(const char* msg);
static void synchronize();

void markCompilerRoots();

#endif
