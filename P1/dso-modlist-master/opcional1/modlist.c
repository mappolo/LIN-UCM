#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/types.h>
#include <linux/list.h>
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

MODULE_AUTHOR("R.S.R.");
MODULE_DESCRIPTION("Implementa una lista de enteros ordenables en un módulo"\
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

void sort_list(struct list_head *list){
	list_item_t *aux, *elem, *piv = list_first_entry(list, list_item_t, links);
	LIST_HEAD(greater);
	
	if(list_empty(list) || list_is_singular(list))
		return;		
		
	list_del(&(piv->links));
	
	list_for_each_entry_safe(elem, aux, list, links){
		if(elem->data > piv->data)
			list_move_tail(&(elem->links),&greater);
	} 
	
	sort_list(list);
	sort_list(&greater);
	
	list_add(&(piv->links),&greater); 
	// es importante que greater no este vacia
	
	// Unimos las listas
	list->prev->next = greater.next;
	greater.next->prev = list->prev;
	list->prev = greater.prev;
	greater.prev->next = list;
}

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

int procfile_read(char *buffer, char **buffer_location,
		  off_t offset, int buffer_len, int *eof, void *data)
{
	list_item_t *elem = NULL;
	int used = 0;
	
	DBG("[modlist] evento de lectura [max]: %d char", buffer_len);
	
	list_for_each_entry(elem, &mylist, links){
		if(buffer_len - used == 0) 
			break;
		used += snprintf(buffer + used, buffer_len - used, 
				"%d\n", elem->data);	
	}
	return used;
}

int procfile_write(struct file *file, const char __user *buffer, 
		   unsigned long count, void *data)
{
	char line[25] = "";
	char command[8] = "";
	int num = 0;
	
	DBG("[modlist] evento de escritura: %lu char", count);
	
	copy_from_user(line, buffer, min(count,24UL));
	line[min(count,24UL)] = 0;
	DBG("[modlist] se han copiado al buffer del kernel: %lu char", min(count,24UL));
	
	sscanf(line,"%7s %d", command, &num);
	
	DBG("\t [i]-evento pedido: %s [%d]",command, num);
	
	if(streq(command,"add")){
		add_item_list(&mylist, num);
	}else if(streq(command,"cleanup")){
		clear_list(&mylist);
	}else if(streq(command,"remove")){
		remove_items_list(&mylist, num);
	}else if(streq(command,"sort")){
		sort_list(&mylist);
	}

	DBG("\t [*]-evento finalizado :)");
	
	return min(count,24UL);
}

/*
 *   Funciones de carga y descarga del módulo
 *
 *
 */


int modlist_init(void){
	DBG("[modlist] Cargado. Creando entrada /proc/modlist");
	modlist_proc_entry = create_proc_entry(proc_entry_name,0666,NULL);
	modlist_proc_entry->read_proc = procfile_read;
	modlist_proc_entry->write_proc = procfile_write;
	return 0;
}

void modlist_clean(void){
	DBG("[modlist] Descargado. Eliminando entrada /proc/modlist");
	remove_proc_entry(proc_entry_name, NULL);
	clear_list(&mylist);
}

module_init(modlist_init);
module_exit(modlist_clean);
