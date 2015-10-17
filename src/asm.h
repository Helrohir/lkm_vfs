/*
 ============================================================================
 Name        : asm.h
 Author      : Eleonora Calore & Nicolò Rivetti
 Created on  : Oct 29, 2012
 Version     : 1.0
 Copyright   : Copyright (c) 2012  Eleonora Calore & Nicolò Rivetti
 Description : Contains the function to call all kind of syscalls using the "user level" asm
 ============================================================================
 */

#ifndef ASM_H_
#define ASM_H_

static inline long stub_syscall0(long syscall)
{
	long ret;

	__asm__ volatile ("int $0x80" : "=a" (ret) : "0" (syscall));

	return ret;
}

static inline long stub_syscall1(long syscall, long arg1)
{
	long ret;

	__asm__ volatile ("int $0x80" : "=a" (ret) : "0" (syscall), "b" (arg1));

	return ret;
}

static inline long stub_syscall2(long syscall, long arg1, long arg2)
{
	long ret;

	__asm__ volatile ("int $0x80" : "=a" (ret) : "0" (syscall), "b" (arg1),
			"c" (arg2));

	return ret;
}

static inline long stub_syscall3(long syscall, long arg1, long arg2, long arg3)
{
	long ret;

	__asm__ volatile ("int $0x80" : "=a" (ret) : "0" (syscall), "b" (arg1),
			"c" (arg2), "d" (arg3));

	return ret;
}

static inline long stub_syscall4(long syscall, long arg1, long arg2, long arg3,
				 long arg4)
{
	long ret;

	__asm__ volatile ("int $0x80" : "=a" (ret) : "0" (syscall), "b" (arg1),
			"c" (arg2), "d" (arg3), "S" (arg4));

	return ret;
}

static inline long stub_syscall5(long syscall, long arg1, long arg2, long arg3,
				 long arg4, long arg5)
{
	long ret;

	__asm__ volatile ("int $0x80" : "=a" (ret) : "0" (syscall), "b" (arg1),
			"c" (arg2), "d" (arg3), "S" (arg4), "D" (arg5));

	return ret;
}


#endif /* ASM_H_ */
