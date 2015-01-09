#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <asm-generic/uaccess.h>

#define MODLIST_DEBUG
#ifdef MODLIST_DEBUG
#define DBG(format, arg...) do { \
			printk(KERN_DEBUG "%s: " format "\n" , __func__ , ## arg); \
		} while (0)
#else
#define DBG(format, arg...) /* */
#endif

#define proc_entry_name "modlist"
#define streq(a,b) (strcmp((a),(b)) == 0)
#define MAX_ITEMS 100

DEFINE_RWLOCK(mutex_list);


MODULE_AUTHOR("Miguel Angel Perez");
MODULE_DESCRIPTION("Implementa una lista de enteros en un módulo"\
					"de kernel administrable por una entrada en /proc");
MODULE_LICENSE("GPL");
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
	
	write_lock(&mutex_list);   // Entra sección crítica
	
	list_for_each_entry_safe(elem, aux, list, links){
		remove_singel_item(elem);
	}
	write_unlock(&mutex_list); // Sale sección crítica
}

void add_item_list(struct list_head *list, int data){
	list_item_t *item = vmalloc(sizeof(list_item_t));
	item->data = data;
	write_lock(&mutex_list);   // Entra sección crítica
	
	list_add(&(item->links),list);
	
	write_unlock(&mutex_list); // Sale sección crítica
}

void remove_items_list(struct list_head *list, int data){
	list_item_t *aux, *elem = NULL;
	
	write_lock(&mutex_list);   // Entra sección crítica
	
	list_for_each_entry_safe(elem, aux, list, links){
		if(elem->data == data)
			remove_singel_item(elem);
	}
	
	write_unlock(&mutex_list); // Sale sección crítica
}

/*
 *   Manejo de la entrada en /proc
 *
 */


static ssize_t modlist_read(struct file *filp, char __user *buf, size_t len, loff_t *off) {
  list_item_t *item = NULL;
  struct list_head *cur_node=NULL;
  
  char kbuf[MAX_ITEMS]; 
  char* dest=kbuf; 
  if ((*off) > 0) 
    return 0;
    
    read_lock(&mutex_list);   // Entra sección crítica
    
  list_for_each(cur_node,&mylist){
	item = list_entry(cur_node, list_item_t, links);	
        dest += sprintf(dest, "%i\n", item->data); 
  }
  
    
  if (copy_to_user(buf, kbuf,dest-kbuf)) 
	return -EINVAL;
  *off+=len; 
  
  read_unlock(&mutex_list); // Sale sección crítica
  
  return (dest-kbuf);
}

static ssize_t modlist_write(struct file *file, const char __user *buffer, unsigned long count, loff_t *data)
{
	char line[25] = ""; 
	char command[8] = "";
	int num = 0;
	
	DBG("[modlist] evento de escritura: %lu char", count);
	
	copy_from_user(line, buffer, count);
	line[count] = 0;
	DBG("[modlist] se han copiado al buffer del kernel: %lu char", count);
	
	sscanf(line,"%7s %d", command, &num);
	
	if(streq(command,"add")){
		add_item_list(&mylist, num);
	}else if(streq(command,"cleanup")){
		clear_list(&mylist);
	}else if(streq(command,"remove")){
		remove_items_list(&mylist, num);
	}

	DBG("\t evento realizado: %s [%d]",command, num);
	
	return count;
}

/*
 *   Funciones de carga y descarga del módulo
 *
 *
 */

static const struct file_operations proc_entry_fops = {
    .read = modlist_read,
    .write = modlist_write,    
};

int modlist_init(void){
	DBG("[modlist] Cargado. Creando entrada /proc/modlist");
	modlist_proc_entry = proc_create(proc_entry_name,0666,NULL, &proc_entry_fops);
		return 0;
}

void modlist_clean(void){
	DBG("[modlist] Descargado. Eliminando entrada /proc/modlist");
	remove_proc_entry(proc_entry_name, NULL);
	clear_list(&mylist);
}

module_init(modlist_init);
module_exit(modlist_clean);
