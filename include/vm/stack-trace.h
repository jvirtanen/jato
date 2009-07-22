#ifndef JATO_VM_STACK_TRACE_H
#define JATO_VM_STACK_TRACE_H

#include "arch/stack-frame.h"

#include "vm/stack-trace.h"
#include "vm/natives.h"
#include "vm/vm.h"

#include <stdbool.h>

struct compilation_unit;
struct vm_method;
struct vm_class;

struct jni_stack_entry {
	void *caller_frame;
	unsigned long call_site_addr;

	/* We don't know the address of JNI callee at compilation time
	 * so code generated for JNI call site stores a pointer to
	 * vm_method from which we obtain target pointer at the time
	 * when stack is traversed. */
	struct vm_method *method;

	/* This field is filled in on entry to any JNI interface
	 * function. Having this field filled in allows the
	 * stackwalker to skip whole JNI call. When JNI interface
	 * fuction returns to JNI caller nothing is done because this
	 * structure can be accessed only when we're in VM which can
	 * happen only after some JNI interface function was
	 * called. */
	void *jni_interface_frame;
} __attribute__((packed));

struct vm_native_stack_entry {
	void *stack_ptr;
	void *target;
} __attribute__((packed));

#define VM_NATIVE_STACK_SIZE 256
#define JNI_STACK_SIZE 1024

extern void *vm_native_stack_offset_guard;
extern void *vm_native_stack_badoffset;
extern void *jni_stack_offset_guard;
extern void *jni_stack_badoffset;

extern __thread struct jni_stack_entry jni_stack[JNI_STACK_SIZE];
extern __thread unsigned long jni_stack_offset;
extern __thread struct vm_native_stack_entry vm_native_stack[VM_NATIVE_STACK_SIZE];
extern __thread unsigned long vm_native_stack_offset;

int vm_enter_jni(void *caller_frame, unsigned long call_site_addr,
		 struct vm_method *method);
int vm_enter_vm_native(void *target, void *stack_ptr);
void vm_leave_jni(void);
void vm_leave_vm_native(void);

static inline int jni_stack_index(void)
{
	return jni_stack_offset / sizeof(struct jni_stack_entry);
}

static inline int vm_native_stack_index(void)
{
	return vm_native_stack_offset /
		sizeof(struct vm_native_stack_entry);
}

static inline void *vm_native_stack_get_frame(void)
{
	if (vm_native_stack_offset == 0)
		return NULL;

	return vm_native_stack[vm_native_stack_index() - 1].stack_ptr -
		sizeof(struct native_stack_frame);
}

/*
 * This is defined as a macro because we must assure that
 * __builtin_frame_address() returns the macro user's frame. Compiler
 * optimizations might optimize some function calls so that the target
 * function runs in the caller's frame. We want to avoid this situation.
 */
#define vm_enter_jni_interface() {					\
	jni_stack[jni_stack_index() - 1].jni_interface_frame = \
			__builtin_frame_address(0);			\
	}


/*
 * Points to a native stack frame that is considered as bottom-most
 * for given thread.
 */
extern __thread struct native_stack_frame *bottom_stack_frame;

enum stack_trace_elem_type {
	STACK_TRACE_ELEM_TYPE_JIT,
	STACK_TRACE_ELEM_TYPE_JNI,
	STACK_TRACE_ELEM_TYPE_VM_NATIVE,

	STACK_TRACE_ELEM_TYPE_OTHER,
	STACK_TRACE_ELEM_TYPE_TRAMPOLINE,
};

struct stack_trace_elem {
	/* Holds instruction address of this stack trace element. */
	unsigned long addr;

	enum stack_trace_elem_type type;

	int vm_native_stack_index;
	int jni_stack_index;

	/*
	 * If true then @frame has format of struct native_stack_frame
	 * and struct jit_stack_frame otherwise.
	 */
	bool is_native;

	/*
	 * Points to a stack frame of this stack trace element. Note
	 * that for type == STACK_TRACE_ELEM_TYPE_JNI value of @frame
	 * is undefined.
	 */
	void *frame;
};

void init_stack_trace_printing(void);
int init_stack_trace_elem(struct stack_trace_elem *elem);
int get_prev_stack_trace_elem(struct stack_trace_elem *elem);
int skip_frames_from_class(struct stack_trace_elem *elem, struct vm_class *class);
int get_stack_trace_depth(struct stack_trace_elem *elem);
struct vm_object *get_stack_trace(void);
struct vm_object *get_stack_trace_from_ctx(void *ctx);
struct vm_object * __vm_native native_vmthrowable_fill_in_stack_trace(struct vm_object *);
struct vm_object * __vm_native native_vmthrowable_get_stack_trace(struct vm_object *, struct vm_object *);

bool called_from_jit_trampoline(struct native_stack_frame *frame);
void vm_print_exception(struct vm_object *exception);
struct vm_object *vm_alloc_stack_overflow_error(void);

#endif /* JATO_VM_STACK_TRACE_H */
