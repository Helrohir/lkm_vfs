/*
 ============================================================================
 Name        : sessionFileOperations.c
 Author      : Eleonora Calore & Nicolò Rivetti
 Created on  : Oct 30, 2012
 Version     : 1.0
 Copyright   : Copyright (c) 2012  Eleonora Calore & Nicolò Rivetti
 Description : Implementations of the Session Open and the Session File Operations
 ============================================================================
 */

#include <linux/fs.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <linux/file.h>
#include <linux/gfp.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/rwsem.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/sched.h>

#include "Defines.h"
#include "sessionFileOperations.h"
#include "workaround.h"

#define DEFAULT_SESSIONNUM 512 // Default maximum session num
#define MAX_SESSIONNUM 2048 // session num cap
#define DEFAULT_PAGENUM 4 // Default number of pages allocated per session buffer
#define DEFAULT_ORDER 2 // Default order of pages allocated per session buffer
#define MAX_PAGENUM 16 // Maximum number of pages allocated per session buffer
#define MAX_BUFFERORDER 4 // Maximum order of pages allocated per session buffer

#define KAMLLOCFLAGS GFP_KERNEL | __GFP_ZERO // kmalloc flags

#define BADSTATEFLAG 0x10000000

static int maxSessionNum = DEFAULT_SESSIONNUM; // Current maximum session num
static int maxBufferSize = 4096 * DEFAULT_PAGENUM; // Current session buffer size
static int maxBufferOrder = DEFAULT_ORDER; // Current session buffer order

struct sessionData_struct {
	atomic_t usageCountAndFlag; // Usage Counter and Flag to mark a bad state struct
	char* buffer; // Pointer to the sesssion buffer
	struct rw_semaphore fileInBufferLock; // Lock that protects the size of the stored file
	unsigned long fileInBufferSize; // Size of the stored size
	struct mutex writeLock; // Lock against concurrent writes
	void* private_data; // Pointer to the previous private_data
	const struct file_operations * oldFops; // Pointer to the previous fops
};

typedef struct sessionData_struct sessionData;

#define getSessionData(FilePtr)  ((sessionData*) FilePtr->private_data)

//TODO Ridefinito loff_t per aggirare il problema della define(__GNUC__) in types.h
typedef long long loff_t;

// Current active session counter
static atomic_t sessionCount = ATOMIC_INIT(0);

// New Session File Operations propotypes
ssize_t sessionRead(struct file * filePtsr, char __user * buff, size_t count,
		loff_t * pos);
ssize_t sessionWrite(struct file * filePtr, const char __user * buff,
		size_t count, loff_t * pos);
loff_t sessionLlseek(struct file *filePtr, loff_t offset, int origin);
int sessionFlush(struct file * filePtr, fl_owner_t id);

// New Session File Operations Struct
const struct file_operations session_fops = { owner : THIS_MODULE, read:sessionRead, write
		: sessionWrite, llseek: sessionLlseek, flush: sessionFlush, };

extern int kernel_read(struct file * filePtr, loff_t offset, char* addr,
unsigned long count);

/*
 * if maxSession is less than 0, then the maximum number of sessions is set to default, if it exceeds the cap of sessions
 * it is set to the cap value, otherwise maxSession is the maximum number of session
 * Similarly for maxBufferOrder
 * @maxSession: requested maximum number of sessions
 * @bufferOrder: requested session buffer order
 */
int sessionInit(int maxSession, int bufferOrder) {
	int pages;
	if (maxSession > 0) {
		if (maxSession > MAX_SESSIONNUM) {
			maxSessionNum = MAX_SESSIONNUM;
		} else {
			maxSessionNum = maxSession;
		}
	}

	if (bufferOrder >= 0) {
		pages = 1 << bufferOrder;
		maxBufferOrder = bufferOrder;
		if (pages > MAX_PAGENUM) {
			pages = MAX_PAGENUM;
			maxBufferOrder = MAX_BUFFERORDER;
		}
		maxBufferSize = 4096 * pages;
	}
	return 0;
}

/*
 * IF the flags contain the O_SESSION bit, creates a new session based on the given file pointer.
 * @filePtr: a pointer to a file struct
 * @flags: open flags
 */
int sessionOpen(struct file *filePtr, int flags) {
	unsigned long count;
	unsigned long offset;
	unsigned long readenBytes;
	sessionData * sessionDataPtr;

	if (flags & O_SESSION) {
		// If the O_SESSION flag is present, check if a new session can be created and go ahead
		if (_atomicIncUnless(&sessionCount, maxSessionNum)
				>= maxSessionNum) {
			printk(KERN_WARNING "Too many session opened\n");
			return -EMFILE;
		}

		// Retrieve the file size, and check it against the maximum manageable file size
		count = filePtr->f_dentry->d_inode->i_size;
		if (count > maxBufferSize) {
			printk(KERN_WARNING "File too large\n");
			atomic_dec(&sessionCount);
			return -EFBIG;
		}

		// Allocate a pointer to a sessionData
		sessionDataPtr = (sessionData*) kmalloc(sizeof(sessionData),
		KAMLLOCFLAGS);
		if (sessionDataPtr == NULL ) {
			printk(KERN_WARNING "Can't allocate pointer_struct\n");
			atomic_dec(&sessionCount);
			return -ENOMEM;
		}

		// Allocate the session buffer
		sessionDataPtr->buffer = (char*) __get_free_pages(KAMLLOCFLAGS,
		maxBufferOrder);
		if (sessionDataPtr->buffer == NULL ) {
			printk(KERN_WARNING "Can't allocate session buffer\n");
			kfree(sessionDataPtr);
			atomic_dec(&sessionCount);
			return -ENOMEM;
		}
		sessionDataPtr->fileInBufferSize = 0;

		// Read the file and store it in the session buffer
		memset(sessionDataPtr->buffer, 0, maxBufferSize);
		readenBytes = 0;
		offset = 0;
		do {
			readenBytes = kernel_read(filePtr, offset,
			&sessionDataPtr->buffer[offset], count);
			if (readenBytes < 0) {
				printk(KERN_WARNING "Kernel read failed\n");
				kfree(sessionDataPtr->buffer);
				kfree(sessionDataPtr);
				atomic_dec(&sessionCount);
				return readenBytes;
			}
			sessionDataPtr->fileInBufferSize += readenBytes;
			count = count - readenBytes;
			offset += readenBytes;
		} while (count > 0);

		// Set the flag to avoid concurrent session FOPS
		atomic_set(&sessionDataPtr->usageCountAndFlag,BADSTATEFLAG);

		// Initialize both locks
		init_rwsem(&sessionDataPtr->fileInBufferLock);
		mutex_init(&sessionDataPtr->writeLock);

		// Save the original private_data pointer and file operations struct pointers in the sessionData
		sessionDataPtr->private_data = filePtr->private_data;
		sessionDataPtr->oldFops = filePtr->f_op;

		// Atomically switch file operations
		xchg(&filePtr->f_op, &session_fops);

		// Switch session and private data
		filePtr->private_data = (void*) sessionDataPtr;

		// Unlock session FOPS
		atomic_set(&sessionDataPtr->usageCountAndFlag,0);

		printk(KERN_ERR "Open Switch Done\n");

		// Locks the module until the session exists
		try_module_get(THIS_MODULE );
	}

	return 0;
}

/*
 * Session Read File Operation
 * Runs concurrently against other reads and writes, except when reading the current  size of the stored file
 */
ssize_t sessionRead(struct file * filePtr, char __user * buff, size_t count,
loff_t * pos) {
	ssize_t ret;
	char *addr;

	// Check if the bad state flag is raised. If not it increments the usage count, otherwise returns with an error
	if(_atomicIncUnlessSet(&getSessionData(filePtr)->usageCountAndFlag,BADSTATEFLAG) < 0 ){
		printk(KERN_ERR "Session Data in Bad State\n");
		return -EBADFD;
	}

	// Check if the given pos is inside the file in buffer size, taking the lock
	// protecting the field in read mode
	down_read(&getSessionData(filePtr) ->fileInBufferLock);
	if (*pos > getSessionData(filePtr) ->fileInBufferSize) {
		up_read(&getSessionData(filePtr) ->fileInBufferLock);
		printk(KERN_WARNING "Requested read overflows session buffer\n");
		atomic_dec(&getSessionData(filePtr) ->usageCountAndFlag);
		return -EOVERFLOW;
	}

	// Limit the read to the file in buffer size
	if ((*pos + count) > getSessionData(filePtr) ->fileInBufferSize) {
		count = getSessionData(filePtr) ->fileInBufferSize - *pos;
	}
	up_read(&getSessionData(filePtr) ->fileInBufferLock);

	addr = getSessionData(filePtr) ->buffer;

	// Performs the write (copy) of buff on the session buffer
	ret = copy_to_user(buff, &addr[*pos], count); //ret = number of non copied bytes
	*pos = *pos + count - ret;

	// Decrements the usage count
	atomic_dec(&getSessionData(filePtr) ->usageCountAndFlag);
	return count - ret;
}

/*
 * Session Write File Operation
 * Runs concurrently against reads, but exclusively against other writes taking the write lock
 */
ssize_t sessionWrite(struct file * filePtr, const char __user * buff,
size_t count, loff_t * pos) {
	ssize_t ret;
	char *addr;

	// Check if the bad state flag is raised. If not it increments the usage count, otherwise returns with an error
	if(_atomicIncUnlessSet(&getSessionData(filePtr)->usageCountAndFlag,BADSTATEFLAG) < 0 ){
		printk(KERN_ERR "Session Data in Bad State\n");
		return -EBADFD;
	}

	// Check if the given pos is inside the buffer
	if (*pos >= maxBufferSize) {
		printk(KERN_WARNING "Requested write overflows session buffer\n");
		atomic_dec(&getSessionData(filePtr) ->usageCountAndFlag);
		return -EOVERFLOW;
	}

	// Limit the read to the buffer size
	if ((count + *pos) > maxBufferSize) {
		count = maxBufferSize - *pos;
	}

	mutex_lock(&getSessionData(filePtr) ->writeLock);

	addr = getSessionData(filePtr) ->buffer;

	// Performs the "read" (copy) from the session buffer to buff
	ret = copy_from_user(&addr[*pos], buff, count); //ret = number of non copied bytes

	// Check if we must update the size of the stored file
	if (getSessionData(filePtr) ->fileInBufferSize < (*pos + (count - ret))) {
		// Since write are not executed concurrently , we only need to avoid some read to check the size of the
		// stored file when we are updating it
		down_write(&getSessionData(filePtr) ->fileInBufferLock);
		getSessionData(filePtr) ->fileInBufferSize = *pos + (count - ret);
		up_write(&getSessionData(filePtr) ->fileInBufferLock);
	}

	mutex_unlock(&getSessionData(filePtr) ->writeLock);

	*pos = *pos + count - ret;
	// Decrements the usage count
	atomic_dec(&getSessionData(filePtr) ->usageCountAndFlag);
	return count - ret;
}

/*
 * Session llseek File Operation
 * Mimicks the generic_file_llseek, returning errors on non supported operations and avoiding that pos overflows the
 * maximum buffer size
 */

loff_t sessionLlseek(struct file *filePtr, loff_t offset, int origin) {
	int ret;
	loff_t maxsize = maxBufferSize;

	// Check if the bad state flag is raised. If not it increments the usage count, otherwise returns with an error
	if(_atomicIncUnlessSet(&getSessionData(filePtr)->usageCountAndFlag,BADSTATEFLAG) < 0 ){
		printk(KERN_ERR "Session Data in Bad State\n");
		return -EBADFD;
	}

	switch (origin) {
	case SEEK_END:
		offset += getSessionData(filePtr) ->fileInBufferSize;
		break;

	case SEEK_CUR:
		if (offset == 0){
			// Decrements the usage count
			atomic_dec(&getSessionData(filePtr) ->usageCountAndFlag);
			return filePtr->f_pos;
		}

		spin_lock(&filePtr->f_lock);
		offset = _lseekExecute(filePtr, filePtr->f_pos + offset, maxsize);
		spin_unlock(&filePtr->f_lock);

		// Decrements the usage count
		atomic_dec(&getSessionData(filePtr) ->usageCountAndFlag);
		return offset;
		break;
	case SEEK_DATA:
		// Decrements the usage count
		atomic_dec(&getSessionData(filePtr) ->usageCountAndFlag);
		return -ENOTSUPP;
		break;
	case SEEK_HOLE:
		// Decrements the usage count
		atomic_dec(&getSessionData(filePtr) ->usageCountAndFlag);
		return -ENOTSUPP;
		break;
	}
	ret = _lseekExecute(filePtr, offset, maxsize);
	// Decrements the usage count
	atomic_dec(&getSessionData(filePtr) ->usageCountAndFlag);
	return ret;
}


/*
 * Session Flush File Operation
 * If this file operation is called, then a close has been requested, hence we tear down the session and write
 * back the data stored in the session buffer to the related file
 */
int sessionFlush(struct file * filePtr, fl_owner_t id) {
	sessionData* sessionDataPtr;
	int ret;
	loff_t pos = 0;
	size_t fileInBufferSize = 0;
	loff_t oldPos = 0;

	// Check if the related file is writable
	if (!(filePtr->f_mode & FMODE_WRITE )) {
		printk(KERN_WARNING "File is not writable\n");
		return -EBADF;
	}

	sessionDataPtr = getSessionData(filePtr);

	// If the bad state bit is already set it returns an error, otherwise it sets the bit
	if(_atomicSetUnlessSet(&sessionDataPtr->usageCountAndFlag, BADSTATEFLAG) < 0){
		printk(KERN_ERR "Session Data in Bad State\n");
		return -EBADFD;
	}

	// Loops until no fops is running
	while(atomic_read(&sessionDataPtr->usageCountAndFlag) != BADSTATEFLAG){
		msleep(1);
	};

	// Switches back the private data
	filePtr->private_data = sessionDataPtr->private_data;
	// Atomically switch back the fops
	xchg(&filePtr->f_op, sessionDataPtr->oldFops);

	fileInBufferSize = sessionDataPtr->fileInBufferSize;
	ret = -1;

	// Save the current pos and reset it to the beginning of the file
	spin_lock(&filePtr->f_lock);
	oldPos = filePtr->f_pos;
	spin_unlock(&filePtr->f_lock);
	filePtr->f_op->llseek(filePtr, 0, SEEK_SET);

	// Writes the content of the buffer on the file
	do {
		pos = filePtr->f_pos;

		ret = _writeSessionBufferToFile(filePtr, sessionDataPtr->buffer,
		fileInBufferSize, pos);

		filePtr->f_pos = pos;
		if (ret < 0) {
			printk(
					KERN_WARNING "Error while committing the sessione buffer to file %d\n",
					ret);

			// Rolls back to pre flush situation
			xchg(&filePtr->f_op, &session_fops);
			filePtr->private_data = (void*) sessionDataPtr;
			atomic_set(&sessionDataPtr->usageCountAndFlag, 0);

			spin_lock(&filePtr->f_lock);
			filePtr->f_pos = oldPos;
			spin_unlock(&filePtr->f_lock);
			return ret;
		}
		pos += ret;
		fileInBufferSize -= ret;
	} while (fileInBufferSize > 0);

	// The file may have grown, hence we must truncate it to the size of the session file
	_doTruncate(filePtr->f_dentry, sessionDataPtr->fileInBufferSize, 0,
			NULL );

	// Freeing session meta data
	free_pages((unsigned long) sessionDataPtr->buffer, maxBufferOrder);
	mutex_destroy(&sessionDataPtr->writeLock);
	kfree(sessionDataPtr);

	// Reduce the number of active sessions and the usage counter of the module
	atomic_dec(&sessionCount);
	module_put(THIS_MODULE );

	return 0;
}

