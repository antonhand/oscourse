// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>

#include <kern/tsc.h>
#include <kern/pmap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "text", "Display any text", mon_text },
	{ "backtrace", "Display backtrace", mon_backtrace },
	{ "start", "Start tsc", tsc_start },
	{ "stop", "Stop tsc", tsc_stop },
	{ "pages", "Stop tsc", mon_pages},
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

/***** Implementations of basic kernel monitor commands *****/

int
mon_pages(int argc, char **argv, struct Trapframe *tf)
{
	size_t status = is_page_free(&pages[1]);
	size_t beg = 1;
	size_t en = 0;
	size_t i;
	for(i = 1; i < npages; i++){
		if(is_page_free(&pages[i]) == status){
			en = i;
		} else {
			if(beg != en){
				cprintf("%d..%d ", beg, en);
			} else {
				cprintf("%d ", beg);
			}
			if(!status){
				cprintf("ALLOCATED\n");
			} else {
				cprintf("FREE\n");
			}
			beg = i;
			status = is_page_free(&pages[i]);
			en = i;
		}
	}

	if(beg != en){
		cprintf("%d..%d ", beg, en);
	} else {
		cprintf("%d ", beg);
	}
	if(!status){
		cprintf("ALLOCATED\n");
	} else {
		cprintf("FREE\n");
	}
	return 0;
}

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", (uint32_t)_start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n",
            (uint32_t)entry, (uint32_t)entry - KERNTOP);
	cprintf("  etext  %08x (virt)  %08x (phys)\n",
            (uint32_t)etext, (uint32_t)etext - KERNTOP);
	cprintf("  edata  %08x (virt)  %08x (phys)\n",
            (uint32_t)edata, (uint32_t)edata - KERNTOP);
	cprintf("  end    %08x (virt)  %08x (phys)\n",
            (uint32_t)end, (uint32_t)end - KERNTOP);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_text(int argc, char **argv, struct Trapframe *tf)
{
	cprintf("Any text\n");
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	cprintf("Stack backtrace:\n");
	int *ebp = (int *)read_ebp();
	while(ebp != NULL){
		int eip = *(ebp + 1);
		cprintf("ebp %08x eip %08x args %08x %08x %08x %08x %08x\n", (unsigned int)ebp, eip, *(ebp + 2), *(ebp + 3), *(ebp + 4), *(ebp + 5), *(ebp + 6));
		struct Eipdebuginfo info;
		debuginfo_eip(eip, &info);
		cprintf("%s:%d: %.*s+%d\n", info.eip_file, info.eip_line, info.eip_fn_namelen, info.eip_fn_name, eip - info.eip_fn_addr);
		ebp = (int*)*ebp;
	}
	return 0;
}

int
tsc_start(int argc, char **argv, struct Trapframe *tf)
{
	timer_start();
	return 0;
}

int
tsc_stop(int argc, char **argv, struct Trapframe *tf)
{
	timer_stop();
	return 0;
}

/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < NCOMMANDS; i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");

	if (tf != NULL)
		print_trapframe(tf);
		
	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
