#ifndef _ASM_TSAR_SPINLOCK_H
#define _ASM_TSAR_SPINLOCK_H

/* mostly taken from MIPS */

/*
 * Spinlocks API
 * - lock
 * - trylock
 * - unlock
 * - is_locked
 * - is_contented
 * - lock_flags
 * - unlock_wait
 */

static inline void arch_spin_lock(arch_spinlock_t *lock)
{
	int my_ticket;
	int tmp;
	int inc = 0x10000; /* used to add 1 to lock->ticket */

	__asm__ __volatile__ (
			".set push					\n"
			".set noreorder					\n"
			"1:	ll	%[tck], %[tck_ptr]		\n"
			"	addu	%[my_tck], %[tck], %[inc]	\n"
			"	sc	%[my_tck], %[tck_ptr]		\n"
			"	beqz	%[my_tck], 1b			\n"
			"	srl	%[my_tck], %[tck], 16		\n"
			"	andi	%[tck], %[tck], 0xffff		\n"
			"	bne	%[tck], %[my_tck], 4f		\n"
			"	subu	%[tck], %[my_tck], %[tck]	\n"
			"2:						\n"
			/* Note: the code below does not belong to the same
			 * execution flow as the code above. It means we exit
			 * at label 2 in the normal case, that is when ticket
			 * is not negative. Otherwise we execute the code below
			 */
			".subsection 2					\n"
			"4:	andi	%[tck], %[tck], 0x1fff		\n"
			"	sll	%[tck], 5			\n"
			"						\n"
			"6:	bnez	%[tck], 6b			\n"
			"	subu	%[tck], 1			\n"
			"						\n"
			"	lhu	%[tck], %[serving_now_ptr]	\n"
			"	beq	%[tck], %[my_tck], 2b		\n"
			"	subu	%[tck], %[my_tck], %[tck]	\n"
			"	b	4b				\n"
			"	subu	%[tck], %[tck], 1		\n"
			".previous					\n"
			".set pop					\n"
			: [tck_ptr] "+m" (lock->lock),
			[serving_now_ptr] "+m" (lock->h.serving_now),
			[tck] "=&r" (tmp),
			[my_tck] "=&r" (my_ticket)
			: [inc] "r" (inc));

	smp_mb__after_llsc();
}

static inline unsigned int arch_spin_trylock(arch_spinlock_t *lock)
{
	int tmp, tmp2, tmp3;
	int inc = 0x10000;

	__asm__ __volatile__ (
			".set push					\n"
			".set noreorder					\n"
			"1:	ll	%[tck], %[tck_ptr]		\n"
			"	srl	%[my_tck], %[tck], 16		\n"
			"	andi	%[now_serving], %[tck], 0xffff	\n"
			"	bne	%[my_tck], %[now_serving], 3f	\n"
			"	addu	%[tck], %[tck], %[inc]		\n"
			"	sc	%[tck], %[tck_ptr]		\n"
			"	beqz	%[tck], 1b			\n"
			"	li	%[tck], 1			\n"
			"2:						\n"
			".subsection 2					\n"
			"3:	b	2b				\n"
			"	li	%[tck], 0			\n"
			".previous					\n"
			".set pop					\n"
			: [tck_ptr] "+m" (lock->lock),
			[tck] "=&r" (tmp),
			[my_tck] "=&r" (tmp2),
			[now_serving] "=&r" (tmp3)
			: [inc] "r" (inc));

	smp_mb__after_llsc();

	return tmp;
}

static inline void arch_spin_unlock(arch_spinlock_t *lock)
{
	unsigned int serving_now = lock->h.serving_now + 1;
	wmb();
	lock->h.serving_now = (u16)serving_now;
	mb();
}

static inline int arch_spin_is_locked(arch_spinlock_t *lock)
{
	/* a spinlock is free when serving_now is equal to ticket. If a thread
	 * is current holding the spinlock, then it has incremented ticket. It
	 * will increment serving_now only when it is done with the lock */

	u32 tmp = ACCESS_ONCE(lock->lock);
	return (((arch_spinlock_t)tmp).h.ticket !=
			((arch_spinlock_t)tmp).h.serving_now);
}

static inline int arch_spin_is_contended(arch_spinlock_t *lock)
{
	u32 tmp = ACCESS_ONCE(lock->lock);
	return (((arch_spinlock_t)tmp).h.ticket -
			((arch_spinlock_t)tmp).h.serving_now) > 1;
}
#define arch_spin_is_contended arch_spin_is_contended

static inline void arch_spin_lock_flags(arch_spinlock_t *lock,
		unsigned long flags)
{
	arch_spin_lock(lock);
}

static inline void arch_spin_unlock_wait(arch_spinlock_t *lock)
{
	while (arch_spin_is_locked(lock))
		cpu_relax();
}


/*
 * Read-write spinlocks API (multiple readers, one writer)
 * - read_can_lock/write_can_lock
 * - read_lock/write_lock
 * - read_trylock/write_trylock
 * - read_unlock/write_unlock
 * - read_lock_flags/write_lock_flags
 */

static inline int arch_read_can_lock(arch_rwlock_t *rw)
{
	return (rw->lock >= 0);
}

static inline int arch_write_can_lock(arch_rwlock_t *rw)
{
	return (!rw->lock);
}

static inline void arch_read_lock(arch_rwlock_t *rw)
{
	unsigned int tmp;

	__asm__ __volatile__(
			"1:	ll	%[tmp], %[mem]	\n"
			"	bltz	%[tmp], 1b	\n"
			"	addu	%[tmp], 1	\n"
			"	sc	%[tmp], %[mem]	\n"
			"	beqz	%[tmp], 1b	\n"
			: [mem] "+m" (rw->lock),
			[tmp] "=&r" (tmp)
			:: "memory");

	smp_mb__after_llsc();
}

static inline void arch_write_lock(arch_rwlock_t *rw)
{
	unsigned int tmp;

	/* when locking for writing, put a negative value so nobody else can
	 * lock afterwards (including readers, hence the use of bltz above) */
	__asm__ __volatile__(
			"1:	ll	%[tmp], %[mem]	\n"
			"	bnez	%[tmp], 1b	\n"
			"	lui	%[tmp], 0x8000	\n"
			"	sc	%[tmp], %[mem]	\n"
			"	beqz	%[tmp], 1b	\n"
			: [mem] "+m" (rw->lock),
			[tmp] "=&r" (tmp)
			:: "memory");

	smp_mb__after_llsc();
}

static inline int arch_read_trylock(arch_rwlock_t *rw)
{
	unsigned int tmp;
	int ret = 0;

	__asm__ __volatile__(
			"1:	ll	%[tmp], %[mem]	\n"
			"	bltz	%[tmp], 2f	\n"
			"	addu	%[tmp], 1	\n"
			"	sc	%[tmp], %[mem]	\n"
			"	beqz	%[tmp], 1b	\n"
			"	li	%[ret], 1	\n"
			"2:				\n"
			: [mem] "+m" (rw->lock),
			[tmp] "=&r" (tmp), [ret] "=&r" (ret)
			:: "memory");

	smp_mb__after_llsc();

	return ret;
}

static inline int arch_write_trylock(arch_rwlock_t *rw)
{
	unsigned int tmp;
	int ret = 0;

	/* when locking for writing, put a negative value so nobody else can
	 * lock afterwards (including readers, hence the use of bltz above) */
	__asm__ __volatile__(
			"1:	ll	%[tmp], %[mem]	\n"
			"	bnez	%[tmp], 2f	\n"
			"	lui	%[tmp], 0x8000	\n"
			"	sc	%[tmp], %[mem]	\n"
			"	beqz	%[tmp], 1b	\n"
			"	li	%[ret], 1	\n"
			"2:				\n"
			: [mem] "+m" (rw->lock),
			[tmp] "=&r" (tmp), [ret] "=&r" (ret)
			:: "memory");

	smp_mb__after_llsc();

	return ret;
}

/* Note the use of sub, not subu which will make the kernel die with an
 * overflow exception if we ever try to unlock an rwlock that is already
 * unlocked or is being held by a writer. */
static inline void arch_read_unlock(arch_rwlock_t *rw)
{
	unsigned int tmp;

	smp_mb__before_llsc();

	__asm__ __volatile__(
			"1:	ll	%[tmp], %[mem]	\n"
			"	sub	%[tmp], 1	\n"
			"	sc	%[tmp], %[mem]	\n"
			"	beqz	%[tmp], 1b	\n"
			: [mem] "+m" (rw->lock),
			[tmp] "=&r" (tmp)
			:: "memory");
}

static inline void arch_write_unlock(arch_rwlock_t *rw)
{
	smp_mb();

	__asm__ __volatile__(
			"sw $0, %[mem]"
			: [mem] "+m" (rw->lock)
			:: "memory");
}

#define arch_read_lock_flags(lock, flags) arch_read_lock(lock)
#define arch_write_lock_flags(lock, flags) arch_write_lock(lock)

#define arch_spin_relax(lock)	cpu_relax()
#define arch_read_relax(lock)	cpu_relax()
#define arch_write_relax(lock)	cpu_relax()

#endif /* _ASM_TSAR_SPINLOCK_H */
