#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kifs.h>
#include <asm-generic/uaccess.h>

MODULE_AUTHOR("Miguel Angel Perez");
MODULE_DESCRIPTION("Modulo contador sobre sistema de ficheros virtual KIFS");
MODULE_LICENSE("GPL");

#define KIFS_ENTRYNAME "counter"

int count;

inline int digit_count(int a){
	int ret = 0;
	while(a){
		a /= 10;
		ret++;
	}
	return ret;
}

/**/int kifs_read_KIFS_ENTRYNAME(struct kifs_entry* entry, char *user_buffer, unsigned int maxchars){
	char kbuff[11];
	int used = 0;
	
	if(maxchars <= (used = snprintf(kbuff,10,"%d",count)))
		return -ENOSPC;
	used++; //Por el \0
	
	used -= copy_to_user(user_buffer, kbuff, used);
	
	return used;
}

/**/int kifs_write_KIFS_ENTRYNAME(struct kifs_entry* entry, char *user_buffer, unsigned int maxchars){
	count ++;
	return maxchars;
}
/**/
	static struct kifs_operations KIFS_ENTRYNAME_operations = {
	.read=kifs_read_KIFS_ENTRYNAME,
	.write=kifs_write_KIFS_ENTRYNAME,
};


int init_KIFS_ENTRYNAME(void){

	create_kifs_entry("counter",&KIFS_ENTRYNAME_operations);

	count = 0;
	return 0;
}
module_init(init_KIFS_ENTRYNAME);



/**/void exit_KIFS_ENTRYNAME(void){
	remove_kifs_entry(KIFS_ENTRYNAME);		
}
module_exit(exit_KIFS_ENTRYNAME);
