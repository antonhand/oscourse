// Fork a binary tree of processes and display their structure.

#include <inc/lib.h>

#define DEPTH 3

void forktree(const char *cur);

void
forkchild(const char *cur, char branch)
{
	char nxt[DEPTH+1];

	//envid_t e = sys_getenvid();

	if (strlen(cur) >= DEPTH)
		return;

	//cprintf("wr I am %x\n", e);
	snprintf(nxt, DEPTH+1, "%s%c", cur, branch);
	//cprintf("end wr I am %x\n", e);
	if (fork() == 0) {
		forktree(nxt);
		exit();
	}
	
}

void
forktree(const char *cur)
{
	cprintf("%04x: I am '%s'\n", sys_getenvid(), cur);

	forkchild(cur, '0');
	forkchild(cur, '1');
}

void
umain(int argc, char **argv)
{
	forktree("");
}

