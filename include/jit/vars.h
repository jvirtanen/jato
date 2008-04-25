#ifndef __JIT_VARS_H
#define __JIT_VARS_H

#include <vm/list.h>
#include <arch/registers.h>
#include <stdbool.h>

struct live_range {
	unsigned long start, end;	/* end is exclusive */
};

static inline bool in_range(struct live_range *range, unsigned long offset)
{
	return (offset >= range->start) && (offset < range->end);
}

static inline unsigned long range_len(struct live_range *range)
{
	return range->end - range->start;
}

static inline bool range_is_empty(struct live_range *range)
{
	return range->start == ~0UL && range->end == 0;
}

struct var_info;
struct insn;

struct live_interval {
	/* Parent variable of this interval.  */
	struct var_info *var_info;

	/* Live range of this interval.  */
	struct live_range range;

	/* Machine register of this interval.  */
	enum machine_reg reg;

	/* List of use positions of this interval.  */
	struct list_head registers;

	/* Member of list of intervals during linear scan.  */
	struct list_head interval;

	/* Member of list of active intervals during linear scan.  */
	struct list_head active;

	/* Array of instructions within this interval.  */
	struct insn **insn_array;

	/* Do we need to spill the register of this interval?  */
	bool need_spill;

	/* Do we need to reload the register of this interval?  */
	bool need_reload;

	/* The slot this interval is spilled to. Only set if ->need_spill is
	   true.  */
	struct stack_slot *spill_slot;
};

struct var_info {
	unsigned long		vreg;
	struct var_info		*next;
	struct live_interval	*interval;
};

struct insn;

struct register_info {
	struct insn		*insn;
	struct live_interval	*interval;
	struct list_head	reg_list;
};

static inline void __assoc_var_to_operand(struct var_info *var,
					  struct insn *insn,
					  struct register_info *reg_info)
{
	reg_info->insn = insn;
	reg_info->interval = var->interval;
	INIT_LIST_HEAD(&reg_info->reg_list);
	list_add(&reg_info->reg_list, &var->interval->registers);
}

static inline enum machine_reg mach_reg(struct register_info *reg)
{
	return reg->interval->reg;
}

static inline struct var_info *mach_reg_var(struct register_info *reg)
{
	return reg->interval->var_info;
}

static inline bool is_vreg(struct register_info *reg, unsigned long vreg)
{
	return reg->interval->var_info->vreg == vreg;
}

struct live_interval *alloc_interval(struct var_info *);
void free_interval(struct live_interval *);
struct live_interval *split_interval_at(struct live_interval *, unsigned long pos);

#endif /* __JIT_VARS_H */
