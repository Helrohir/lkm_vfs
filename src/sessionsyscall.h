/*
 ============================================================================
 Name        : sessionsyscall.h
 Author      : Eleonora Calore & Nicolò Rivetti
 Created on  : Oct 29, 2012
 Version     : 1.0
 Copyright   : Copyright (c) 2012  Eleonora Calore & Nicolò Rivetti
 Description : Declaration of the function needed by the module to setup and and restore
 	 the system calls
 ============================================================================
 */

#ifndef SESSIONSYSCALL_H_
#define	 SESSIONSYSCALL_H_

int registerSessionSyscall(int maxSession, int bufferOrder);
int unregisterSessionSyscall(void);

#endif /* SESSIONSYSCALL_H_ */
