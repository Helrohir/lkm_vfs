/*
 ============================================================================
 Name        : syscallStealing.h
 Author      : Eleonora Calore & Nicolò Rivetti
 Created on  : Nov 7, 2012
 Version     : 1.0
 Copyright   : Copyright (c) 2012  Eleonora Calore & Nicolò Rivetti
 Description : Declaration of the functions needed for the system call stealing
 ============================================================================
 */

#ifndef SYSCALLSTEALING_H_
#define SYSCALLSTEALING_H_

void disable_page_protection(void);
void enable_page_protection(void);
int getSystemCallTableAddr(unsigned long*** syscallTableAddr);

#endif /* SYSCALLSTEALING_H_ */
