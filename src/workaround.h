/*
 ============================================================================
 Name        : workaround.h
 Author      : Eleonora Calore & Nicolò Rivetti
 Created on  : Nov 9, 2012
 Version     : 1.0
 Copyright   : Copyright (c) 2012  Eleonora Calore & Nicolò Rivetti
 Description : Declaration and Implementation of the function that are required
 	 	 	 by the session module but which symbols are not exported and
 	 	 	 functions to provide some atomic operations not supported by the
 	 	 	 Kernel
 ============================================================================
 */

#ifndef WORKAROUND_H_
#define WORKAROUND_H_

#include <linux/fs.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <linux/file.h>
#include <linux/gfp.h>

/*
 * Increments the atomic counter only if the current value is not threshold
 */
inline int _atomicIncUnless(atomic_t *v, int threshold){
	int currentValue, previousValue;
	// Reads the current value
	currentValue = atomic_read(v);
	for (;;) {
		// If the current value is the threshold, return
		if (unlikely(currentValue == (threshold)))
			break;
		// Try to compare and swap currentValue with currentValue++, returns the read value in previousValue
		previousValue = atomic_cmpxchg((v), currentValue, currentValue+(1));
		// If the value has not been changed by someone else, return
		if (likely(previousValue == currentValue))
			break;
		// If the value has been changed by someone else, try again
		currentValue = previousValue;
	}
	return currentValue;
}

/*
 * ORs the current value with the bitMask if it has not been done already
 */
inline int _atomicSetUnlessSet(atomic_t *v, int bitMask){
	int currentValue, previousValue;
	// Reads the current value
	currentValue = atomic_read(v);
	for (;;) {
		// If the current value is the threshold, return
		if (unlikely(currentValue & (bitMask)))
			return -1;
		// Try to compare and swap currentValue with currentValue++, returns the read value in previousValue
		previousValue = atomic_cmpxchg((v), currentValue, currentValue | (bitMask));
		// If the value has not been changed by someone else, return
		if (likely(previousValue == currentValue))
			break;
		// If the value has been changed by someone else, try again
		currentValue = previousValue;
	}
	return 0;
}

/*
 * Increments the current value if it does not contain the bitMask
 */
inline int _atomicIncUnlessSet(atomic_t *v, int bitMask){
	int currentValue, previousValue;
	// Reads the current value
	currentValue = atomic_read(v);
	for (;;) {
		// If the current value is the threshold, return
		if (unlikely(currentValue & (bitMask)))
			return -1;
		// Try to compare and swap currentValue with currentValue++, returns the read value in previousValue
		previousValue = atomic_cmpxchg((v), currentValue, currentValue+(1));
		// If the value has not been changed by someone else, return
		if (likely(previousValue == currentValue))
			break;
		// If the value has been changed by someone else, try again
		currentValue = previousValue;
	}
	return 0;
}


/*
 * Mimics the kernel_write function which symbol is not exported
 */
ssize_t _writeSessionBufferToFile(struct file *file, const char *buf,
		size_t count, loff_t pos) {
	mm_segment_t old_fs;
	ssize_t res;

	old_fs = get_fs();
	set_fs(get_ds());
	/* The cast to a user pointer is valid due to the set_fs() */
	res = vfs_write(file, (const char __user *) buf, count, &pos);
	set_fs(old_fs);

	return res;
}

/*
 * Mimics the do_truncate function which symbol is not exported
 */
int _doTruncate(struct dentry *dentry, loff_t length, unsigned int time_attrs,
	struct file *filp)
{
	int ret;
	struct iattr newattrs;

	/* Not pretty: "inode->i_size" shouldn't really be signed. But it is. */
	if (length < 0)
		return -EINVAL;

	newattrs.ia_size = length;
	newattrs.ia_valid = ATTR_SIZE | time_attrs;
	if (filp) {
		newattrs.ia_file = filp;
		newattrs.ia_valid |= ATTR_FILE;
	}

	/* Remove suid/sgid on truncate too */
	ret = should_remove_suid(dentry);
	if (ret)
		newattrs.ia_valid |= ret | ATTR_FORCE;

	mutex_lock(&dentry->d_inode->i_mutex);
	ret = notify_change(dentry, &newattrs);
	mutex_unlock(&dentry->d_inode->i_mutex);
	return ret;
}

/*
 * Mimics the usigned_offset function which is required by the lseek_execute
 */
static inline int _unsignedOffsets(struct file *file)
{
	return file->f_mode & FMODE_UNSIGNED_OFFSET;
}

/*
 * Mimics the llseek_execute which symbol is not exported
 */
loff_t _lseekExecute(struct file *file, loff_t offset, loff_t maxsize)
{
	if (offset < 0 && !_unsignedOffsets(file))
		return -EINVAL;
	if (offset > maxsize)
		return -EINVAL;

	if (offset != file->f_pos) {
		file->f_pos = offset;
		file->f_version = 0;
	}
	return offset;
}

#endif /* WORKAROUND_H_ */
