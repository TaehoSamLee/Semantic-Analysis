/* parser.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/parser.h"
#include "../../include/lexer.h"
#include "../../include/tokens.h"

/* Rename the parser's Symbol struct to ParserSymbol to avoid conflict with semantic's Symbol */
typedef struct ParserSymbol {
    char name[100];
    struct ParserSymbol *next;
} ParserSymbol;

static ParserSymbol *current_scope = NULL;

void push_scope() {
    // Create a new empty scope by pushing a marker onto the stack.
    ParserSymbol *new_scope = malloc(sizeof(ParserSymbol));
    new_scope->name[0] = '\0';  // marker (empty name)
    new_scope->next = current_scope;
    current_scope = new_scope;
}

void pop_scope() {
    if (current_scope) {
        ParserSymbol *old = current_scope;
        current_scope = current_scope->next;
        free(old);
    }
}

/* Renamed function: add_parser_symbol */
void add_parser_symbol(const char *name) {
    ParserSymbol *sym = malloc(sizeof(ParserSymbol));
    strcpy(sym->name, name);
    sym->next = current_scope;
    current_scope = sym;
}

// --------------------------------------------------------------------------
// Parser implementation
// --------------------------------------------------------------------------

/* TODO 1: Add more parsing function declarations for:
// - if statements: if (condition) { ... }
static ASTNode *parse_if_statement(void);
// - while loops: while (condition) { ... }
static ASTNode *parse_while_statement(void);
// - repeat-until: repeat { ... } until (condition)
static ASTNode *parse_repeat_statement(void);
// - print statements: print x;
static ASTNode *parse_print_statement(void);
// - blocks: { statement1; statement2; }
static ASTNode *parse_block(void);
// - factorial function: factorial(x)
static ASTNode *parse_function_call(ASTNode *identifierNode);
*/

static ASTNode *parse_if_statement(void);
static ASTNode *parse_while_statement(void);
static ASTNode *parse_repeat_statement(void);
static ASTNode *parse_print_statement(void);
static ASTNode *parse_block(void);
static ASTNode *parse_function_call(ASTNode *identifierNode);

static ASTNode *parse_bool_expression(void);
static ASTNode *parse_factor(void);
static ASTNode *parse_term(void);
static ASTNode *parse_expression(void);

static int is_comparison_operator(const char *op) {
    return (strcmp(op, "<") == 0 ||
            strcmp(op, ">") == 0 ||
            strcmp(op, "==") == 0 ||
            strcmp(op, "!=") == 0);
}

// Current token being processed
static Token current_token;
static int position = 0;
static const char *source;

static void parse_error(ParseError error, Token token) {
    printf("Parse Error at line %d: ", token.line);
    switch (error) {
        case PARSE_ERROR_UNEXPECTED_TOKEN:
            printf("Unexpected token '%s'\n", token.lexeme);
            break;
        case PARSE_ERROR_MISSING_SEMICOLON:
            printf("Missing semicolon after '%s'\n", token.lexeme);
            break;
        case PARSE_ERROR_MISSING_IDENTIFIER:
            printf("Expected identifier after '%s'\n", token.lexeme);
            break;
        case PARSE_ERROR_MISSING_EQUALS:
            printf("Expected '=' after '%s'\n", token.lexeme);
            break;
        case PARSE_ERROR_INVALID_EXPRESSION:
            printf("Invalid expression after '%s'\n", token.lexeme);
            break;
        case PARSE_ERROR_MISSING_LPAREN:
            printf("Missing '(' near '%s'\n", token.lexeme);
            break;
        case PARSE_ERROR_MISSING_RPAREN:
            printf("Missing ')' near '%s'\n", token.lexeme);
            break;
        case PARSE_ERROR_MISSING_BLOCK:
            printf("Missing block braces near '%s'\n", token.lexeme);
            break;
        case PARSE_ERROR_INVALID_OPERATOR:
            printf("Invalid operator '%s'\n", token.lexeme);
            break;
        case PARSE_ERROR_FUNCTION_CALL_ERROR:
            printf("Function call error near '%s'\n", token.lexeme);
            break;
        default:
            printf("Unknown error\n");
    }
}

static void advance(void) {
    current_token = get_next_token(source, &position);
}

static ASTNode *create_node(ASTNodeType type) {
    ASTNode *node = malloc(sizeof(ASTNode));
    if (node) {
        node->type = type;
        node->token = current_token;
        node->left = NULL;
        node->right = NULL;
        node->next = NULL;  // Initialize the chaining pointer
    }
    return node;
}

static int match(TokenType type) {
    return current_token.type == type;
}

static void synchronize() {
    while (!match(TOKEN_SEMICOLON) && !match(TOKEN_RBRACE) && current_token.type != TOKEN_EOF) {
        advance();
    }
    if (match(TOKEN_SEMICOLON)) {
        advance();
    }
}

static void expect(TokenType type) {
    if (match(type)) {
        advance();
    } else {
        parse_error(PARSE_ERROR_UNEXPECTED_TOKEN, current_token);
        synchronize();
    }
}

// Forward declaration
static ASTNode *parse_statement(void);

static ASTNode *parse_if_statement(void) {
    ASTNode *node = create_node(AST_IF);
    advance();  // consume 'if'
    
    if (!match(TOKEN_LPAREN)) {
        printf("Parse Error at line %d: Expected '(' after 'if', but found '%s'\n", current_token.line, current_token.lexeme);
        synchronize();
        exit(1);
    }
    advance(); // consume '('

    node->left = parse_bool_expression();
    if (!match(TOKEN_RPAREN)) {
        printf("Parse Error at line %d: Expected ')' after if condition, but found '%s'\n", current_token.line, current_token.lexeme);
        synchronize();
        exit(1);
    }
    advance(); // consume ')'
    
    ASTNode *thenBlock = parse_block();
    node->right = thenBlock;
    
    if (match(TOKEN_ELSE)) {
        advance(); // consume 'else'
        ASTNode *elseBlock = parse_block();
        thenBlock->right = elseBlock;
    }
    return node;
}

static ASTNode *parse_while_statement(void) {
    ASTNode *node = create_node(AST_WHILE);
    advance();  // consume 'while'
    if (!match(TOKEN_LPAREN)) {
        printf("Parse Error at line %d: Expected '(' after 'while', but found '%s'\n", current_token.line, current_token.lexeme);
        synchronize();
        exit(1);
    }
    advance(); // consume '('
    node->left = parse_bool_expression();
    if (!match(TOKEN_RPAREN)) {
        printf("Parse Error at line %d: Expected ')' after while condition, but found '%s'\n", current_token.line, current_token.lexeme);
        synchronize();
        exit(1);
    }
    advance(); // consume ')'
    node->right = parse_block();
    return node;
}

static ASTNode *parse_repeat_statement(void) {
    ASTNode *node = create_node(AST_REPEAT);
    advance(); // consume 'repeat'
    node->left = parse_block();
    if (!match(TOKEN_UNTIL)) {
        printf("Parse Error at line %d: Expected 'until' after repeat block, but found '%s'\n", current_token.line, current_token.lexeme);
        exit(1);
    }
    advance(); // consume 'until'
    if (!match(TOKEN_LPAREN)) {
        printf("Parse Error at line %d: Expected '(' after 'until', but found '%s'\n", current_token.line, current_token.lexeme);
        synchronize();
        exit(1);
    }
    advance();
    ASTNode *condition = parse_bool_expression();
    node->right = condition;
    if (!match(TOKEN_RPAREN)) {
        printf("Parse Error at line %d: Expected ')' after repeat condition, but found '%s'\n", current_token.line, current_token.lexeme);
        synchronize();
        exit(1);
    }
    advance();
    if (!match(TOKEN_SEMICOLON)) {
        printf("Parse Error at line %d: Expected ';' after repeat statement, but found '%s'\n", current_token.line, current_token.lexeme);
        exit(1);
    }
    advance(); // consume ';'
    return node;
}

static ASTNode *parse_print_statement(void) {
    ASTNode *node = create_node(AST_PRINT);
    advance(); // consume 'print'
    node->left = parse_expression();
    if (!match(TOKEN_SEMICOLON)) {
        printf("Parse Error at line %d: Expected ';' after print statement, but found '%s'\n", current_token.line, current_token.lexeme);
        exit(1);
    }
    advance(); // consume ';'
    return node;
}

static ASTNode *parse_block(void) {
    push_scope();
    ASTNode *node = create_node(AST_BLOCK);
    expect(TOKEN_LBRACE);
    ASTNode *firstStmt = NULL;
    ASTNode *currentStmt = NULL;
    while (!match(TOKEN_RBRACE) && !match(TOKEN_EOF)) {
        ASTNode *stmt = parse_statement();
        if (firstStmt == NULL) {
            firstStmt = stmt;
            currentStmt = stmt;
        } else {
            currentStmt->next = stmt;
            currentStmt = stmt;
        }
    }
    expect(TOKEN_RBRACE);
    node->left = firstStmt;  // The block's statements are in the left subtree
    pop_scope();
    return node;
}

static ASTNode *parse_function_call(ASTNode *identifierNode) {
    ASTNode *node = create_node(AST_FUNC_CALL);
    node->left = identifierNode;
    expect(TOKEN_LPAREN);
    node->right = parse_expression();
    expect(TOKEN_RPAREN);
    return node;
}

static ASTNode *parse_expression(void);

static ASTNode *parse_declaration(void) {
    ASTNode *node = create_node(AST_VARDECL);
    advance(); // consume 'int'

    if (!match(TOKEN_IDENTIFIER)) {
        printf("Parse Error at line %d: Expected identifier after 'int', but found '%s'\n", current_token.line, current_token.lexeme);
        exit(1);
    }
    
    node->token = current_token;
    /* Use the renamed function for the parser's own symbol table */
    add_parser_symbol(current_token.lexeme);
    advance();

    if (match(TOKEN_EQUALS)) {
        advance(); // consume '='
        ASTNode *initExpr = parse_expression();
        node->right = initExpr;
    }

    if (!match(TOKEN_SEMICOLON)) {
        printf("Parse Error at line %d: Expected ';' at end of declaration, but found '%s'\n", current_token.line, current_token.lexeme);
        exit(1);
    }
    
    advance();
    return node;
}

static ASTNode *parse_assignment(void) {
    ASTNode *node = create_node(AST_ASSIGN);
    node->left = create_node(AST_IDENTIFIER);
    node->left->token = current_token;
    advance();

    if (!match(TOKEN_EQUALS)) {
        printf("Parse Error at line %d: Expected '=' after identifier in assignment, but found '%s'\n", current_token.line, current_token.lexeme);
        exit(1);
    }
    advance();

    node->right = parse_expression();

    if (!match(TOKEN_SEMICOLON)) {
        printf("Parse Error at line %d: Expected ';' after assignment, but found '%s'\n", current_token.line, current_token.lexeme);
        exit(1);
    }
    advance();
    return node;
}

static ASTNode *parse_statement(void) {
    if (match(TOKEN_INT)) {
        return parse_declaration();
    } else if (match(TOKEN_IDENTIFIER)) {
        return parse_assignment();
    } else if (match(TOKEN_IF)) {
        return parse_if_statement();
    } else if (match(TOKEN_WHILE)) {
        return parse_while_statement();
    } else if (match(TOKEN_REPEAT)) {
        return parse_repeat_statement();
    } else if (match(TOKEN_PRINT)) {
        return parse_print_statement();
    } else if (match(TOKEN_LBRACE)) {
        return parse_block();
    }

    printf("Syntax Error: Unexpected token '%s' at line %d\n", current_token.lexeme, current_token.line);
    exit(1);
    return NULL;
}

static ASTNode *parse_factor(void) {
    ASTNode *node = NULL;
    if (match(TOKEN_NUMBER)) {
        node = create_node(AST_NUMBER);
        advance();
    } else if (match(TOKEN_IDENTIFIER)) {
        //printf("DEBUG: creating identifier node for '%s' at line %d\n", current_token.lexeme, current_token.line);
        node = create_node(AST_IDENTIFIER);
        node->token = current_token;
        advance();
        if (match(TOKEN_LPAREN)) {
            node = parse_function_call(node);
        }
    } else if (match(TOKEN_LPAREN)) {
        advance();
        node = parse_expression();
        expect(TOKEN_RPAREN);
    } else {
        printf("Parse Error at line %d: Expected number, identifier, or '(' in expression, but found '%s'\n", current_token.line, current_token.lexeme);
        exit(1);
    }
    return node;
}

static ASTNode *parse_term(void) {
    ASTNode *node = parse_factor();
    while (match(TOKEN_OPERATOR) &&
          (current_token.lexeme[0] == '*' || current_token.lexeme[0] == '/')) {
        ASTNode *binOpNode = create_node(AST_BINOP);
        binOpNode->token = current_token;
        advance();
        binOpNode->left = node;
        binOpNode->right = parse_factor();
        node = binOpNode;
    }
    return node;
}

static ASTNode *parse_expression(void) {
    ASTNode *node = parse_term();
    while (match(TOKEN_OPERATOR) &&
          (current_token.lexeme[0] == '+' || current_token.lexeme[0] == '-')) {
        ASTNode *binOpNode = create_node(AST_BINOP);
        binOpNode->token = current_token;
        advance();
        binOpNode->left = node;
        binOpNode->right = parse_term();
        node = binOpNode;
    }
    return node;
}

static ASTNode *parse_bool_expression(void) {
    ASTNode *node = parse_expression();
    while (match(TOKEN_OPERATOR) && is_comparison_operator(current_token.lexeme)) {
        ASTNode *binOpNode = create_node(AST_BINOP);
        binOpNode->token = current_token;
        advance();
        binOpNode->left = node;
        binOpNode->right = parse_expression();
        node = binOpNode;
    }
    return node;
}

static ASTNode *parse_program(void) {
    ASTNode *program = create_node(AST_PROGRAM);
    ASTNode *current = program;
    while (!match(TOKEN_EOF)) {
        current->left = parse_statement();
        if (!match(TOKEN_EOF)) {
            current->next = create_node(AST_PROGRAM);
            current = current->next;
        }
    }
    return program;
}

void parser_init(const char *input) {
    source = input;
    position = 0;
    advance();
}

ASTNode *parse(void) {
    return parse_program();
}

void print_ast(ASTNode *node, int level) {
    if (!node) return;
    for (int i = 0; i < level; i++) printf("  ");
    // Print node info based on type...
    switch (node->type) {
        case AST_PROGRAM:
            printf("Program\n");
            break;
        case AST_VARDECL:
            printf("VarDecl: %s\n", node->token.lexeme);
            break;
        case AST_ASSIGN:
            printf("Assign\n");
            break;
        case AST_NUMBER:
            printf("Number: %s\n", node->token.lexeme);
            break;
        case AST_IDENTIFIER:
            printf("Identifier: %s\n", node->token.lexeme);
            break;
        case AST_IF:
            printf("If Statement\n");
            break;
        case AST_WHILE:
            printf("While Loop\n");
            break;
        case AST_REPEAT:
            printf("Repeat-Until Loop\n");
            break;
        case AST_BLOCK:
            printf("Block\n");
            break;
        case AST_BINOP:
            printf("BinaryOp: %s\n", node->token.lexeme);
            break;
        case AST_PRINT:
            printf("Print Statement\n");
            break;
        case AST_FUNC_CALL:
            printf("Function Call: %s\n", node->left->token.lexeme);
            break;
        default:
            printf("Unknown node type\n");
    }
    // First, print the node's children (left/right)
    print_ast(node->left, level + 1);
    print_ast(node->right, level + 1);
    // Then print the next statement in the chain
    print_ast(node->next, level);
}


void free_ast(ASTNode *node) {
    if (!node) return;
    free_ast(node->left);
    free_ast(node->right);
    free(node);
}

/* 
// If you have a main() in parser.c for testing purposes, make sure to conditionally compile it
int main() {
    // Your test code here
    return 0;
}
*/

// int main() {
//     // Change the filename below to test different inputs.
//     //Correct input
//     //const char *filename = "./test/input_correct_par.txt";
//     //Incorrect input
//     const char *filename = "./test/input_incorrect_par_3.txt";
    
//     FILE *file = fopen(filename, "r");
//     fseek(file, 0, SEEK_END);
//     long file_size = ftell(file);
//     rewind(file);
//     char *input = malloc(file_size + 1);
    
//     fread(input, 1, file_size, file);
//     input[file_size] = '\0';
//     fclose(file);
    
//     printf("Parsing file: %s\n", filename);
//     parser_init(input);
//     ASTNode *ast = parse();
    
//     printf("\nAbstract Syntax Tree:\n");
//     print_ast(ast, 0);
    
//     free_ast(ast);
//     free(input);
    
//     return 0;
// }