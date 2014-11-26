#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/types.h>
#include <linux/list.h>
#include <asm-generic/uaccess.h>



#define proc_entry_name "modlist"
#define MAX_ITEMS 100


MODULE_AUTHOR("Miguel Angel Perez");
MODULE_DESCRIPTION("Implementa una lista de enteros en un módulo"\
					"de kernel administrable por una entrada en /proc");
MODULE_LICENSE("GPL");
struct proc_dir_entry *proc_entry_name_proc_entry;

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
   Funcion para ordenar lista
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




/*
 *   Manejo de la entrada en /proc
 *
 */


static ssize_t proc_entry_name_read(struct file *filp, char __user *buf, size_t len, loff_t *off) {
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


static ssize_t proc_entry_name_write(struct file *file, const char __user *buffer, unsigned long count, loff_t *data){


 char line[MAX_ITEMS];
  char command[8];
  int num;
	if (count>MAX_ITEMS)
		return -ENOSPC;
		
	if (copy_from_user(line, buffer, count))
		return -EINVAL;
	
	line[count] = '\0';
 
  sscanf(line,"%s %d", command, &num);
  if(strcmp(command,"add")==0){
	add_item_list(&mylist, num); 
  }else if (strcmp(command,"remove")==0){
  	remove_items_list(&mylist, num); 
  }else if (strcmp(command,"cleanup")==0){
	clear_list(&mylist);
  }else if (strcmp(command,"sort")==0){
  	sort_list(&mylist);
  }
  *data+=count; 
  return count;
}

/*
 *   Funciones de carga y descarga del módulo
 *
 *
 */

static const struct file_operations proc_entry_fops = {
    .read = proc_entry_name_read,
    .write = proc_entry_name_write,    
};

int proc_entry_name_init(void){
		proc_entry_name_proc_entry = proc_create(proc_entry_name,0666,NULL, &proc_entry_fops);
		return 0;
}

void proc_entry_name_clean(void){
	remove_proc_entry(proc_entry_name, NULL);
	clear_list(&mylist);
}

module_init(proc_entry_name_init);
module_exit(proc_entry_name_clean);
