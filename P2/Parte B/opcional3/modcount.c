#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kifs.h>
#include <asm-generic/uaccess.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/list.h>
#include <linux/proc_fs.h>

#define MODLIST_DEBUG
#ifdef MODLIST_DEBUG
#define DBG(format, arg...) do { \
			printk(KERN_DEBUG "%s: " format "\n" , __func__ , ## arg); \
		} while (0)
#else
#define DBG(format, arg...) /* */
#endif
MODULE_AUTHOR("Miguel Angel Perez");
MODULE_DESCRIPTION("Modulo contador sobre sistema de ficheros virtual KIFS");
MODULE_LICENSE("GPL");

#define KIFS_ENTRYNAME "counter"
#define proc_entry_name "modlist"
#define streq(a,b) (strcmp((a),(b)) == 0)
#define MAX_ITEMS 100

struct proc_dir_entry *modlist_proc_entry;
LIST_HEAD(mylist);

/*
 *  Definición de manejo de elementos de la lista
 *
 */

typedef struct {
       int data;
       struct list_head links;
}list_item_t;


inline void remove_singel_item(list_item_t *elem){
	list_del(&(elem->links));
	vfree(elem);
}

/*
 *	FUNCIONES DE MANEJO DE LISTA
 *	--------------------------------------
 */

void clear_list(struct list_head *list){
	list_item_t *aux, *elem = NULL;
	
	list_for_each_entry_safe(elem, aux, list, links){
		remove_singel_item(elem);
	}
}

void add_item_list(struct list_head *list, int data){
	list_item_t *item = vmalloc(sizeof(list_item_t));
	item->data = data;
	list_add(&(item->links),list);
}

void remove_items_list(struct list_head *list, int data){
	list_item_t *aux, *elem = NULL;
	
	list_for_each_entry_safe(elem, aux, list, links){
		if(elem->data == data)
			remove_singel_item(elem);
	}
}

/*
 *   Manejo de la entrada en /proc
 *
 */

//Funcion de lectura de la entrada modlist
int kifs_read_proc_entry_name(struct kifs_entry* entry, char *user_buffer, unsigned int maxchars) {
  list_item_t *item = NULL;
  struct list_head *cur_node=NULL;
  
  char kbuf[MAX_ITEMS]; //Buffer de uso interno
  char* dest=kbuf; //Puntero para recorrer el buffer

  list_for_each(cur_node,&mylist){
	item = list_entry(cur_node, list_item_t, links);	
        dest += sprintf(dest, "%i\n", item->data); //Avanzo el puntero para no machacar el buffer
  }
  if (copy_to_user(user_buffer, kbuf,dest-kbuf)) //Cuando se tiene todo el contenido de la entrada se pasa al espacio de usuario
	return -EINVAL;
  
  return (dest-kbuf);
}
//Funcion de escritura de la entrada modlist
int kifs_write_proc_entry_name(struct kifs_entry* entry, const char *user_buffer, unsigned int maxchars)
{
	char line[25] = ""; // Un entero no puede ocupar más de 10 caracteres
	char command[8] = "";
	int num = 0;
	
		
	copy_from_user(line, user_buffer, maxchars);
	line[maxchars] = '\0';
	
	
	sscanf(line,"%7s %d", command, &num);
	
	if(streq(command,"add")){
		add_item_list(&mylist, num);
	}else if(streq(command,"cleanup")){
		clear_list(&mylist);
	}else if(streq(command,"remove")){
		remove_items_list(&mylist, num);
	}

	DBG("\t evento realizado: %s [%d]",command, num);
	
	return maxchars;
}


/*Inicio de la entrada Counter*/

int count;

//Funcion Lectura de la entrada counter
int kifs_read_KIFS_ENTRYNAME(struct kifs_entry* entry, char *user_buffer, unsigned int maxchars){
	char kbuff[11];
	int used = 0;
	
	if(maxchars <= (used = snprintf(kbuff,10,"%d",count)))
		return -ENOSPC;
	used++; //Por el \0
	
	used -= copy_to_user(user_buffer, kbuff, used);
	
	return used;
}
//Funcion escritua de la entrada counter
int kifs_write_KIFS_ENTRYNAME(struct kifs_entry* entry,  const char *user_buffer, unsigned int maxchars){
	count ++;
	return maxchars;
}
//Definicion Operaciones counter
	static struct kifs_operations KIFS_ENTRYNAME_operations = {
	.read=kifs_read_KIFS_ENTRYNAME,
	.write=kifs_write_KIFS_ENTRYNAME,
};
//Definicion Operaciones modlist
	static struct kifs_operations proc_entry_name_operations = {
	.read=kifs_read_proc_entry_name,
	.write=kifs_write_proc_entry_name,
};


int init_KIFS_ENTRYNAME(void){

	create_kifs_entry("counter",&KIFS_ENTRYNAME_operations); //Crea entrada counter
	create_kifs_entry("modlist",&proc_entry_name_operations); //Crea entrada modlist

	count = 0;
	return 0;
}
module_init(init_KIFS_ENTRYNAME);



void exit_KIFS_ENTRYNAME(void){
	remove_kifs_entry(KIFS_ENTRYNAME);
	remove_kifs_entry(proc_entry_name);
	printk(KERN_INFO "Counter y Modlist Descargados.\n");
}
module_exit(exit_KIFS_ENTRYNAME);
