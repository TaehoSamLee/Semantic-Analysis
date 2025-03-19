#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "parser.h"   // For ASTNode definition
#include "tokens.h"   // For token types (e.g. TOKEN_INT)

// --------------------------------------------------------------------------
// Symbol Table Structures
// --------------------------------------------------------------------------

typedef struct Symbol {
    char name[100];          // Variable name
    int type;                // Data type (e.g., TOKEN_INT)
    int scope_level;         // Scope nesting level
    int line_declared;       // Line where declared
    int is_initialized;      // 0 = not initialized, 1 = initialized
    struct Symbol* next;     // Next symbol in the linked list
} Symbol;

typedef struct {
    Symbol* head;            // Head of the symbol linked list
    int current_scope;       // Current scope level
} SymbolTable;

// --------------------------------------------------------------------------
// Symbol Table Operations
// --------------------------------------------------------------------------

SymbolTable* init_symbol_table();
void add_symbol(SymbolTable* table, const char* name, int type, int line);
Symbol* lookup_symbol(SymbolTable* table, const char* name);
Symbol* lookup_symbol_current_scope(SymbolTable* table, const char* name);
void enter_scope(SymbolTable* table);
void exit_scope(SymbolTable* table);
void remove_symbols_in_current_scope(SymbolTable* table);
void free_symbol_table(SymbolTable* table);

// --------------------------------------------------------------------------
// Semantic Error Types and Reporting
// --------------------------------------------------------------------------

typedef enum {
    SEM_ERROR_NONE,
    SEM_ERROR_UNDECLARED_VARIABLE,
    SEM_ERROR_REDECLARED_VARIABLE,
    SEM_ERROR_TYPE_MISMATCH,
    SEM_ERROR_UNINITIALIZED_VARIABLE,
    SEM_ERROR_INVALID_OPERATION,
    SEM_ERROR_SEMANTIC_ERROR  // Generic semantic error
} SemanticErrorType;

void semantic_error(SemanticErrorType error, const char* name, int line);

// --------------------------------------------------------------------------
// Semantic Analysis Functions
// --------------------------------------------------------------------------

// Entry point for semantic analysis. Returns nonzero on success.
int analyze_semantics(ASTNode* ast);

// Check a variable declaration (no redeclaration in same scope).
int check_declaration(ASTNode* node, SymbolTable* table);

// Check a variable assignment (variable must be declared; mark as initialized).
int check_assignment(ASTNode* node, SymbolTable* table);

// Check an expression for type correctness and variable usage.
int check_expression(ASTNode* node, SymbolTable* table);

// Check a block of statements (handle scope creation and removal).
int check_block(ASTNode* node, SymbolTable* table);

// Check a condition (e.g., in if or while statements).
int check_condition(ASTNode* node, SymbolTable* table);

#endif /* SEMANTIC_H */
