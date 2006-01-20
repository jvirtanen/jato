/*
 * Copyright (C) 2006  Pekka Enberg
 */

#include <statement.h>
#include <stdlib.h>
#include <string.h>

struct statement *alloc_stmt(enum statement_type type)
{
	struct statement *stmt = malloc(sizeof(*stmt));
	if (!stmt)
		return NULL;
	memset(stmt, 0, sizeof(*stmt));
	stmt->s_type = type;

	return stmt;
}

void free_stmt(struct statement *stmt)
{
	if (stmt) {
		free_stmt(stmt->s_next);
		free(stmt->s_target);
		free(stmt->s_left);
		free(stmt->s_right);
		free(stmt);
	}
}
