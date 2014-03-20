/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2013 Pierre and Marie Curie University
 *  Joël Porquet <joel.porquet@lip6.fr>
 */

#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/sched.h>

#include <asm/uaccess.h>
#include <asm/mmuc2.h>
#include <asm/traps.h>

extern void die(const char *, struct pt_regs *);

void do_page_fault(struct pt_regs *regs, unsigned long address,
		unsigned int exec_acc, unsigned int write_acc)
{
	struct task_struct *tsk;
	struct mm_struct *mm;
	struct vm_area_struct *vma;
	siginfo_t info;
	int fault;
	unsigned int flags = FAULT_FLAG_ALLOW_RETRY | FAULT_FLAG_KILLABLE;

	tsk = current;

	/*
	 * We fault-in kernel-space virtual memory on-demand. The
	 * 'reference' page table is init_mm.pgd.
	 *
	 * NOTE! We MUST NOT take any locks for this case. We may
	 * be in an interrupt or a critical region, and should
	 * only copy the information from the master page table,
	 * nothing more.
	 *
	 * NOTE2: This is done so that, when updating the vmalloc
	 * mappings we don't have to walk all processes pgdirs and
	 * add the high mappings all at once. Instead we do it as they
	 * are used. However vmalloc'ed page entries have the PAGE_GLOBAL
	 * bit set so sometimes the TLB can use a lingering entry.
	 *
	 * This verifies that the fault happens in kernel space
	 * and that the fault was not a protection error.
	 */

	if (address >= VMALLOC_START && address <= VMALLOC_END &&
			!user_mode(regs))
		goto vmalloc_fault;

	/* If exceptions were enabled, we can reenable them here */
	if (user_mode(regs)) {
		/* Exception was in userspace: reenable interrupts */
		local_irq_enable();
		flags |= FAULT_FLAG_USER;
	} else {
		/* If exception was in a syscall, then IRQ's may have
		 * been enabled or disabled.  If they were enabled,
		 * reenable them.
		 */
		if (regs->cp0_status & ST0_IE)
			local_irq_enable();
	}

	mm = tsk->mm;
	info.si_code = SEGV_MAPERR;

	/*
	 * If we're in an interrupt or have no user
	 * context, we must not take the fault..
	 */

	if (in_interrupt() || !mm)
		goto no_context;

retry:
	down_read(&mm->mmap_sem);
	vma = find_vma(mm, address);

	if (!vma)
		goto bad_area;

	if (vma->vm_start <= address)
		goto good_area;

	if (!(vma->vm_flags & VM_GROWSDOWN))
		goto bad_area;

	if (user_mode(regs)) {
		/*
		 * accessing the stack below usp is always a bug.
		 * we get page-aligned addresses so we can only check
		 * if we're within a page from usp, but that might be
		 * enough to catch brutal errors at least.
		 */
		if (address + PAGE_SIZE < user_stack_pointer(regs))
			goto bad_area;
	}
	if (expand_stack(vma, address))
		goto bad_area;

	/*
	 * Ok, we have a good vm_area for this memory access, so
	 * we can handle it..
	 */

good_area:
	info.si_code = SEGV_ACCERR;

	/* first do some preliminary protection checks */

	if (write_acc) {
		if (!(vma->vm_flags & VM_WRITE))
			goto bad_area;
		flags |= FAULT_FLAG_WRITE;
	} else {
		/* not present */
		if (!(vma->vm_flags & (VM_READ | VM_EXEC)))
			goto bad_area;
	}

	/* are we trying to execute nonexecutable area */
	if (exec_acc && !(vma->vm_page_prot.pgprot & _PAGE_EXECUTABLE))
		goto bad_area;

	/*
	 * If for any reason at all we couldn't handle the fault,
	 * make sure we exit gracefully rather than endlessly redo
	 * the fault.
	 */

	fault = handle_mm_fault(mm, vma, address, flags);

	if ((fault & VM_FAULT_RETRY) && fatal_signal_pending(current))
		return;

	if (unlikely(fault & VM_FAULT_ERROR)) {
		if (fault & VM_FAULT_OOM)
			goto out_of_memory;
		else if (fault & VM_FAULT_SIGBUS)
			goto do_sigbus;
		BUG();
	}

	if (flags & FAULT_FLAG_ALLOW_RETRY) {
		/*RGD modeled on Cris */
		if (fault & VM_FAULT_MAJOR)
			tsk->maj_flt++;
		else
			tsk->min_flt++;
		if (fault & VM_FAULT_RETRY) {
			flags &= ~FAULT_FLAG_ALLOW_RETRY;
			flags |= FAULT_FLAG_TRIED;

			/* No need to up_read(&mm->mmap_sem) as we would
			 * have already released it in __lock_page_or_retry
			 * in mm/filemap.c.
			 */

			goto retry;
		}
	}

	up_read(&mm->mmap_sem);
	return;

	/*
	 * Something tried to access memory that isn't in our memory map..
	 * Fix it, but check if it's kernel or user first..
	 */

bad_area:
	up_read(&mm->mmap_sem);

bad_area_nosemaphore:

	/* User mode accesses just cause a SIGSEGV */

	if (user_mode(regs)) {
		info.si_signo = SIGSEGV;
		info.si_errno = 0;
		/* info.si_code has been set above */
		info.si_addr = (void *)address;
		force_sig_info(SIGSEGV, &info, tsk);
		return;
	}

no_context:

	/* Are we prepared to handle this kernel fault?
	 *
	 * (The kernel has valid exception-points in the source
	 *  when it acesses user-memory. When it fails in one
	 *  of those points, we find it in a table and do a jump
	 *  to some fixup code that loads an appropriate error
	 *  code)
	 */

	{
		const struct exception_table_entry *entry;

		if ((entry = search_exception_tables(instruction_pointer(regs))) != NULL) {
			/* Adjust the instruction pointer in the stackframe */
			instruction_pointer_set(regs, entry->fixup);
			return;
		}
	}

	/*
	 * Oops. The kernel tried to access some bad page. We'll have to
	 * terminate things with extreme prejudice.
	 */

	if ((unsigned long)(address) < PAGE_SIZE)
		printk(KERN_ALERT
				"Unable to handle kernel NULL pointer dereference");
	else
		printk(KERN_ALERT "Unable to handle kernel access");
	printk(" at virtual address 0x%08lx\n", address);

	die("Oops", regs);

	do_exit(SIGKILL);

	/*
	 * We ran out of memory, or some other thing happened to us that made
	 * us unable to handle the page fault gracefully.
	 */

out_of_memory:
	up_read(&mm->mmap_sem);
	if (!user_mode(regs))
		goto no_context;
	pagefault_out_of_memory();
	return;

do_sigbus:
	up_read(&mm->mmap_sem);

	/*
	 * Send a sigbus, regardless of whether we were in kernel
	 * or user mode.
	 */
	info.si_signo = SIGBUS;
	info.si_errno = 0;
	info.si_code = BUS_ADRERR;
	info.si_addr = (void *)address;
	force_sig_info(SIGBUS, &info, tsk);

	/* Kernel mode? Handle exceptions or die */
	if (!user_mode(regs))
		goto no_context;
	return;

vmalloc_fault:
	{
		/*
		 * Synchronize this task's top level page-table
		 * with the 'reference' page table.
		 *
		 * Use current_pgd instead of tsk->active_mm->pgd
		 * since the latter might be unavailable if this
		 * code is executed in a misfortunately run irq
		 * (like inside schedule() between switch_mm and
		 *  switch_to...).
		 */

		int offset = pgd_index(address);
		pgd_t *pgd, *pgd_k;
		pud_t *pud, *pud_k;
		pmd_t *pmd, *pmd_k;
		pte_t *pte_k;

		/*
		   phx_warn("do_page_fault(): vmalloc_fault will not work, "
		   "since current_pgd assign a proper value somewhere\n"
		   "anyhow we don't need this at the moment\n");

		   phx_mmu("vmalloc_fault");
		   */
		pgd = (pgd_t *)read_c2_ptpr() + offset;
		pgd_k = init_mm.pgd + offset;

		/* Since we're two-level, we don't need to do both
		 * set_pgd and set_pmd (they do the same thing). If
		 * we go three-level at some point, do the right thing
		 * with pgd_present and set_pgd here.
		 *
		 * Also, since the vmalloc area is global, we don't
		 * need to copy individual PTE's, it is enough to
		 * copy the pgd pointer into the pte page of the
		 * root task. If that is there, we'll find our pte if
		 * it exists.
		 */

		pud = pud_offset(pgd, address);
		pud_k = pud_offset(pgd_k, address);
		if (!pud_present(*pud_k))
			goto no_context;

		pmd = pmd_offset(pud, address);
		pmd_k = pmd_offset(pud_k, address);

		if (!pmd_present(*pmd_k))
			goto bad_area_nosemaphore;

		set_pmd(pmd, *pmd_k);

		/* Make sure the actual PTE exists as well to
		 * catch kernel vmalloc-area accesses to non-mapped
		 * addresses. If we don't do this, this will just
		 * silently loop forever.
		 */

		pte_k = pte_offset_kernel(pmd_k, address);
		if (!pte_present(*pte_k))
			goto no_context;

		return;
	}
}

