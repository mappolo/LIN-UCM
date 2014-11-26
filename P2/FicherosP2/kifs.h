#ifndef KIFS_H
#define KIFS_H
#include <linux/list.h> /* list_head */

#define MAX_SIZE_KIFS_ENTRY_NAME 50 


/* Predeclaration required */
struct kifs_entry;

/* Callback prototypes for kifs entries */
struct kifs_operations {
	int (*read)(struct kifs_entry* entry, char *user_buffer, unsigned int maxchars);	
	int (*write)(struct kifs_entry* entry, const char *user_buffer, unsigned int maxchars);	
};


/* Descriptor for a KIFS entry */
struct kifs_entry {
	char entryname[MAX_SIZE_KIFS_ENTRY_NAME];
	struct kifs_operations* ops;
	void *private_data;
	struct list_head links;	/* Set of links in kifs entry */ 
};

enum {
	KIFS_READ_OP=0,
	KIFS_WRITE_OP,
	KIFS_NR_OPS};


/* This fuction must ensure that no entry will be created as long as another entry with the same name already exists.
 * == Return Value ==
 * NULL -> Entry name already exists or no memory is available for the entry
 * Otherwise, return pointer to the kifs entry
 * */ 
struct kifs_entry* create_kifs_entry(const char* entryname, struct kifs_operations* ops);


/* Remove kifs entry
 * == Return Value ==
 * -1 Entry does not exist
 *  0 success
 * */ 
int remove_kifs_entry(const char* entry_name);



/* Data types and functions for the module that implements the KIFS subsystem */
struct kifs_impl_operations {
	struct kifs_entry* (*create_kifs_entry)(const char* entryname, struct kifs_operations* ops);
	int (*remove_kifs_entry)(const char* entry_name);
	long (*sys_kifs)(const char* entry_name, unsigned int op_mode, char* user_buffer,unsigned int maxchars);	
};

/* These functions must be invoked by the Kernel Module to install
	the functions that provide the functionality for the KIFS subsystem */
int register_kifs_implementation(struct kifs_impl_operations* ops);
int unregister_kifs_implementation(struct kifs_impl_operations* ops);

#endif
