#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/list.h>
#include <asm-generic/uaccess.h>

MODULE_LICENSE("GPL");

#define BUFFER_LENGTH       PAGE_SIZE
#define MAX_ITEMS 100

static struct proc_dir_entry *proc_entry;
//static char *modlist;  // Space for the "clipboard"

struct list_head mylist;
typedef struct {
	int data;
	struct list_head links;
}list_item_t;
list_item_t *nodo;
LIST_HEAD(mylist);

void anadir(struct list_head *list, int num){
  nodo = vmalloc(sizeof(list_item_t)); // Reserva de memoria para la creacion del nuevo NODO.
  nodo->data=num; // Añadir dato al nuevo nodo.
  list_add(&(nodo->links),list); //Añadir nodo a la lista.
}

void borrar_items(struct list_head *list, int num){
	list_item_t *aux, *elem = NULL;
	list_for_each_entry_safe(elem, aux, list, links){
		if(elem->data == num){
			list_del(&(elem->links));
	         	vfree(elem);		
		}			
	}
}

void vaciar(struct list_head *list){
	list_item_t *aux, *elem = NULL;
	list_for_each_entry_safe(elem, aux, list, links){
		list_del(&(elem->links));
	        vfree(elem);
	}
}

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

static ssize_t modlist_write(struct file *filp, const char __user *buf, size_t len, loff_t *off) {
  char param[50];
  char ope[6];
  int num;
  if ((*off) > 0) /* The application can write in this entry just once !! */
    return 0;
 
  copy_from_user(param, buf, len);
 
  sscanf(param,"%s %d", ope, &num);
  if(strcmp(ope,"add")==0){
	anadir(&mylist, num); 
  }else if (strcmp(ope,"remove")==0){
  	borrar_items(&mylist, num); 
  }else if (strcmp(ope,"cleanup")==0){
	vaciar(&mylist);
  }else if (strcmp(ope,"sort")==0){
  	sort_list(&mylist);
  }
  *off+=len; 
  return len;
}

static ssize_t modlist_read(struct file *filp, char __user *buf, size_t len, loff_t *off) {
  list_item_t *item = NULL;
  struct list_head *cur_node=NULL;
  char kbuf[MAX_ITEMS]; //Buffer de uso interno
  char* dest=kbuf; //Puntero para recorrer el buffer
  if ((*off) > 0) /* The application can write in this entry just once !! */
    return 0;
  list_for_each(cur_node,&mylist){
	item = list_entry(cur_node, list_item_t, links);	
        dest += sprintf(dest, "%i\n", item->data); //Avanzo el puntero para no machacar el buffer
  }
  if (copy_to_user(buf, kbuf,dest-kbuf)) //Cuando se tiene todo el contenido de la entrada se pasa al espacio de usuario
	return -EINVAL;
  *off+=len; 
  return (dest-kbuf);
}

static const struct file_operations proc_entry_fops = {
    .read = modlist_read,
    .write = modlist_write,    
};

int init_modlist_module( void )
{
  int ret = 0;
  proc_entry = proc_create( "modlist", 0666, NULL, &proc_entry_fops);
  if (proc_entry == NULL) {
      ret = -ENOMEM;
      printk(KERN_INFO "Modlist: Can't create /proc entry\n");
  } else {
      printk(KERN_INFO "Modlist: Module loaded\n");
  }
  return ret;

}

void exit_modlist_module( void )
{
  remove_proc_entry("modlist", NULL);
  vaciar(&mylist);
  printk(KERN_INFO "Modlist: Module unloaded.\n");
}

module_init( init_modlist_module );
module_exit( exit_modlist_module );
