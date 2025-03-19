#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "../../include/semantic.h"
#include "../../include/parser.h"
#include "../../include/lexer.h"

#define MAX_REPORTED_ERRORS 100

static char reportedErrors[MAX_REPORTED_ERRORS][100];
static int reportedErrorCount = 0;

bool errorAlreadyReported(const char* name) {
    for (int i = 0; i < reportedErrorCount; i++) {
        if (strcmp(reportedErrors[i], name) == 0) {
            return true;
        }
    }
    return false;
}

void addReportedError(const char* name) {
    if (reportedErrorCount < MAX_REPORTED_ERRORS) {
        strncpy(reportedErrors[reportedErrorCount], name, sizeof(reportedErrors[0]) - 1);
        reportedErrors[reportedErrorCount][sizeof(reportedErrors[0]) - 1] = '\0';
        reportedErrorCount++;
    }
}


SymbolTable* init_symbol_table() {
    SymbolTable* table = malloc(sizeof(SymbolTable));
    if (table) {
        table->head = NULL;
        table->current_scope = 0;
    }
    return table;
}

void add_symbol(SymbolTable* table, const char* name, int type, int line) {
    Symbol* symbol = malloc(sizeof(Symbol));
    if (symbol) {
        strncpy(symbol->name, name, sizeof(symbol->name) - 1);
        symbol->name[sizeof(symbol->name) - 1] = '\0';
        symbol->type = type;
        symbol->scope_level = table->current_scope;
        symbol->line_declared = line;
        symbol->is_initialized = 0;
        symbol->next = table->head;
        table->head = symbol;
    }
}

Symbol* lookup_symbol(SymbolTable* table, const char* name) {
    Symbol* current = table->head;
    while (current) {
        if (strcmp(current->name, name) == 0)
            return current;
        current = current->next;
    }
    return NULL;
}

Symbol* lookup_symbol_current_scope(SymbolTable* table, const char* name) {
    Symbol* current = table->head;
    while (current) {
        if ((strcmp(current->name, name) == 0) &&
            (current->scope_level == table->current_scope))
            return current;
        current = current->next;
    }
    return NULL;
}

void enter_scope(SymbolTable* table) {
    table->current_scope++;
}

void remove_symbols_in_current_scope(SymbolTable* table) {
    Symbol* current = table->head;
    Symbol* prev = NULL;
    while (current) {
        if (current->scope_level == table->current_scope) {
            if (prev == NULL) {
                table->head = current->next;
                free(current);
                current = table->head;
            } else {
                prev->next = current->next;
                free(current);
                current = prev->next;
            }
        } else {
            prev = current;
            current = current->next;
        }
    }
}

void exit_scope(SymbolTable* table) {
    if (table->current_scope > 0)
        table->current_scope--;
}

void free_symbol_table(SymbolTable* table) {
    Symbol* current = table->head;
    while (current) {
        Symbol* temp = current;
        current = current->next;
        free(temp);
    }
    free(table);
}

void print_symbol_table(SymbolTable* table) {
    Symbol* current = table->head;
    printf("Symbol Table Contents:\n");
    while (current) {
        printf("Name: %s, Type: %s, Scope: %d, Line: %d, Initialized: %s\n",
               current->name,
               (current->type == TOKEN_INT ? "int" : "unknown"),
               current->scope_level,
               current->line_declared,
               (current->is_initialized ? "Yes" : "No"));
        current = current->next;
    }
}

void dump_symbol_table(SymbolTable *table) {
    int count = 0;
    for (Symbol *sym = table->head; sym != NULL; sym = sym->next) {
        count++;
    }
    Symbol **symbols = malloc(count * sizeof(Symbol *));
    int index = 0;
    for (Symbol *sym = table->head; sym != NULL; sym = sym->next) {
        symbols[index++] = sym;
    }
    
    printf("== SYMBOL TABLE DUMP ==\n");
    printf("Total symbols: %d\n\n", count);
    
    for (int i = count - 1; i >= 0; i--) {
        int printedIndex = count - 1 - i;
        printf("Symbol[%d]:\n", printedIndex);
        printf("  Name: %s\n", symbols[i]->name);
        printf("  Type: %s\n", (symbols[i]->type == TOKEN_INT ? "int" : "unknown"));
        printf("  Scope Level: %d\n", symbols[i]->scope_level);
        printf("  Line Declared: %d\n", symbols[i]->line_declared);
        printf("  Initialized: %s\n\n", (symbols[i]->is_initialized ? "Yes" : "No"));
    }
    printf("===================\n");
    free(symbols);
}



void semantic_error(SemanticErrorType error, const char* name, int line) {
    printf("Semantic Error at line %d: ", line);
    switch (error) {
        case SEM_ERROR_UNDECLARED_VARIABLE:
            printf("Undeclared variable '%s'\n", name);
            break;
        case SEM_ERROR_REDECLARED_VARIABLE:
            printf("Variable '%s' already declared in this scope\n", name);
            break;
        case SEM_ERROR_TYPE_MISMATCH:
            printf("Type mismatch involving '%s'\n", name);
            break;
        case SEM_ERROR_UNINITIALIZED_VARIABLE:
            printf("Variable '%s' used without initialization\n", name);
            break;
        case SEM_ERROR_INVALID_OPERATION:
            printf("Invalid operation involving '%s'\n", name);
            break;
        default:
            printf("Generic semantic error with '%s'\n", name);
            break;
    }
}

// Forward declaration for statement checking helper.
static int check_statement(ASTNode* node, SymbolTable* table);

int check_declaration(ASTNode* node, SymbolTable* table) {
    if (!node || node->type != AST_VARDECL)
        return 0;
    const char* name = node->token.lexeme;
    Symbol* existing = lookup_symbol_current_scope(table, name);
    if (existing) {
        semantic_error(SEM_ERROR_REDECLARED_VARIABLE, name, node->token.line);
        return 0;
    }
    add_symbol(table, name, TOKEN_INT, node->token.line);
    if (node->right) {
        int initValid = check_expression(node->right, table);
        if (!initValid)
            return 0;
        Symbol* sym = lookup_symbol(table, name);
        if (sym)
            sym->is_initialized = 1;
    }
    return 1;
}

int check_assignment(ASTNode* node, SymbolTable* table) {
    if (!node || node->type != AST_ASSIGN)
        return 0;
    if (!node->left)
        return 0;
    const char* name = node->left->token.lexeme;
    Symbol* symbol = lookup_symbol(table, name);
    if (!symbol) {
        if (!errorAlreadyReported(name)) {
            semantic_error(SEM_ERROR_UNDECLARED_VARIABLE, name, node->token.line);
            addReportedError(name);
        }
        return 0;
    }
    symbol->is_initialized = 1;
    int rightValid = check_expression(node->right, table);
    return rightValid;
}

int check_expression(ASTNode* node, SymbolTable* table) {
    if (!node)
        return 0;
    if (node->type == AST_NUMBER) {
        return 1;
    } else if (node->type == AST_IDENTIFIER) {
        //printf("DEBUG: checking identifier '%s' at line %d\n", node->token.lexeme, node->token.line);
        Symbol* symbol = lookup_symbol(table, node->token.lexeme);
        if (!symbol) {
            if (!errorAlreadyReported(node->token.lexeme)) {
                semantic_error(SEM_ERROR_UNDECLARED_VARIABLE, node->token.lexeme, node->token.line);
                addReportedError(node->token.lexeme);
            }
            return 0;
        }
        if (!symbol->is_initialized) {
            semantic_error(SEM_ERROR_UNINITIALIZED_VARIABLE, node->token.lexeme, node->token.line);
            return 0;
        }
        return 1;
    } else if (node->type == AST_BINOP) {
        int left_valid = check_expression(node->left, table);
        int right_valid = check_expression(node->right, table);
        return left_valid & right_valid;
    } else if (node->type == AST_FUNC_CALL) {
        if (node->left->type != AST_IDENTIFIER) {
            semantic_error(SEM_ERROR_INVALID_OPERATION, "Invalid function call", node->token.line);
            return 0;
        }
        if (strcmp(node->left->token.lexeme, "factorial") != 0) {
            semantic_error(SEM_ERROR_INVALID_OPERATION, node->left->token.lexeme, node->token.line);
            return 0;
        }
        return check_expression(node->right, table);
    }
    return 1;
}

int check_block(ASTNode* node, SymbolTable* table) {
    if (!node || node->type != AST_BLOCK)
        return 0;
    int result = 1;
    enter_scope(table);
    ASTNode* stmt = node->left;
    while (stmt) {
        result = check_statement(stmt, table) & result;
        stmt = stmt->next;
    }
    exit_scope(table);
    return result;
}

int check_condition(ASTNode* node, SymbolTable* table) {
    if (!node)
        return 0;
    return check_expression(node->left, table);
}

static int check_statement(ASTNode* node, SymbolTable* table) {
    int result = 1;
    if (!node)
        return 1;
    switch (node->type) {
        case AST_VARDECL:
            result = check_declaration(node, table);
            break;
        case AST_ASSIGN:
            result = check_assignment(node, table);
            break;
        case AST_PRINT:
            result = check_expression(node->left, table);
            break;
        case AST_IF: {
            int condValid = check_expression(node->left, table);
            int thenValid = (node->right) ? check_block(node->right, table) : 1;
            int elseValid = (node->next) ? check_block(node->next, table) : 1;
            result = condValid & thenValid & elseValid;
            break;
        }
        case AST_WHILE: {
            int condValid = check_expression(node->left, table);
            int bodyValid = (node->right) ? check_block(node->right, table) : 1;
            result = condValid & bodyValid;
            break;
        }
        case AST_BLOCK:
            result = check_block(node, table);
            break;
        default:
            result = check_expression(node, table);
            break;
    }
    return result;
}

static int check_program(ASTNode* node, SymbolTable* table) {
    if (!node)
        return 1;
    int result = 1;
    if (node->left)
        result &= check_statement(node->left, table);
    if (node->right)
        result &= check_program(node->right, table);
    if (node->next)
        result &= check_program(node->next, table);
    return result;
}

int analyze_semantics(ASTNode* ast) {
    reportedErrorCount = 0;
    SymbolTable* table = init_symbol_table();
    int result = check_program(ast, table);
    if (result) {
        dump_symbol_table(table); 
    }
    free_symbol_table(table);
    return result;
}


int main(void) {
    const char *filePath = "./test/input_semantic_error.txt"; 

    FILE *fp = fopen(filePath, "r");
    if (!fp) {
        perror("Error opening file");
        return 1;
    }

    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    rewind(fp);

    char *input = malloc(filesize + 1);
    if (!input) {
        perror("Memory allocation error");
        fclose(fp);
        return 1;
    }

    size_t bytesRead = fread(input, 1, filesize, fp);
    input[bytesRead] = '\0'; 
    fclose(fp);

    printf("Input file content from '%s':\n%s\n\n", filePath, input);

    parser_init(input);
    ASTNode* ast = parse();

    printf("AST created. Performing semantic analysis...\n\n");
    int result = analyze_semantics(ast);

    if (result) {
        printf("Semantic analysis successful. No errors found.\n");
    } else {
        printf("Semantic analysis failed. Errors detected.\n");
    }

    // printf("\nAbstract Syntax Tree:\n");
    // print_ast(ast, 0);

    free_ast(ast);
    free(input);

    return 0;
}

