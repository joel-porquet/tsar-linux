#ifndef _ASM_TSAR_SPINLOCK_TYPES_H
#define _ASM_TSAR_SPINLOCK_TYPES_H

#ifndef __LINUX_SPINLOCK_TYPES_H
#error only <linux/spinlock.h> can be included directly
#endif

/*
 * Definition of arch_spinlock_t.
 *
 * Spinlocks are defined as ticket locks. When a thread arrives, it atomically
 * increments "ticket". Then it atomically compares its ticket value (before
 * the increment) to the current "serving_now". If both values are equal, then
 * the thread gets the lock. Otherwise the thread must wait its turn. When the
 * thread holding the lock leaves, it increments serving_now.
 */

typedef union {
	/*
	 * bits 0..15:	serving_now
	 * bits 16..31:	ticket
	 */
	u32 lock;
	struct {
		u16 serving_now;
		u16 ticket;
	} h;
} arch_spinlock_t;

/* init value: spinlock is unlocked */
#define __ARCH_SPIN_LOCK_UNLOCKED { .lock = 0 }


/*
 * Definition of arch_rwlock_t
 */
typedef struct {
	volatile unsigned int lock;
} arch_rwlock_t;

#endif /* _ASM_TSAR_SPINLOCK_TYPES_H */
