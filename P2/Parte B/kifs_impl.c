#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/list.h>
#include <linux/kifs.h>
#include <asm-generic/uaccess.h>

MODULE_AUTHOR("Miguel Angel Perez");
MODULE_DESCRIPTION("Implementa una entrada"\
					"de kernel administrable por una entrada en /Kifs");
MODULE_LICENSE("GPL");

#define MAX_KIFS_ENTRY 10
#define MAX_KIFS_ENTRY_NAME_SIZE 50 
#define MAX_KIFS_ENTRY_NAME_SIZE_STR "100"
#define CLIP_SIZE 50
#define BUFFER_LENGTH       PAGE_SIZE
struct kifs_entry entry_buff[MAX_KIFS_ENTRY];

LIST_HEAD(entry_list);

struct kifs_entry* my_create_kifs_entry(const char * entryname, struct kifs_operations* ops) {
 	struct kifs_entry *ptr=NULL;
	struct list_head *lista=NULL;
		printk(KERN_INFO "SALIENDO %s\n",entryname);
	list_for_each(lista,&entry_list){
		ptr = list_entry(lista, struct kifs_entry, links);
		printk(KERN_INFO "%s\n",entryname);
		if(strcmp(ptr->entryname, entryname) == 0){
			printk(KERN_INFO "[KIFS] Entrada duplicada");
			return NULL;
		}
	}

	ptr = vmalloc(sizeof(struct kifs_entry));
        strcpy(ptr->entryname, entryname);
	ptr->ops = ops;
	list_add_tail(&(ptr->links),&entry_list);
	if ((strcmp(ptr->entryname,"list") !=0) && (strcmp(ptr->entryname,"clipborad") !=0)){
		try_module_get(THIS_MODULE);
	}
	return ptr;
	
}

int my_remove_kifs_entry(const char * entry_name) {
	struct kifs_entry *ptr;
	list_for_each_entry(ptr,&entry_list,links){
		if(strncmp(entry_name,ptr->entryname, MAX_KIFS_ENTRY_NAME_SIZE) == 0){
			list_del(&(ptr->links));
			module_put(THIS_MODULE);
			return 0;
		}
	}
	return -EINVAL;
	
}

long my_sys_kifs(const char * entry_name,unsigned int op_mode,char * user_buffer,unsigned int maxchars) {
	struct kifs_entry *elem = NULL;
	char kbuff[MAX_KIFS_ENTRY_NAME_SIZE];
	memset(kbuff,0,MAX_KIFS_ENTRY_NAME_SIZE);
	strncpy_from_user(kbuff, entry_name, MAX_KIFS_ENTRY_NAME_SIZE);
	list_for_each_entry(elem, &entry_list, links){
		if(strncmp(elem->entryname, kbuff, MAX_KIFS_ENTRY_NAME_SIZE) == 0){
			switch(op_mode){
				case KIFS_READ_OP:
					if(elem->ops->read == NULL)
						return -EPERM;
					return elem->ops->read(elem,user_buffer,maxchars);
				case KIFS_WRITE_OP:
					if(elem->ops->write == NULL)
						return -EPERM;
					return elem->ops->write(elem,user_buffer,maxchars);
				default:
					return -EINVAL;
			}
		}
	}
	return -EINVAL;
}

struct kifs_impl_operations ops = {
	.create_kifs_entry=my_create_kifs_entry,
	.remove_kifs_entry=my_remove_kifs_entry,
	.sys_kifs=my_sys_kifs,
};

int list_read (struct kifs_entry* entry, char *user_buffer, unsigned int maxchars){
  struct kifs_entry *item = NULL;
  struct list_head *cur_node=NULL;
  char kbuf[MAX_KIFS_ENTRY_NAME_SIZE]=""; //Buffer de uso interno
  char* dest=kbuf; //Puntero para recorrer el buffer
  list_for_each(cur_node,&entry_list){
	item = list_entry(cur_node, struct kifs_entry, links);	
        dest += sprintf(dest, "%s\n", item->entryname); //Avanzo el puntero para no machacar el buffer
  }
  if (copy_to_user(user_buffer, kbuf,dest-kbuf)) //Cuando se tiene todo el contenido de la entrada se pasa al espacio de usuario
	return -EINVAL;
  return (dest-kbuf); 
}

static struct kifs_operations list_operations = {
	.read=list_read,
};

static char *clipboard;  // Space for the "clipboard"

int clipboard_write(struct kifs_entry* entry, const char *user_buffer, unsigned int maxchars) {  
  /* Transfer data from user to kernel space */
  if (copy_from_user( &clipboard[0], user_buffer, maxchars ))  
    return -EFAULT;
  clipboard[maxchars] = '\0'; /* Add the `\0' */
  return maxchars;
}

int clipboard_read(struct kifs_entry* entry, char *user_buffer, unsigned int maxchars) {
  int nr_bytes;
  nr_bytes=strlen(clipboard);
    /* Transfer data from the kernel to userspace */  
  if (copy_to_user(user_buffer,clipboard,nr_bytes))
    return -EINVAL;
  return nr_bytes; 
}

static struct kifs_operations clipboard_operations = {
	.read=clipboard_read,
	.write=clipboard_write,
};

int init_modlist_module( void )
{
	clipboard = (char *)vmalloc( BUFFER_LENGTH );
	register_kifs_implementation(&ops);        
	create_kifs_entry("clipboard",&clipboard_operations);
	create_kifs_entry("list",&list_operations);
	printk(KERN_INFO "[KIFS] CARGANDO MODULO");
	return 0;
}

void exit_modlist_module( void )
{
	unregister_kifs_implementation(&ops);
        printk(KERN_INFO "[KIFS] DESCARGAO EL MODULO");
}

module_init( init_modlist_module );
module_exit( exit_modlist_module );
