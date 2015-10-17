/*
 ============================================================================
 Name        : module.c
 Author      : Eleonora Calore & Nicolò Rivetti
 Created on  : Oct 29, 2012
 Version     : 1.0
 Copyright   : Copyright (c) 2012  Eleonora Calore & Nicolò Rivetti
 Description : Module Init and Cleanup functions
 ============================================================================
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include "sessionsyscall.h"

MODULE_LICENSE("GPL");

static int maxSession = -1;
static int bufferOrder = -1;

module_param(maxSession, int, S_IRUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(myint, "Max sessions");
module_param(bufferOrder, int, S_IRUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(myint, "Order of the maximum size of the session buffer");

static int __init init_sessionSyscall(void) {
	int ret = -1;
	printk(KERN_INFO "Installing Session Module\n");

	ret = registerSessionSyscall(maxSession, bufferOrder);
	if (ret < 0) {
		return ret;
	}

	return 0;
}

static void __exit cleanup_sessionSyscall(void) {
	printk(KERN_INFO "Removing Session Module\n");
	unregisterSessionSyscall();
}

module_init(init_sessionSyscall);
module_exit(cleanup_sessionSyscall);
