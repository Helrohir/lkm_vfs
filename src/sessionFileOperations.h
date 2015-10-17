/*
 ============================================================================
 Name        : sessionFileOperations.h
 Author      : Eleonora Calore & Nicolò Rivetti
 Created on  : Oct 30, 2012
 Version     : 1.0
 Copyright   : Copyright (c) 2012  Eleonora Calore & Nicolò Rivetti
 Description : Declaration of the initialization function and the open function
 ============================================================================
 */

#ifndef SESSIONFILEOPERATIONS_H_
#define SESSIONFILEOPERATIONS_H_

int sessionInit(int maxSession, int bufferOrder);
int sessionOpen(struct file *filePtr, int flags);

#endif /* SESSIONFILEOPERATIONS_H_ */
