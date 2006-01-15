#ifndef __STATEMENT_H
#define __STATEMENT_H

#include <operand.h>
#include <stddef.h>
#include <jam.h>

struct operand_stack;

enum statement_type {
	STMT_NOP,
	STMT_ASSIGN,
	STMT_NULL_CHECK,
	STMT_ARRAY_CHECK,
};

struct statement {
	enum statement_type s_type;	/* Type of the statement.  */
	struct operand *s_target;	/* Target temporary of the statement.  */
	struct operand *s_left;		/* Left operand of the statement.  */
	struct operand *s_right;	/* Left operand of the statement.  */
	struct statement *s_next;	/* Next statement. */
};

struct statement *convert_bytecode_to_stmts(struct classblock *,
					    unsigned char *, size_t,
					    struct operand_stack *);

struct statement *alloc_stmt(enum statement_type);
void free_stmt(struct statement *);

#endif
