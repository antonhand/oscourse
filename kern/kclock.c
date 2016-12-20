/* See COPYRIGHT for copyright information. */

#include <inc/x86.h>
#include <kern/kclock.h>
#include <inc/time.h>

int read_time(void)
{
	uint8_t a_val;
	do {
		outb(IO_RTC_CMND, RTC_AREG);
		a_val = inb(IO_RTC_DATA);
	} while(a_val & RTC_UPDATE_IN_PROGRESS);

	struct tm tme;

	outb(IO_RTC_CMND, RTC_SEC);
	tme.tm_sec = BCD2BIN(inb(IO_RTC_DATA));

	outb(IO_RTC_CMND, RTC_MIN);
	tme.tm_min = BCD2BIN(inb(IO_RTC_DATA));

	outb(IO_RTC_CMND, RTC_HOUR);
	tme.tm_hour = BCD2BIN(inb(IO_RTC_DATA));

	outb(IO_RTC_CMND, RTC_DAY);
	tme.tm_mday = BCD2BIN(inb(IO_RTC_DATA));

	outb(IO_RTC_CMND, RTC_MON);
	tme.tm_mon = BCD2BIN(inb(IO_RTC_DATA)) - 1;

	outb(IO_RTC_CMND, RTC_YEAR);
	tme.tm_year = BCD2BIN(inb(IO_RTC_DATA));

	return timestamp(&tme);
}

int gettime(void)
{
	nmi_disable();
	// LAB 12: your code here

	int tme = read_time();
	int tme1 = read_time();

	if(tme != tme1){
		tme = read_time();
	}

	nmi_enable();
	return tme;
}

void
rtc_init(void)
{
	nmi_disable();
	// LAB 4: your code here
	outb(IO_RTC_CMND, RTC_BREG);
	uint8_t b_val = inb(IO_RTC_DATA);
	b_val |= RTC_PIE;
	outb(IO_RTC_DATA, b_val);
	/*outb(IO_RTC_CMND, RTC_AREG);
	uint8_t a_val = inb(IO_RTC_DATA);
	a_val |= 0xF;
	outb(IO_RTC_DATA, a_val);*/

	nmi_enable();
}

uint8_t
rtc_check_status(void)
{
	uint8_t status = 0;
	// LAB 4: your code here
	outb(IO_RTC_CMND, RTC_CREG);
	status = inb(IO_RTC_DATA);
	return status;
}

unsigned
mc146818_read(unsigned reg)
{
	outb(IO_RTC_CMND, reg);
	return inb(IO_RTC_DATA);
}

void
mc146818_write(unsigned reg, unsigned datum)
{
	outb(IO_RTC_CMND, reg);
	outb(IO_RTC_DATA, datum);
}
