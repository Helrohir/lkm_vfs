/*
 ============================================================================
 Name        : syscallStealing.c
 Author      : Eleonora Calore & Nicolò Rivetti
 Created on  : Nov 7, 2012
 Version     : 1.0
 Copyright   : Copyright (c) 2012  Eleonora Calore & Nicolò Rivetti
 Description : Implementation of the functions needed for the sys call stealing
 ============================================================================
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/uaccess.h>

#define MAX_LEN 256

long getSystemCallTableAddr(unsigned long*** syscallTableAddr) {
	char buf[MAX_LEN];
	int i = 0;
	char *p;
	struct file *kallsymsFileStruct = NULL;
	mm_segment_t oldfs;
	char *sys_string;
	*syscallTableAddr = 0;

	// Switches the Segment Descriptors
	oldfs = get_fs();
	set_fs(KERNEL_DS);

	kallsymsFileStruct = filp_open("/proc/kallsyms", O_RDONLY, 0);

	if (IS_ERR(kallsymsFileStruct) || (kallsymsFileStruct == NULL )) {
		return -1;
	}

	memset(buf, 0x0, MAX_LEN);
	p = buf;

	// Read the kallsyms file byte per byte
	while (vfs_read(kallsymsFileStruct, p + i, 1, &kallsymsFileStruct->f_pos) == 1) {
		if (p[i] == '\n' || i == MAX_LEN - 1) {
			// If we reached a \n or the maximum lenght of the buffer, we check if we found the
			// sys_call_table symbol
			i = 0;
			// We use " sys_call_table" to avoid getting symbols with sys_call_table

			if ((strstr(p, " sys_call_table")) != NULL ) {
				sys_string = kmalloc(MAX_LEN, GFP_KERNEL);
				// No match found
				if (sys_string == NULL ) {
					filp_close(kallsymsFileStruct, 0);
					set_fs(oldfs);
					return -1;
				}

				// Bad formatted string
				if (p[8] != ' ') {
					filp_close(kallsymsFileStruct, 0);
					set_fs(oldfs);
					return -1;
				}

				// Take from the string the address of the sys_call_table symbol in hexadecimal notation
				// 32 / 4 = 8
				memset(sys_string, 0, MAX_LEN);
				strncpy(sys_string, p, 8);

				// Returns the value of a number contained in a string in hexadecimal notation
				*syscallTableAddr = (unsigned long **) simple_strtol(sys_string,
						NULL, 16);

				kfree(sys_string);
				set_fs(oldfs);
				filp_close(kallsymsFileStruct, 0);

				return 0;
			}

			memset(buf, 0x0, MAX_LEN);
			continue;
		}
		i++;
	}

	set_fs(oldfs);
	filp_close(kallsymsFileStruct, 0);

	return -1;
}

void disable_page_protection(void) {

	unsigned long value;
	// Move the value of CR0 into some register and then into the variable value
	asm volatile("mov %%cr0,%0" : "=r" (value));
	if (value & 0x00010000) {
		// If the Write Protect bit (16th bit) is set, reset it
		value &= ~0x00010000;
		// Then store the new value in some register and move its value into CR0
		asm volatile("mov %0,%%cr0": : "r" (value));
	}
}

void enable_page_protection(void) {

	unsigned long value;
	// Move the value of CR0 into some register and then into the variable value
	asm volatile("mov %%cr0,%0" : "=r" (value));
	if (!(value & 0x00010000)) {
		// If the Write Protect bit (16 bit) is not set, set it
		value |= 0x00010000;
		// Then store the new value in some register and move its value into CR0
		asm volatile("mov %0,%%cr0": : "r" (value));
	}
}
