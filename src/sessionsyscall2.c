/*
 ============================================================================
 Name        : sessionSyscall2.c
 Author      : Eleonora Calore & Nicolò Rivetti
 Created on  : Oct 29, 2012
 Version     : 1.0
 Copyright   : Copyright (c) 2012  Eleonora Calore & Nicolò Rivetti
 Description : Implementation of the Session Open Syscall that will replace
	the original open, and the function that perform the switches on the
	system call table. Do not required that the system call table is writable
	or that the symbol is exported, only that it can be looked up in
	/proc/kallsyms
 ============================================================================
 */
#include <linux/kernel.h>
#include <linux/linkage.h>
#include <linux/file.h>

#include <linux/module.h>

#include <linux/fs.h>
#include <linux/types.h>

#include "Defines.h"
#include "sessionFileOperations.h"
#include "syscallStealing.h"

#define __NR_sys_open 5

static long *previousSysCall_sys_open = 0x0;
unsigned long **sys_call_table_stealed;

// Prototype of the original open syscall
asmlinkage int (*original_open) (const char *, int, mode_t);
extern asmlinkage long sys_close(unsigned int fd);

/*
 * Session Open Syscall implementation, call the original open and then calls the sessionOpen
 * function which will create a new session if required
 */
asmlinkage int sys_sessionOpen(const char *pathname, int flags, mode_t mode) {

	int fd;
	int ret;
	struct file *filePtr;

	try_module_get(THIS_MODULE);

	// Calling the original Open Syscall
	fd = original_open(pathname, flags, mode);

	if (fd < 0) {
		module_put(THIS_MODULE);
		return fd;
	}

	// If the open has not raised any error, we retrieve the new struct file
	filePtr = fget(fd);
	// Reducing the f_count field
	atomic_long_dec(&filePtr->f_count);
	ret = sessionOpen(filePtr, flags);
	if(ret < 0){
		// If a failure occurred, close the file
		sys_close(fd);
		module_put(THIS_MODULE);
		return ret;
	}

	module_put(THIS_MODULE);
	return fd;
}

/*
 * Initialize the session module and switches the syscall places on the system call table
 */
int registerSessionSyscall(int maxSession, int bufferOrder) {

	if(getSystemCallTableAddr(&sys_call_table_stealed) < 0){
		printk(KERN_ERR "Cannot retrieve sys call table address");
		return -1;
	}

	sessionInit(maxSession, bufferOrder);

	// Store the original open into our function pointer, allowing efficient call
	original_open = (asmlinkage int (*) (const char *, int, mode_t)) sys_call_table_stealed[__NR_sys_open];

	// Disable the read only protection
	disable_page_protection();

	previousSysCall_sys_open = sys_call_table_stealed[__NR_sys_open];
	sys_call_table_stealed[__NR_sys_open] = (unsigned long *) sys_sessionOpen;

	// Re enable the read only protection
	enable_page_protection();

	return 0;
}

/*
 * Restore the previous state of the system call table
 */
int unregisterSessionSyscall(void) {

	// Disable the read only protection
	disable_page_protection();

	sys_call_table_stealed[__NR_sys_open] = previousSysCall_sys_open;

	// Re enable the read only protection
	enable_page_protection();
	return 0;
}

