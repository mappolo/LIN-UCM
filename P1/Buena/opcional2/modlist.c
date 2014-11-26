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
#define MAX_ITEMS 100


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
 
 #define DATA_SIZE 10
#define DATA_SIZE_STR "20"

typedef struct {
       char data[DATA_SIZE];
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

void add_item_list(struct list_head *list, char *data){
	list_item_t *item = vmalloc(sizeof(list_item_t));
	int i;
	
		for(i = 0; i < DATA_SIZE; i++){
		item->data[i] = data[i];
		if(!data[i]) break;
	}
	
	list_add(&(item->links),list);
}

void remove_items_list(struct list_head *list, char *data){
	list_item_t *aux, *elem = NULL;
	
	list_for_each_entry_safe(elem, aux, list, links){
		if(streq(elem->data, data))
			remove_singel_item(elem);
	}
}




/*
 *   Manejo de la entrada en /proc
 *
 */


static ssize_t modlist_read(struct file *filp, char __user *buf, size_t len, loff_t *off) {
  list_item_t *item = NULL;
  struct list_head *puntero_nodo=NULL;
  
  char kbuffer[MAX_ITEMS]; //Buffer de uso interno
  char* dest=kbuffer; //Puntero para recorrer el buffer
  if ((*off) > 0) /* The application can write in this entry just once !! */
    return 0;
  list_for_each(puntero_nodo,&mylist){
	item = list_entry(puntero_nodo, list_item_t, links);	
        dest += sprintf(dest, "%i\n", item->data); //Avanzo el puntero para no machacar el buffer
  }
  if (copy_to_user(buf, kbuffer,dest-kbuffer)) //Cuando se tiene todo el contenido de la entrada se pasa al espacio de usuario
	return -EINVAL;
  *off+=len; 
  return (dest-kbuffer);
}

static ssize_t modlist_write(struct file *file, const char __user *buffer, unsigned long count, loff_t *data)
{
	char line[25] = ""; // Un entero no puede ocupar más de 10 caracteres
	char command[8] = "";
	int num = 0;
	
	DBG("[modlist] evento de escritura: %lu char", count);
	
	copy_from_user(line, buffer, min(count,24UL));
	line[min(count,24UL)] = 0;
	DBG("[modlist] se han copiado al buffer del kernel: %lu char", min(count,24UL));
	
	sscanf(line,"%7s %d", command, &num);
	
	if(streq(command,"add")){
		add_item_list(&mylist, num);
	}else if(streq(command,"cleanup")){
		clear_list(&mylist);
	}else if(streq(command,"remove")){
		remove_items_list(&mylist, num);
	}else if (strcmp(command,"sort")==0){
  	sort_list(&mylist);
  }

	DBG("\t evento realizado: %s [%d]",command, num);
	
	return min(count,24UL);
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
