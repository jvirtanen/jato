/*
 * Copyright (C) 2006  Pekka Enberg
 *
 * This file is released under the GPL version 2. Please refer to the file
 * LICENSE for details.
 */

#include <jit/basic-block.h>
#include <jit/compilation-unit.h>
#include <vm/buffer.h>

#include <stdlib.h>
#include <string.h>

struct compilation_unit *alloc_compilation_unit(struct methodblock *method)
{
	struct compilation_unit *cu = malloc(sizeof *cu);
	if (cu) {
		memset(cu, 0, sizeof *cu);
		INIT_LIST_HEAD(&cu->bb_list);
		cu->method = method;
		cu->expr_stack = alloc_stack();
		cu->is_compiled = false;
		cu->exit_bb = alloc_basic_block(cu, 0, 0);
		pthread_mutex_init(&cu->mutex, NULL);
	}
	return cu;
}

void free_compilation_unit(struct compilation_unit *cu)
{
	struct basic_block *bb, *tmp_bb;

	list_for_each_entry_safe(bb, tmp_bb, &cu->bb_list, bb_list_node)
		free_basic_block(bb);

	pthread_mutex_destroy(&cu->mutex);
	free_stack(cu->expr_stack);
	free_basic_block(cu->exit_bb);
	free_buffer(cu->objcode);
	free(cu);
}

/**
 * 	bb_find - Find basic block containing @offset.
 * 	@bb_list: First basic block in list.
 * 	@offset: Offset to find.
 * 
 * 	Find the basic block that contains the given offset and returns a
 * 	pointer to it.
 */
struct basic_block *find_bb(struct compilation_unit *cu, unsigned long offset)
{
	struct basic_block *bb;

	for_each_basic_block(bb, &cu->bb_list) {
		if (offset >= bb->start && offset < bb->end)
			return bb;
	}
	return NULL;
}

unsigned long nr_bblocks(struct compilation_unit *cu)
{
	struct basic_block *bb;
	unsigned long nr = 0;

	for_each_basic_block(bb, &cu->bb_list) {
		nr++;
	}

	return nr;
}
