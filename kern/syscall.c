/* See COPYRIGHT for copyright information. */

#include <inc/x86.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/syscall.h>
#include <kern/console.h>
#include <kern/sched.h>
#include <kern/kclock.h>
#include <kern/tsc.h>

// Print a string to the system console.
// The string is exactly 'len' characters long.
// Destroys the environment on memory errors.
static void
sys_cputs(const char *s, size_t len)
{
	// Check that the user has permission to read memory [s, s+len).
	// Destroy the environment if not.

	// LAB 8: Your code here.
	user_mem_assert(curenv, (void *)s, len, PTE_U|PTE_P);

	// Print the string supplied by the user.
	cprintf("%.*s", len, s);
}

// Read a character from the system console without blocking.
// Returns the character, or 0 if there is no input waiting.
static int
sys_cgetc(void)
{
	return cons_getc();
}

// Returns the current environment's envid.
static envid_t
sys_getenvid(void)
{
	return curenv->env_id;
}

// Destroy a given environment (possibly the currently running environment).
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_destroy(envid_t envid)
{
	int r;
	struct Env *e;

	if ((r = envid2env(envid, &e, 1)) < 0)
		return r;
	/*if (e == curenv)
		cprintf("[%08x] exiting gracefully\n", curenv->env_id);
	else
		cprintf("[%08x] destroying %08x\n", curenv->env_id, e->env_id);*/
	env_destroy(e);
	return 0;
}

// Deschedule current environment and pick a different one to run.
static void
sys_yield(void)
{
	sched_yield();
}

// Allocate a new environment.
// Returns envid of new environment, or < 0 on error.  Errors are:
//	-E_NO_FREE_ENV if no free environment is available.
//	-E_NO_MEM on memory exhaustion.
static envid_t
sys_exofork(void)
{
	// Create the new environment with env_alloc(), from kern/env.c.
	// It should be left as env_alloc created it, except that
	// status is set to ENV_NOT_RUNNABLE, and the register set is copied
	// from the current environment -- but tweaked so sys_exofork
	// will appear to return 0.

	// LAB 9: Your code here.
	struct Env *e;
	int ret_alloc = env_alloc(&e, curenv->env_id);
	if(ret_alloc < 0) {
		return ret_alloc;
	}

	e->env_status = ENV_NOT_RUNNABLE;
	e->env_tf = curenv->env_tf;
	e->env_tf.tf_regs.reg_eax = 0;
	return e->env_id;
}

// Set envid's env_status to status, which must be ENV_RUNNABLE
// or ENV_NOT_RUNNABLE.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if status is not a valid status for an environment.
static int
sys_env_set_status(envid_t envid, int status)
{
	// Hint: Use the 'envid2env' function from kern/env.c to translate an
	// envid to a struct Env.
	// You should set envid2env's third argument to 1, which will
	// check whether the current environment has permission to set
	// envid's status.

	// LAB 9: Your code here.
	struct Env *e;
	int r = envid2env(envid, &e, 1);
	if(r < 0){
		return r;
	}

	if(status < 0 || status > 4) {
		return -E_INVAL;
	}

	e->env_status = status;
	return 0;
}

// Set envid's trap frame to 'tf'.
// tf is modified to make sure that user environments always run at code
// protection level 3 (CPL 3) with interrupts enabled.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_trapframe(envid_t envid, struct Trapframe *tf)
{
	// LAB 11: Your code here.
	// Remember to check whether the user has supplied us with a good
	// address!
	struct Env *env;

	if (envid2env(envid, &env, true) != 0)
		return -E_BAD_ENV;

	user_mem_assert(env, tf, sizeof(struct Trapframe), PTE_W);

	env->env_tf = *tf;
	env->env_tf.tf_eflags |= FL_IF;
	return 0;
}

// Set the page fault upcall for 'envid' by modifying the corresponding struct
// Env's 'env_pgfault_upcall' field.  When 'envid' causes a page fault, the
// kernel will push a fault record onto the exception stack, then branch to
// 'func'.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_pgfault_upcall(envid_t envid, void *func)
{
	// LAB 9: Your code here.
	struct Env *env;
	int r = envid2env(envid, &env, 1);
	if(r < 0){
		return r;
	}
	env->env_pgfault_upcall = func;
	return 0;
}

// Allocate a page of memory and map it at 'va' with permission
// 'perm' in the address space of 'envid'.
// The page's contents are set to 0.
// If a page is already mapped at 'va', that page is unmapped as a
// side effect.
//
// perm -- PTE_U | PTE_P must be set, PTE_AVAIL | PTE_W may or may not be set,
//         but no other bits may be set.  See PTE_SYSCALL in inc/mmu.h.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
//	-E_INVAL if perm is inappropriate (see above).
//	-E_NO_MEM if there's no memory to allocate the new page,
//		or to allocate any necessary page tables.
static int
sys_page_alloc(envid_t envid, void *va, int perm)
{
	// Hint: This function is a wrapper around page_alloc() and
	//   page_insert() from kern/pmap.c.
	//   Most of the new code you write should be to check the
	//   parameters for correctness.
	//   If page_insert() fails, remember to free the page you
	//   allocated!

	// LAB 9: Your code here.
	if((uint32_t)va > UTOP || (uint32_t)va % PGSIZE || !(perm & PTE_U) || !(perm & PTE_P) || (perm | PTE_SYSCALL) != PTE_SYSCALL){
		return -E_INVAL;
	}

	struct Env *env;
	int r = envid2env(envid, &env, 1);
	if(r < 0){
		return r;
	}

	struct PageInfo *new_page = page_alloc(ALLOC_ZERO); 
	if(!new_page){
		return -E_NO_MEM;
	}
	r = page_insert(env->env_pgdir,new_page, va, perm);
	if(r < 0){
		page_free(new_page);
		return -r;
	}

	return 0;
}

// Map the page of memory at 'srcva' in srcenvid's address space
// at 'dstva' in dstenvid's address space with permission 'perm'.
// Perm has the same restrictions as in sys_page_alloc, except
// that it also must not grant write access to a read-only
// page.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if srcenvid and/or dstenvid doesn't currently exist,
//		or the caller doesn't have permission to change one of them.
//	-E_INVAL if srcva >= UTOP or srcva is not page-aligned,
//		or dstva >= UTOP or dstva is not page-aligned.
//	-E_INVAL is srcva is not mapped in srcenvid's address space.
//	-E_INVAL if perm is inappropriate (see sys_page_alloc).
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in srcenvid's
//		address space.
//	-E_NO_MEM if there's no memory to allocate any necessary page tables.
static int
sys_page_map(envid_t srcenvid, void *srcva,
	     envid_t dstenvid, void *dstva, int perm)
{
	// Hint: This function is a wrapper around page_lookup() and
	//   page_insert() from kern/pmap.c.
	//   Again, most of the new code you write should be to check the
	//   parameters for correctness.
	//   Use the third argument to page_lookup() to
	//   check the current permissions on the page.

	// LAB 9: Your code here.
	struct Env *srcenv;
	struct Env *dstenv;
	pte_t *pte_store;

	if((uintptr_t) srcva >= UTOP || (uintptr_t) srcva % PGSIZE	|| (uintptr_t) dstva >= UTOP || (uintptr_t) dstva % PGSIZE) {
		return -E_INVAL;
	}

	int r = envid2env(srcenvid, &srcenv, 1);
	if(r < 0){
		return r;
	}

	r = envid2env(dstenvid, &dstenv, 1);
	if(r < 0){
		return r;
	}

	if(!(perm & PTE_U)|| !(perm & PTE_P) || (perm | PTE_SYSCALL) != PTE_SYSCALL){
		return -E_INVAL;
	}

	struct PageInfo *srcpage = page_lookup(srcenv->env_pgdir, srcva, &pte_store);
	if((perm & PTE_W) && !((*pte_store) & PTE_W)){
			return -E_INVAL;
	}

	if(!srcpage){
		return -E_INVAL;
	}

	r = page_insert(dstenv->env_pgdir, srcpage, dstva, perm);
	if(r < 0){
		return r;
	}

	return 0;
}

// Unmap the page of memory at 'va' in the address space of 'envid'.
// If no page is mapped, the function silently succeeds.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
static int
sys_page_unmap(envid_t envid, void *va)
{
	// Hint: This function is a wrapper around page_remove().

	// LAB 9: Your code here.
	if((uint32_t)va > UTOP || (uint32_t)va % PGSIZE){
		return -E_INVAL;
	}

	struct Env *env;

	int r = envid2env(envid,&env,1);
	if(r < 0){
		return r;
	}

	page_remove(env->env_pgdir, va);

	return 0;
}

// Try to send 'value' to the target env 'envid'.
// If srcva < UTOP, then also send page currently mapped at 'srcva',
// so that receiver gets a duplicate mapping of the same page.
//
// The send fails with a return value of -E_IPC_NOT_RECV if the
// target is not blocked, waiting for an IPC.
//
// The send also can fail for the other reasons listed below.
//
// Otherwise, the send succeeds, and the target's ipc fields are
// updated as follows:
//    env_ipc_recving is set to 0 to block future sends;
//    env_ipc_from is set to the sending envid;
//    env_ipc_value is set to the 'value' parameter;
//    env_ipc_perm is set to 'perm' if a page was transferred, 0 otherwise.
// The target environment is marked runnable again, returning 0
// from the paused sys_ipc_recv system call.  (Hint: does the
// sys_ipc_recv function ever actually return?)
//
// If the sender wants to send a page but the receiver isn't asking for one,
// then no page mapping is transferred, but no error occurs.
// The ipc only happens when no errors occur.
//
// Returns 0 on success, < 0 on error.
// Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist.
//		(No need to check permissions.)
//	-E_IPC_NOT_RECV if envid is not currently blocked in sys_ipc_recv,
//		or another environment managed to send first.
//	-E_INVAL if srcva < UTOP but srcva is not page-aligned.
//	-E_INVAL if srcva < UTOP and perm is inappropriate
//		(see sys_page_alloc).
//	-E_INVAL if srcva < UTOP but srcva is not mapped in the caller's
//		address space.
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in the
//		current environment's address space.
//	-E_NO_MEM if there's not enough memory to map srcva in envid's
//		address space.
static int
sys_ipc_try_send(envid_t envid, uint32_t value, void *srcva, unsigned perm)
{
	// LAB 9: Your code here.
	struct Env *env;
	int r = envid2env(envid, &env, 0);
	if(r < 0)
		return r;
	if(env->env_ipc_from)
		return -E_IPC_NOT_RECV;
	if(!env->env_ipc_recving){
		return -E_IPC_NOT_RECV;
	}

	if((uint32_t)srcva < UTOP){
		if((uint32_t)srcva % PGSIZE || !(perm & PTE_U) || !(perm & PTE_P) || (perm | PTE_SYSCALL) != PTE_SYSCALL){
			return -E_INVAL;
		}

		pte_t *pte;
		struct PageInfo *pg = page_lookup(curenv->env_pgdir, srcva, &pte);

		if(!pg){
			return -E_INVAL;
		}

		if(perm & PTE_W && !(*pte & PTE_W)) {
			return -E_INVAL;
		}

		if ((uint32_t)(env->env_ipc_dstva) < UTOP){
			r = page_insert(env->env_pgdir, pg, env->env_ipc_dstva, perm);
			
			if (r < 0)
				return r;
		}
	}

	env->env_ipc_recving = 0;
	env->env_ipc_from = curenv->env_id;
	env->env_ipc_value = value;

	if((uint32_t)(env->env_ipc_dstva) < UTOP) {
		env->env_ipc_perm = perm;	
	} else {
		env->env_ipc_perm = 0;
	}

	env->env_status = ENV_RUNNABLE;

	env->env_tf.tf_regs.reg_eax = 0;

	return 0;
}

// Block until a value is ready.  Record that you want to receive
// using the env_ipc_recving and env_ipc_dstva fields of struct Env,
// mark yourself not runnable, and then give up the CPU.
//
// If 'dstva' is < UTOP, then you are willing to receive a page of data.
// 'dstva' is the virtual address at which the sent page should be mapped.
//
// This function only returns on error, but the system call will eventually
// return 0 on success.
// Return < 0 on error.  Errors are:
//	-E_INVAL if dstva < UTOP but dstva is not page-aligned.
static int
sys_ipc_recv(void *dstva)
{
	// LAB 9: Your code here.
	if((uint32_t)dstva < UTOP && ((uint32_t)dstva) % PGSIZE){
			return -E_INVAL;
	}

	curenv->env_ipc_recving = 1;
	curenv->env_ipc_dstva = dstva;
	curenv->env_ipc_from = 0;

	curenv->env_status = ENV_NOT_RUNNABLE;

	sched_yield();
	return 0;
}

// Return date and time in UNIX timestamp format: seconds passed
// from 1970-01-01 00:00:00 UTC.
static int
sys_gettime(void)
{
	// LAB 12: Your code here.
	return gettime();
}

static int
sys_clock_getres(int clock_id, struct timespec *res)
{
	user_mem_assert(curenv, (void *)res, sizeof(struct timespec), PTE_U|PTE_P);
	if(clock_id < 0 || clock_id >= CLOCK_NUM){
		return -E_INVAL;
	}
	clock_getres(clock_id, res);
	return 0;
}

static int
sys_clock_gettime(int clock_id, struct timespec *tp)
{
	user_mem_assert(curenv, (void *)tp, sizeof(struct timespec), PTE_U|PTE_P);
	if(clock_id < 0 || clock_id >= CLOCK_NUM){
		return -E_INVAL;
	}

	clock_gettime(clock_id, tp);
	return 0;
}

static int
sys_clock_settime(int clock_id, const struct timespec *tp)
{
	user_mem_assert(curenv, (void *)tp, sizeof(struct timespec), PTE_U|PTE_P|PTE_W);
	if(clock_id == CLOCK_MONOTONIC){
		return -E_INVAL;
	}

	if(clock_id < 0 || clock_id >= CLOCK_NUM){
		return -E_INVAL;
	}

	if(tp->tv_sec < 0 || tp->tv_nsec < 0 || tp->tv_nsec > 999999999){
		return -E_INVAL;
	}

	clock_settime(clock_id, tp);

	return 0;
}

// В случае передачи в clock_id CLOCK_PROCESS_CPUTIME_ID вернёт ошибку.
//
// При установленном флаге TIMER_ABSTIME функция установит curenv->env_sleep_clockid
// идентификатор соответствующих часов, в curenv->env_wakeup_time время, заданное в rqtp, 
// установит  curenv->env_status в ENV_SLEEPING и вызовет sched_yield()
//
// Если флаг TIMER_ABSTIME не установлен, то будет выполнены действия, аналогичные предыдущему случаю,
// только в curenv->env_sleep_clockid будет установлено CLOCK_MONOTONIC,
// а в curenv->env_wakeup_time сумму текущего времени по CLOCK_MONOTONIC и значения, заданного в rqtp.
static int
sys_clock_nanosleep(int clock_id, int flags, const struct timespec *rqtp, struct timespec *rmtp)
{
	if(rmtp){
		user_mem_assert(curenv, (void *)rmtp, sizeof(struct timespec), PTE_U|PTE_P);
	}
    if(clock_id == CLOCK_PROCESS_CPUTIME_ID){
		return -E_INVAL;
	}

	if(clock_id < 0 || clock_id >= CLOCK_NUM){
		return -E_INVAL;
	}

	if(rqtp->tv_sec < 0 || rqtp->tv_nsec < 0 || rqtp->tv_nsec > 999999999){
		return -E_INVAL;
	}

    struct timespec tp = *rqtp;

    if(flags != TIMER_ABSTIME){
        clock_id = CLOCK_MONOTONIC;
        clock_gettime(CLOCK_MONOTONIC, &tp);
        tp = add_timespec(rqtp, &tp);
    }

    curenv->env_sleep_clockid = clock_id;
    curenv->env_wakeup_time = tp;
    curenv->env_status = ENV_SLEEPING;

	curenv->env_tf.tf_regs.reg_eax = 0;
	if(rmtp){
		rmtp->tv_sec = 0;
		rmtp->tv_nsec = 0;
	}
    sched_yield();

    return 0;
}

// Dispatches to the correct kernel function, passing the arguments.
int32_t
syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
	// Call the function corresponding to the 'syscallno' parameter.
	// Return any appropriate return value.
	// LAB 8: Your code here.

	int32_t res = 0;
    switch(syscallno)
    {
        case SYS_cputs:
            sys_cputs((const char*)a1,(size_t)a2);
            res = 0;
            break;
        case SYS_cgetc:
            res = sys_cgetc();
            break;
        case SYS_getenvid:
            res = sys_getenvid();
            break;
        case SYS_env_destroy:
            res = sys_env_destroy(a1);
            break;
        case SYS_yield:
            res = 0;
            sys_yield();
            break;
        case SYS_exofork:
            res = sys_exofork();
            break;
        case SYS_env_set_status:
            res = sys_env_set_status(a1,a2);
            break;
        case SYS_page_alloc:
            res = sys_page_alloc(a1,(void*)a2,a3);
            break;
        case SYS_page_map:
            res = sys_page_map(a1,(void*)a2,a3,(void*)a4,a5);
            break;
        case SYS_page_unmap:
            res = sys_page_unmap(a1,(void*)a2);
            break;
		case SYS_env_set_trapframe:
			res = sys_env_set_trapframe(a1,(void*)a2);
            break;
        case SYS_env_set_pgfault_upcall:
            res = sys_env_set_pgfault_upcall(a1,(void*)a2);
            break;
        case SYS_ipc_try_send:
            res = sys_ipc_try_send(a1,a2,(void*)a3,a4);
            break;
        case SYS_ipc_recv:
            res = sys_ipc_recv((void*)a1);
            break;
		case SYS_gettime:
			res = sys_gettime();
            break;
		case SYS_clock_getres:
			res = sys_clock_getres(a1,(void*)a2);
			break;
		case SYS_clock_gettime:
			res = sys_clock_gettime(a1,(void*)a2);
			break;
		case SYS_clock_settime:
			res = sys_clock_settime(a1,(void*)a2);
			break;
		case SYS_clock_nanosleep:
			sys_clock_nanosleep(a1, a2, (void*)a3, (void*)a4);
        case NSYSCALLS:
        default:
            res = -E_INVAL;
    }

	return res;
}

