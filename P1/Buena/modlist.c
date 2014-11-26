#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/types.h>
#include <linux/list.h>
#include <asm-generic/uaccess.h>



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
	list_add_tail(&(item->links),list);
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


static ssize_t modlist_read(struct file *filp, char __user *buf, size_t len, loff_t *off) {
  list_item_t *item = NULL; // Puntero a list_item_t o Todo el bloque
  struct list_head *cur_node=NULL; //Puntero a list_head o prev y next
  
  char kbuf[MAX_ITEMS]; //Buffer de uso interno
  char* dest=kbuf; //Puntero para recorrer el buffer
  if ((*off) > 0) /* The application can write in this entry just once !! */
    return 0;
  list_for_each(cur_node,&mylist){ //Recorro la lista
	item = list_entry(cur_node, list_item_t, links); //Guardo en item el nodo actual	
        dest += sprintf(dest, "%i\n", item->data); //Avanzo el puntero interno del kbuf para no machacar el buffer
  }
  if (copy_to_user(buf, kbuf,dest-kbuf)) //Cuando se tiene todo el contenido de la entrada se pasa al espacio de usuario
	return -EINVAL;
  *off+=len; //Avanza buffer usuario
  return (dest-kbuf); 
}

static ssize_t modlist_write(struct file *file, const char __user *buffer, unsigned long count, loff_t *data)
{
	char line[25] = ""; // Un entero no puede ocupar más de 10 caracteres
	char command[8] = "";//Defino la operacion de 8 caracteres
	int num = 0;
	
	
	
	copy_from_user(line, buffer, min(count,24UL));
	line[min(count,24UL)] = 0;
	
	
	sscanf(line,"%7s %d", command, &num);
	
	if(streq(command,"add")){
		add_item_list(&mylist, num);
	}else if(streq(command,"cleanup")){
		clear_list(&mylist);
	}else if(streq(command,"remove")){
		remove_items_list(&mylist, num);
	}

	
	
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
		modlist_proc_entry = proc_create(proc_entry_name,0666,NULL, &proc_entry_fops);
		return 0;
}

void modlist_clean(void){
	remove_proc_entry(proc_entry_name, NULL);
	clear_list(&mylist);
}

module_init(modlist_init);
module_exit(modlist_clean);
