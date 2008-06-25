/*
 * Copyright (C) 2005-2006  Pekka Enberg
 */

#include <bc-test-utils.h>
#include <args-test-utils.h>
#include <jit/compilation-unit.h>
#include <jit/expression.h>
#include <jit/compiler.h>
#include <libharness.h>
#include <vm/stack.h>
#include <vm/system.h>
#include <vm/types.h>
#include <vm/vm.h>

#include <test/vm.h>

static void convert_ir_const_single(struct compilation_unit *cu, void *value)
{
	u8 cp_infos[] = { (unsigned long) value };
	u1 cp_types[] = { CONSTANT_Resolved };

	convert_ir_const(cu, (void *)cp_infos, 8, cp_types);
}

struct type_mapping {
	char *type;
	enum vm_type vm_type;
};

static struct type_mapping types[] = {
	{ "B", J_BYTE },
	{ "C", J_CHAR },
	{ "D", J_DOUBLE },
	{ "F", J_FLOAT },
	{ "I", J_INT },
	{ "J", J_LONG },
	{ "L/java/long/Object", J_REFERENCE },
	{ "S", J_SHORT },
	{ "Z", J_BOOLEAN },
};

static void __assert_convert_getstatic(unsigned char opc,
				       enum vm_type expected_vm_type,
				       char *field_type)
{
	struct fieldblock fb;
	struct expression *expr;
	unsigned char code[] = { opc, 0x00, 0x00 };
	struct methodblock method = {
		.jit_code = code,
		.code_size = ARRAY_SIZE(code),
	};
	struct compilation_unit *cu;

	fb.type = field_type;

	cu = alloc_simple_compilation_unit(&method);

	convert_ir_const_single(cu, &fb);
	expr = stack_pop(cu->expr_stack);
	assert_class_field_expr(expected_vm_type, &fb, &expr->node);
	assert_true(stack_is_empty(cu->expr_stack));

	expr_put(expr);
	free_compilation_unit(cu);
}

static void assert_convert_getstatic(unsigned char opc)
{
	unsigned long i;

	for (i = 0; i < ARRAY_SIZE(types); i++)
		__assert_convert_getstatic(opc, types[i].vm_type, types[i].type);
}

void test_convert_getstatic(void)
{
	assert_convert_getstatic(OPC_GETSTATIC);
}

static void __assert_convert_getfield(unsigned char opc,
				      enum vm_type expected_vm_type,
				      char *field_type)
{
	struct fieldblock fb;
	struct expression *expr;
	struct expression *objectref;
	unsigned char code[] = { opc, 0x00, 0x00 };
	struct methodblock method = {
		.jit_code = code,
		.code_size = ARRAY_SIZE(code),
	};
	struct compilation_unit *cu;

	fb.type = field_type;

	cu = alloc_simple_compilation_unit(&method);

	objectref = value_expr(J_REFERENCE, 0xdeadbeef);
	stack_push(cu->expr_stack, objectref);
	convert_ir_const_single(cu, &fb);
	expr = stack_pop(cu->expr_stack);
	assert_instance_field_expr(expected_vm_type, &fb, objectref, &expr->node);
	assert_true(stack_is_empty(cu->expr_stack));

	expr_put(expr);
	free_compilation_unit(cu);
}


static void assert_convert_getfield(unsigned char opc)
{
	unsigned long i;

	for (i = 0; i < ARRAY_SIZE(types); i++)
		__assert_convert_getfield(opc, types[i].vm_type, types[i].type);
}

void test_convert_getfield(void)
{
	assert_convert_getfield(OPC_GETFIELD);
}

static void __assert_convert_putstatic(unsigned char opc,
				       enum vm_type expected_vm_type,
				       char *field_type)
{
	struct fieldblock fb;
	struct statement *stmt;
	unsigned char code[] = { opc, 0x00, 0x00 };
	struct methodblock method = {
		.jit_code = code,
		.code_size = ARRAY_SIZE(code),
	};
	struct compilation_unit *cu;
	struct expression *value;

	fb.type = field_type;
	value = value_expr(expected_vm_type, 0xdeadbeef);
	cu = alloc_simple_compilation_unit(&method);
	stack_push(cu->expr_stack, value);
	convert_ir_const_single(cu, &fb);
	stmt = stmt_entry(bb_entry(cu->bb_list.next)->stmt_list.next);

	assert_store_stmt(stmt);
	assert_class_field_expr(expected_vm_type, &fb, stmt->store_dest);
	assert_ptr_equals(value, to_expr(stmt->store_src));
	assert_true(stack_is_empty(cu->expr_stack));

	free_compilation_unit(cu);
}

static void assert_convert_putstatic(unsigned char opc)
{
	unsigned long i;

	for (i = 0; i < ARRAY_SIZE(types); i++)
		__assert_convert_putstatic(opc, types[i].vm_type, types[i].type);
}

void test_convert_putstatic(void)
{
	assert_convert_putstatic(OPC_PUTSTATIC);
}

static void __assert_convert_putfield(unsigned char opc,
				      enum vm_type expected_vm_type,
				      char *field_type)
{
	struct fieldblock fb;
	struct statement *stmt;
	unsigned char code[] = { opc, 0x00, 0x00 };
	struct methodblock method = {
		.jit_code = code,
		.code_size = ARRAY_SIZE(code),
	};
	struct compilation_unit *cu;
	struct expression *objectref;
	struct expression *value;

	fb.type = field_type;
	cu = alloc_simple_compilation_unit(&method);

	objectref = value_expr(J_REFERENCE, 0xdeadbeef);
	stack_push(cu->expr_stack, objectref);

	value = value_expr(expected_vm_type, 0xdeadbeef);
	stack_push(cu->expr_stack, value);

	convert_ir_const_single(cu, &fb);
	stmt = stmt_entry(bb_entry(cu->bb_list.next)->stmt_list.next);

	assert_store_stmt(stmt);
	assert_instance_field_expr(expected_vm_type, &fb, objectref, stmt->store_dest);
	assert_ptr_equals(value, to_expr(stmt->store_src));
	assert_true(stack_is_empty(cu->expr_stack));

	free_compilation_unit(cu);
}

static void assert_convert_putfield(unsigned char opc)
{
	unsigned long i;

	for (i = 0; i < ARRAY_SIZE(types); i++)
		__assert_convert_putfield(opc, types[i].vm_type, types[i].type);
}

void test_convert_putfield(void)
{
	assert_convert_putfield(OPC_PUTFIELD);
}

static void assert_convert_array_load(enum vm_type expected_type,
				      unsigned char opc,
				      unsigned long arrayref,
				      unsigned long index)
{
	unsigned char code[] = { opc };
	struct expression *arrayref_expr, *index_expr, *temporary_expr;
	struct statement *stmt;
	struct methodblock method = {
		.jit_code = code,
		.code_size = ARRAY_SIZE(code),
	};
	struct compilation_unit *cu;
	unsigned long expected_temporary;

	cu = alloc_simple_compilation_unit(&method);

	arrayref_expr = value_expr(J_REFERENCE, arrayref);
	index_expr = value_expr(J_INT, index);

	stack_push(cu->expr_stack, arrayref_expr);
	stack_push(cu->expr_stack, index_expr);

	convert_to_ir(cu);
	stmt = stmt_entry(bb_entry(cu->bb_list.next)->stmt_list.next);

	struct statement *nullcheck = stmt;
	struct statement *arraycheck = stmt_entry(nullcheck->stmt_list_node.next);
	struct statement *store_stmt = stmt_entry(arraycheck->stmt_list_node.next);

	assert_null_check_stmt(arrayref_expr, nullcheck);
	assert_arraycheck_stmt(expected_type, arrayref_expr, index_expr, arraycheck);

	assert_store_stmt(store_stmt);
	assert_array_deref_expr(expected_type, arrayref_expr, index_expr,
				store_stmt->store_src);

	temporary_expr = stack_pop(cu->expr_stack);

	expected_temporary = to_expr(store_stmt->store_dest)->temporary;
	assert_temporary_expr(expected_temporary, &temporary_expr->node);
	expr_put(temporary_expr);
	assert_true(stack_is_empty(cu->expr_stack));

	free_compilation_unit(cu);
}

void test_convert_iaload(void)
{
	assert_convert_array_load(J_INT, OPC_IALOAD, 0, 1);
	assert_convert_array_load(J_INT, OPC_IALOAD, 1, 2);
}

void test_convert_laload(void)
{
	assert_convert_array_load(J_LONG, OPC_LALOAD, 0, 1);
	assert_convert_array_load(J_LONG, OPC_LALOAD, 1, 2);
}

void test_convert_faload(void)
{
	assert_convert_array_load(J_FLOAT, OPC_FALOAD, 0, 1);
	assert_convert_array_load(J_FLOAT, OPC_FALOAD, 1, 2);
}

void test_convert_daload(void)
{
	assert_convert_array_load(J_DOUBLE, OPC_DALOAD, 0, 1);
	assert_convert_array_load(J_DOUBLE, OPC_DALOAD, 1, 2);
}

void test_convert_aaload(void)
{
	assert_convert_array_load(J_REFERENCE, OPC_AALOAD, 0, 1);
	assert_convert_array_load(J_REFERENCE, OPC_AALOAD, 1, 2);
}

void test_convert_baload(void)
{
	assert_convert_array_load(J_INT, OPC_BALOAD, 0, 1);
	assert_convert_array_load(J_INT, OPC_BALOAD, 1, 2);
}

void test_convert_caload(void)
{
	assert_convert_array_load(J_CHAR, OPC_CALOAD, 0, 1);
	assert_convert_array_load(J_CHAR, OPC_CALOAD, 1, 2);
}

void test_convert_saload(void)
{
	assert_convert_array_load(J_SHORT, OPC_SALOAD, 0, 1);
	assert_convert_array_load(J_SHORT, OPC_SALOAD, 1, 2);
}

static void assert_convert_array_store(enum vm_type expected_type,
				       unsigned char opc,
				       unsigned long arrayref,
				       unsigned long index, unsigned long value)
{
	unsigned char code[] = { opc };
	struct expression *arrayref_expr, *index_expr, *expr;
	struct statement *stmt;
	struct compilation_unit *cu;
	struct methodblock method = {
		.jit_code = code,
		.code_size = ARRAY_SIZE(code),
	};

	cu = alloc_simple_compilation_unit(&method);

	arrayref_expr = value_expr(J_REFERENCE, arrayref);
	index_expr = value_expr(J_INT, index);
	expr = temporary_expr(expected_type, value);

	stack_push(cu->expr_stack, arrayref_expr);
	stack_push(cu->expr_stack, index_expr);
	stack_push(cu->expr_stack, expr);

	convert_to_ir(cu);
	stmt = stmt_entry(bb_entry(cu->bb_list.next)->stmt_list.next);

	struct statement *nullcheck = stmt;
	struct statement *arraycheck = stmt_entry(nullcheck->stmt_list_node.next);
	struct statement *store_stmt = stmt_entry(arraycheck->stmt_list_node.next);

	assert_null_check_stmt(arrayref_expr, nullcheck);
	assert_arraycheck_stmt(expected_type, arrayref_expr, index_expr,
			       arraycheck);

	assert_store_stmt(store_stmt);
	assert_array_deref_expr(expected_type, arrayref_expr, index_expr,
				store_stmt->store_dest);
	assert_temporary_expr(value, store_stmt->store_src);

	assert_true(stack_is_empty(cu->expr_stack));

	free_compilation_unit(cu);
}

void test_convert_iastore(void)
{
	assert_convert_array_store(J_INT, OPC_IASTORE, 0, 1, 2);
	assert_convert_array_store(J_INT, OPC_IASTORE, 2, 3, 4);
}

void test_convert_lastore(void)
{
	assert_convert_array_store(J_LONG, OPC_LASTORE, 0, 1, 2);
	assert_convert_array_store(J_LONG, OPC_LASTORE, 2, 3, 4);
}

void test_convert_fastore(void)
{
	assert_convert_array_store(J_FLOAT, OPC_FASTORE, 0, 1, 2);
	assert_convert_array_store(J_FLOAT, OPC_FASTORE, 2, 3, 4);
}

void test_convert_dastore(void)
{
	assert_convert_array_store(J_DOUBLE, OPC_DASTORE, 0, 1, 2);
	assert_convert_array_store(J_DOUBLE, OPC_DASTORE, 2, 3, 4);
}

void test_convert_aastore(void)
{
	assert_convert_array_store(J_REFERENCE, OPC_AASTORE, 0, 1, 2);
	assert_convert_array_store(J_REFERENCE, OPC_AASTORE, 2, 3, 4);
}

void test_convert_bastore(void)
{
	assert_convert_array_store(J_INT, OPC_BASTORE, 0, 1, 2);
	assert_convert_array_store(J_INT, OPC_BASTORE, 2, 3, 4);
}

void test_convert_castore(void)
{
	assert_convert_array_store(J_CHAR, OPC_CASTORE, 0, 1, 2);
	assert_convert_array_store(J_CHAR, OPC_CASTORE, 2, 3, 4);
}

void test_convert_sastore(void)
{
	assert_convert_array_store(J_SHORT, OPC_SASTORE, 0, 1, 2);
	assert_convert_array_store(J_SHORT, OPC_SASTORE, 2, 3, 4);
}

static void assert_convert_new(unsigned long expected_type_idx,
			       unsigned char idx_1, unsigned char idx_2)
{
	struct object *instance_class = new_class();
	unsigned char code[] = { OPC_NEW, 0x0, 0x0 };
	struct compilation_unit *cu;
	struct expression *new_expr;
	struct methodblock method = {
		.jit_code = code,
		.code_size = ARRAY_SIZE(code),
	};

	cu = alloc_simple_compilation_unit(&method);
	convert_ir_const_single(cu, instance_class);

	new_expr = stack_pop(cu->expr_stack);
	assert_int_equals(EXPR_NEW, expr_type(new_expr));
	assert_int_equals(J_REFERENCE, new_expr->vm_type);
	assert_ptr_equals(instance_class, new_expr->class);

	free_expression(new_expr);
	free_compilation_unit(cu);

	free(instance_class);
}

void test_convert_new(void)
{
	assert_convert_new(0xcafe, 0xca, 0xfe);
}

void test_convert_anewarray(void)
{
	struct object *instance_class = new_class();
	unsigned char code[] = { OPC_ANEWARRAY, 0x00, 0x00 };
	struct compilation_unit *cu;
	struct expression *size,*arrayref;
	struct methodblock method = {
		.jit_code = code,
		.code_size = ARRAY_SIZE(code),
	};

	cu = alloc_simple_compilation_unit(&method);
	size = value_expr(J_INT, 0xff);
	stack_push(cu->expr_stack, size);

	convert_ir_const_single(cu, instance_class);

	arrayref = stack_pop(cu->expr_stack);

	assert_int_equals(EXPR_ANEWARRAY, expr_type(arrayref));
	assert_int_equals(J_REFERENCE, arrayref->vm_type);
	assert_ptr_equals(size, to_expr(arrayref->anewarray_size));
	assert_ptr_equals(instance_class, arrayref->anewarray_ref_type);

	expr_put(arrayref);
	free_compilation_unit(cu);
	free(instance_class);
}

void test_convert_newarray(void)
{
	unsigned char code[] = { OPC_NEWARRAY, T_INT };
	struct methodblock method = {
		.jit_code = code,
		.code_size = ARRAY_SIZE(code),
	};
	struct expression *size, *arrayref;
	struct compilation_unit *cu;

	cu = alloc_simple_compilation_unit(&method);

	size = value_expr(J_INT, 0xff);
	stack_push(cu->expr_stack, size);

	convert_to_ir(cu);

	arrayref = stack_pop(cu->expr_stack);
	assert_int_equals(EXPR_NEWARRAY, expr_type(arrayref));
	assert_int_equals(J_REFERENCE, arrayref->vm_type);
	assert_ptr_equals(size, to_expr(arrayref->array_size));
	assert_int_equals(T_INT, arrayref->array_type);

	expr_put(arrayref);
	free_compilation_unit(cu);
}

void test_convert_multianewarray(void)
{
	struct object *instance_class = new_class();
	unsigned char dimension = 0x02;
	unsigned char code[] = { OPC_MULTIANEWARRAY, 0x00, 0x00, dimension };
	struct compilation_unit *cu;
	struct expression *arrayref;
	struct expression *args_count[dimension];
	struct expression *actual_args;
	struct methodblock method = {
		.jit_code = code,
		.code_size = ARRAY_SIZE(code),
	};

	cu = alloc_simple_compilation_unit(&method);

	create_args(args_count, dimension);
	push_args(cu, args_count, dimension);

	convert_ir_const_single(cu, instance_class);

	arrayref = stack_pop(cu->expr_stack);

	assert_int_equals(EXPR_MULTIANEWARRAY, expr_type(arrayref));
	assert_int_equals(J_REFERENCE, arrayref->vm_type);
	assert_ptr_equals(instance_class, arrayref->multianewarray_ref_type);

	actual_args = to_expr(arrayref->multianewarray_dimensions);
	assert_args(args_count, dimension, actual_args);

	assert_true(stack_is_empty(cu->expr_stack));

	expr_put(arrayref);
	free_compilation_unit(cu);
	free(instance_class);
}

void test_convert_arraylength(void)
{
	unsigned char code[] = { OPC_ARRAYLENGTH };
	struct methodblock method = {
		.jit_code = code,
		.code_size = ARRAY_SIZE(code),
	};
	struct expression *arrayref, *arraylen_exp;
	struct compilation_unit *cu;
	struct object *class = new_class();

	cu = alloc_simple_compilation_unit(&method);

	arrayref = value_expr(J_REFERENCE, (unsigned long) class);
	stack_push(cu->expr_stack, arrayref);

	convert_to_ir(cu);

	arraylen_exp = stack_pop(cu->expr_stack);
	assert_int_equals(EXPR_ARRAYLENGTH, expr_type(arraylen_exp));
	assert_int_equals(J_REFERENCE, arraylen_exp->vm_type);
	assert_ptr_equals(arrayref, to_expr(arraylen_exp->arraylength_ref));

	expr_put(arraylen_exp);
	free_compilation_unit(cu);
	free(class);
}
