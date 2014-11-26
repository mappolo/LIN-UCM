#include <linux/kifs.h> 
#include <asm/string.h>
#include <linux/module.h> /* For EXPORT_SYMBOL() */
#include <linux/syscalls.h> /* For SYSCALL_DEFINE4() */
#include <asm-generic/errno.h>

#ifndef NULL
#define NULL 0
#endif

struct kifs_impl_operations* kifs_impl=NULL; /* When the kernel boots, KIFS is not active.
						No implementation has been registered yet. */

int register_kifs_implementation(struct kifs_impl_operations* ops){
	if (kifs_impl==NULL) {
		kifs_impl=ops;
		return 0;
	}else
		return -1;
}
EXPORT_SYMBOL(register_kifs_implementation);

int unregister_kifs_implementation(struct kifs_impl_operations* ops) {
	if (kifs_impl==ops) {
		kifs_impl=NULL;
		return 0;
	}else
		return -1;	
	
}
EXPORT_SYMBOL(unregister_kifs_implementation);

struct kifs_entry* create_kifs_entry(const char* entry_name, struct kifs_operations* ops) {
	
	if (kifs_impl && kifs_impl->create_kifs_entry){
		return kifs_impl->create_kifs_entry(entry_name,ops);
	}
	else return NULL;
}
EXPORT_SYMBOL(create_kifs_entry);

/* Remove kifs entry()
 * == Return Value ==
 * -ENOSYS: Function not implemented	
 * -1 Entry does not exist
 *  0 success
 * */ 
int remove_kifs_entry(const char* entry_name){
	if (kifs_impl && kifs_impl->remove_kifs_entry){
		return kifs_impl->remove_kifs_entry(entry_name);
	}
	else return -ENOSYS; //Function not implemented	
}
EXPORT_SYMBOL(remove_kifs_entry);

/* sys_kifs()
 * == Return Value ==
 * -ENOSYS: Function not implemented	
 *  <0 : Error
 *  >: NUmber of bytes/chars read or written
 * */ 
SYSCALL_DEFINE4(kifs, const char*, entry_name, unsigned int, op_mode, char*, user_buffer,unsigned int, maxchars) {
	printk(KERN_INFO "Invoking kifs() syscall\n");
	if (kifs_impl && kifs_impl->sys_kifs){
		return kifs_impl->sys_kifs(entry_name,op_mode,user_buffer,maxchars);
	}
	else return -ENOSYS; //Function not implemented	
}
