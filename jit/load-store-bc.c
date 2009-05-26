/*
 * Copyright (C) 2005-2006  Pekka Enberg
 *
 * This file is released under the GPL version 2. Please refer to the file
 * LICENSE for details.
 *
 * The file contains functions for converting Java bytecode load and store
 * instructions to immediate representation of the JIT compiler.
 */

#include <cafebabe/constant_pool.h>

#include <jit/bytecode-converters.h>
#include <jit/compiler.h>
#include <jit/statement.h>

#include <vm/bytecode.h>
#include <vm/bytecodes.h>
#include <vm/class.h>
#include <vm/classloader.h>
#include <vm/object.h>
#include <vm/resolve.h>
#include <vm/stack.h>

#include <errno.h>
#include <stdint.h>

static int __convert_const(struct parse_context *ctx,
			   unsigned long long value, enum vm_type vm_type)
{
	struct expression *expr = value_expr(vm_type, value);
	if (!expr)
		return -ENOMEM;

	convert_expression(ctx, expr);
	return 0;
}

int convert_aconst_null(struct parse_context *ctx)
{
	return __convert_const(ctx, 0, J_REFERENCE);
}

int convert_iconst(struct parse_context *ctx)
{
	return __convert_const(ctx, ctx->opc - OPC_ICONST_0, J_INT);
}

int convert_lconst(struct parse_context *ctx)
{
	return __convert_const(ctx, ctx->opc - OPC_LCONST_0, J_LONG);
}

static int __convert_fconst(struct parse_context *ctx, double value, enum vm_type vm_type)
{
	struct expression *expr = fvalue_expr(vm_type, value);
	if (!expr)
		return -ENOMEM;

	convert_expression(ctx, expr);
	return 0;
}

int convert_fconst(struct parse_context *ctx)
{
	return __convert_fconst(ctx, ctx->opc - OPC_FCONST_0, J_FLOAT);
}

int convert_dconst(struct parse_context *ctx)
{
	return __convert_fconst(ctx, ctx->opc - OPC_DCONST_0, J_DOUBLE);
}

int convert_bipush(struct parse_context *ctx)
{
	unsigned long long value;

	value = bytecode_read_s8(ctx->buffer);
	return __convert_const(ctx, value, J_INT);
}

int convert_sipush(struct parse_context *ctx)
{
	unsigned long long value;

	value = bytecode_read_s16(ctx->buffer);

	return __convert_const(ctx, value, J_INT);
}

static int utf8_char_count(const char *bytes, unsigned int n,
	unsigned int *res)
{
	unsigned int result = 0;

	for (unsigned int i = 0; i < n; ++i) {
		++result;

		/* 0xxxxxxx: 1 byte */
		if (!(bytes[i] & 0x80)) {
			continue;
		}

		/* 110xxxxx: 2 bytes */
		if ((bytes[i] & 0xe0) == 0xc0) {
			if (i + 1 >= n)
				return 1;
			if ((bytes[++i] & 0xc0) != 0x80)
				return 1;
			continue;
		}

		/* 1110xxxx: 3 bytes */
		if ((bytes[i] & 0xf0) == 0xe0) {
			if (i + 2 >= n)
				return 1;
			if ((bytes[++i] & 0xc0) != 0x80)
				return 1;
			if ((bytes[++i] & 0xc0) != 0x80)
				return 1;
			continue;
		}

		/* Anything else is an error */
		return 1;
	}

	*res = result;
	return 0;
}

static struct vm_object *utf8_to_char_array(const char *bytes, unsigned int n)
{
	unsigned int utf16_count;
	if (utf8_char_count(bytes, n, &utf16_count))
		return NULL;

	struct vm_object *array
		= vm_object_alloc_native_array(J_CHAR, utf16_count);
	if (!array) {
		NOT_IMPLEMENTED;
		return array;
	}

	uint16_t *utf16_chars = (uint16_t *) &array->fields;
	for (unsigned int i = 0, j = 0; i < n; ++i) {
		if (!(bytes[i] & 0x80)) {
			utf16_chars[j++] = bytes[i];
			continue;
		}

		if ((bytes[i] & 0xe0) == 0xc0) {
			uint16_t ch = (uint16_t) (bytes[i++] & 0x1f) << 6;
			ch += bytes[i++] & 0x3f;
			utf16_chars[j++] = ch;
			continue;
		}

		if ((bytes[i] & 0xf0) == 0xe0) {
			uint16_t ch = (uint16_t) (bytes[i++] & 0xf) << 12;
			ch += (uint16_t) (bytes[i++] & 0x3f) << 6;
			ch += bytes[i++] & 0x3f;
			utf16_chars[j++] = ch;
			continue;
		}
	}

	return array;
}

static int __convert_ldc(struct parse_context *ctx, unsigned long cp_idx)
{
	struct vm_class *vmc;
	struct cafebabe_constant_pool *cp;
	struct expression *expr = NULL;

	vmc = ctx->cu->method->class;

	if (cafebabe_class_constant_index_invalid(vmc->class, cp_idx))
		return -EINVAL;

	cp = &vmc->class->constant_pool[cp_idx];

	switch (cp->tag) {
	case CAFEBABE_CONSTANT_TAG_INTEGER:
		NOT_IMPLEMENTED;
		expr = value_expr(J_INT, cp->integer_.bytes);
		break;
	case CAFEBABE_CONSTANT_TAG_FLOAT:
		NOT_IMPLEMENTED;
		expr = fvalue_expr(J_FLOAT, cp->float_.bytes);
		break;
	case CAFEBABE_CONSTANT_TAG_STRING: {
		const struct cafebabe_constant_info_utf8 *utf8;

		if (cafebabe_class_constant_get_utf8(vmc->class,
			cp->string.string_index, &utf8))
		{
			NOT_IMPLEMENTED;
			break;
		}

		struct vm_object *array = utf8_to_char_array(utf8->bytes, utf8->length);
		if (!array) {
			NOT_IMPLEMENTED;
			break;
		}

		/* XXX: Yipes. We should be able to do better than this. */
		struct vm_class *string_class
			= classloader_load("java/lang/String");
		if (!string_class) {
			NOT_IMPLEMENTED;
			break;
		}

		struct vm_field *offset = vm_class_get_field(string_class,
			"offset", "I");
		if (!offset) {
			NOT_IMPLEMENTED;
			break;
		}

		struct vm_field *count = vm_class_get_field(string_class,
			"count", "I");
		if (!count) {
			NOT_IMPLEMENTED;
			break;
		}

		struct vm_field *value = vm_class_get_field(string_class,
			"value", "[C");
		if (!value) {
			NOT_IMPLEMENTED;
			break;
		}

		struct vm_object *string = vm_object_alloc(string_class);
		if (!string) {
			NOT_IMPLEMENTED;
			break;
		}

		*(int32_t *) &string->fields[offset->offset] = 0;
		*(int32_t *) &string->fields[count->offset]
			= array->array_length;
		*(void **) &string->fields[value->offset] = array;

		expr = value_expr(J_REFERENCE, (unsigned long) string);
		break;
	}
	case CAFEBABE_CONSTANT_TAG_LONG:
		//NOT_IMPLEMENTED;
		expr = value_expr(J_LONG,
			((uint64_t) cp->long_.high_bytes << 32)
			+ (uint64_t) cp->long_.low_bytes);
		break;
	case CAFEBABE_CONSTANT_TAG_DOUBLE:
		NOT_IMPLEMENTED;
		expr = fvalue_expr(J_DOUBLE,
			((uint64_t) cp->double_.high_bytes << 32)
			+ (uint64_t) cp->double_.low_bytes);
		break;
	default:
		return -EINVAL;
	}

	if (!expr)
		return -ENOMEM;

	convert_expression(ctx, expr);
	return 0;
}

int convert_ldc(struct parse_context *ctx)
{
	unsigned long idx;

	idx = bytecode_read_u8(ctx->buffer);

	return __convert_ldc(ctx, idx);
}

int convert_ldc_w(struct parse_context *ctx)
{
	unsigned long idx;

	idx = bytecode_read_u16(ctx->buffer);

	return __convert_ldc(ctx, idx);
}

int convert_ldc2_w(struct parse_context *ctx)
{
	unsigned long idx;

	idx = bytecode_read_u16(ctx->buffer);

	return __convert_ldc(ctx, idx);
}

static int convert_load(struct parse_context *ctx, unsigned char index, enum vm_type type)
{
	struct expression *expr;

	expr = local_expr(type, index);
	if (!expr)
		return -ENOMEM;

	convert_expression(ctx, expr);
	return 0;
}

int convert_iload(struct parse_context *ctx)
{
	unsigned char idx;

	idx = bytecode_read_u8(ctx->buffer);

	return convert_load(ctx, idx, J_INT);
}

int convert_lload(struct parse_context *ctx)
{
	unsigned char idx;

	idx = bytecode_read_u8(ctx->buffer);

	return convert_load(ctx, idx, J_LONG);
}

int convert_fload(struct parse_context *ctx)
{
	unsigned char idx;

	idx = bytecode_read_u8(ctx->buffer);

	return convert_load(ctx, idx, J_FLOAT);
}

int convert_dload(struct parse_context *ctx)
{
	unsigned char idx;

	idx = bytecode_read_u8(ctx->buffer);

	return convert_load(ctx, idx, J_DOUBLE);
}

int convert_aload(struct parse_context *ctx)
{
	unsigned char idx;

	idx = bytecode_read_u8(ctx->buffer);

	return convert_load(ctx, idx, J_REFERENCE);
}

int convert_iload_n(struct parse_context *ctx)
{
	return convert_load(ctx, ctx->opc - OPC_ILOAD_0, J_INT);
}

int convert_lload_n(struct parse_context *ctx)
{
	return convert_load(ctx, ctx->opc - OPC_LLOAD_0, J_LONG);
}

int convert_fload_n(struct parse_context *ctx)
{
	return convert_load(ctx, ctx->opc - OPC_FLOAD_0, J_FLOAT);
}

int convert_dload_n(struct parse_context *ctx)
{
	return convert_load(ctx, ctx->opc - OPC_DLOAD_0, J_DOUBLE);
}

int convert_aload_n(struct parse_context *ctx)
{
	return convert_load(ctx, ctx->opc - OPC_ALOAD_0, J_REFERENCE);
}

static int convert_store(struct parse_context *ctx, unsigned long index, enum vm_type type)
{
	struct expression *src_expr, *dest_expr;
	struct statement *stmt = alloc_statement(STMT_STORE);
	if (!stmt)
		goto failed;

	dest_expr = local_expr(type, index);
	src_expr = stack_pop(ctx->bb->mimic_stack);

	stmt->store_dest = &dest_expr->node;
	stmt->store_src = &src_expr->node;
	convert_statement(ctx, stmt);
	return 0;
      failed:
	free_statement(stmt);
	return -ENOMEM;
}

int convert_istore(struct parse_context *ctx)
{
	unsigned char idx;

	idx = bytecode_read_u8(ctx->buffer);

	return convert_store(ctx, idx, J_INT);
}

int convert_lstore(struct parse_context *ctx)
{
	unsigned char idx;

	idx = bytecode_read_u8(ctx->buffer);

	return convert_store(ctx, idx, J_LONG);
}

int convert_fstore(struct parse_context *ctx)
{
	unsigned char idx;

	idx = bytecode_read_u8(ctx->buffer);

	return convert_store(ctx, idx, J_FLOAT);
}

int convert_dstore(struct parse_context *ctx)
{
	unsigned char idx;

	idx = bytecode_read_u8(ctx->buffer);

	return convert_store(ctx, idx, J_DOUBLE);
}

int convert_astore(struct parse_context *ctx)
{
	unsigned char idx;

	idx = bytecode_read_u8(ctx->buffer);

	return convert_store(ctx, idx, J_REFERENCE);
}

int convert_istore_n(struct parse_context *ctx)
{
	return convert_store(ctx, ctx->opc - OPC_ISTORE_0, J_INT);
}

int convert_lstore_n(struct parse_context *ctx)
{
	return convert_store(ctx, ctx->opc - OPC_LSTORE_0, J_LONG);
}

int convert_fstore_n(struct parse_context *ctx)
{
	return convert_store(ctx, ctx->opc - OPC_FSTORE_0, J_FLOAT);
}

int convert_dstore_n(struct parse_context *ctx)
{
	return convert_store(ctx, ctx->opc - OPC_DSTORE_0, J_DOUBLE);
}

int convert_astore_n(struct parse_context *ctx)
{
	return convert_store(ctx, ctx->opc - OPC_ASTORE_0, J_REFERENCE);
}
