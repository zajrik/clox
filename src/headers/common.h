#ifndef CLOX_COMMON_H
#define CLOX_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define DEBUG_PRINT_CODE
#define DEBUG_TRACE_EXECUTION

/// Switch case macro to clean up single-statement, non-returning cases requiring
/// a break statement.
///
/// <code>
/// switch (operatorType) {
///   case TOKEN_MINUS: DO(emitByte(OP_NEGATE));
///   case TOKEN_BANG: DO(emitByte(OP_NOT));
///   default: break;
/// }
/// </code>
#define DO(stmt) stmt; break

#endif
