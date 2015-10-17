/*
 ============================================================================
 Name        : sessionSyscall.c
 Author      : Eleonora Calore & Nicolò Rivetti
 Created on  : Oct 29, 2012
 Version     : 1.0
 Copyright   : Copyright (c) 2012  Eleonora Calore & Nicolò Rivetti
 Description : Implementation of the Session Open Syscall that will replace
	the original open, and the function that perform the switches on the
	system call table. Requires that the system call table is writable and its
	symbol is exported.
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
#include "asm.h"

#define __NR_sys_open_placeHolder 31
#define __NR_sys_open 5

static long previousSysCall_sys_open = 0x0;
static long previous_placeHolder = 0x0;
extern void *sys_call_table[];

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
	fd = stub_syscall3(__NR_sys_open_placeHolder, (long) pathname, flags, mode);

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

	sessionInit(maxSession, bufferOrder);

	previousSysCall_sys_open = (long) sys_call_table[__NR_sys_open];
	sys_call_table[__NR_sys_open] = sys_sessionOpen;
	previous_placeHolder = (long)  sys_call_table[__NR_sys_open_placeHolder];
	sys_call_table[__NR_sys_open_placeHolder] = (void*) previousSysCall_sys_open;
	return 0;
}

/*
 * Restore the previous state of the system call table
 */
int unregisterSessionSyscall(void) {
	sys_call_table[__NR_sys_open] = (void*) previousSysCall_sys_open;
	sys_call_table[__NR_sys_open_placeHolder] = (void*) previous_placeHolder;
	return 0;
}

