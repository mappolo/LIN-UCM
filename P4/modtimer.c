#include <linux/vmalloc.h>
#include <asm-generic/uaccess.h>
#include <asm-generic/errno.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>
#include "cbuffer.h"

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("modtimer Module");
MODULE_AUTHOR("Carlos Raspeño");

struct timer_list my_timer; /* Structure that describes the kernel timer */
struct work_struct my_work;
static struct proc_dir_entry *proc_entry;

struct semaphore mtx;
struct semaphore cola;

static char consumidor = false;
static char workqueue_event = false;

int timer_period = HZ;
int emergency_threshold = 80;
int max_random = 300;

DEFINE_SPINLOCK(cbuff);
LIST_HEAD(mylist);

#define MAX_ITEMS_CBUF	10

/* Nodos de la lista */
typedef struct {
int data;
struct list_head links;
}list_item_t;

cbuffer_t* cbuffer;	/* Buffer circular */

/*Operaciones sobre la lista*/
void limpiar_lista(struct list_head *list){
	list_item_t *aux, *elem = NULL;
	
	down(&mtx);
	list_for_each_entry_safe(elem, aux, list, links){
		list_del(&(elem->links));
		vfree(elem);	
	}
	up(&mtx);
}

void insertar_elemento(struct list_head *list, int data){
	list_item_t *item = vmalloc(sizeof(list_item_t));
	
	down(&mtx);
	item->data = data;
	list_add_tail(&(item->links),list);
	up(&mtx);
}

/* Timer */
static void fire_timer(unsigned long data)
{
	unsigned int aleatorio = (get_random_int() % max_random);
	unsigned long flags;

	printk(KERN_INFO "Numero generado: %d \n", aleatorio);
	spin_lock_irqsave(&cbuff,flags);
		insert_cbuffer_t(cbuffer, aleatorio);
	//insert_items_cbuffer_t(cbuffer, (char*) &aleatorio, sizeof(unsigned int));
	if (((size_cbuffer_t(cbuffer) * 100) / MAX_ITEMS_CBUF >= emergency_threshold)&& !workqueue_event){
		int cpu_actual=smp_processor_id();
		if (cpu_actual == 0){
			workqueue_event = true;
			schedule_work_on(1, &my_work);
		}
		else{
			workqueue_event = true;
			schedule_work_on(0, &my_work);
		}
	}
	spin_unlock_irqrestore(&cbuff,flags);

	    /* Re-activate the timer */
	mod_timer( &(my_timer), jiffies + timer_period); 
	
}


/* Workqueue*/
static void my_wq_function( struct work_struct *work )
{
	int elementos, item, i;
	unsigned long flags;
	
	spin_lock_irqsave(&cbuff, flags);
		elementos = (size_cbuffer_t(cbuffer));
		printk(KERN_INFO "%d elementos movidos a la lista\n", elementos);
	spin_unlock_irqrestore(&cbuff, flags);
	
	for(i = elementos ; i > 0; i--){
		spin_lock_irqsave(&cbuff, flags);
			//remove_items_cbuffer_t (cbuffer,(char*) &item, sizeof(unsigned int));
			item = remove_cbuffer_t (cbuffer);
		spin_unlock_irqrestore(&cbuff, flags);
		insertar_elemento(&mylist, item);
		
	}
	
	down(&mtx);
		
		up(&cola);
		
	up(&mtx);
	
	spin_lock_irqsave(&cbuff, flags);
		workqueue_event = false;
	spin_unlock_irqrestore(&cbuff, flags);

}


/* modconfig */
static ssize_t modconfig_read(struct file *filp, char __user *buf, size_t len, loff_t *off){
	
	char kbuf[100];
	char* dest=kbuf;
	if ((*off) > 0){ /* Tell the application that there is nothing left to read */
		return 0;
	}
	
	dest += sprintf(dest, "timer_period = %d\nemergency_threshold = %d\nmax_random = %d\n", timer_period, emergency_threshold, max_random);

	  /* Transfer data from the kernel to userspace */  
	  if (copy_to_user(buf, kbuf, dest-kbuf))
	    return -EINVAL;
	    
	  (*off)+=len;  /* Update the file pointer */
	return (dest-kbuf);

}

static ssize_t modconfig_write(struct file *filp, const char __user *buf, size_t len, loff_t *off){
	char line[25] = "";
	char command[20] = "";
	int num = 0;
	
	if ((*off) > 0){ /* Tell the application that there is nothing left to read */
		return 0;
	}
	
	if (copy_from_user(line, buf, len))
		return -EINVAL;
	line[len] = '\0';
	
	sscanf(line,"%19s %d", command, &num);
	
	if(strcmp((command),("timer_period")) == 0){
		timer_period = num;
	}else if(strcmp((command),("emergency_threshold")) == 0){
		if (num > 100){
			printk(KERN_INFO "emergency_threshold debe ser menor de 100");
		}
		else{
			emergency_threshold = num;
		}
	}else if(strcmp((command),("max_random")) == 0){
		max_random = num;
	}
	
	(*off)+=len;
	
	return len;
}

static struct file_operations modconfig_operations = {
    .read = modconfig_read,
    .write = modconfig_write,
};

/* modtimer */
static int modtimer_open(struct inode *inode, struct file *file){

    if (down_interruptible(&mtx))
		return -EINTR;
    if(consumidor == true) { //Si ya hay consimudor
        up(&mtx);
        return -EUSERS; //Demasiados usuarios
    }
    consumidor = true; //Añadir consumidor
    up(&mtx);
	
	/* Create timer */
    init_timer(&my_timer);
    /* Initialize field */
    my_timer.data=0;
    my_timer.function=fire_timer;
    my_timer.expires=jiffies + timer_period;  /* Activate it one second from now */
    /* Activate the timer for the first time */
    add_timer(&my_timer);
	
	try_module_get(THIS_MODULE);
    return 0;
}

static int modtimer_release(struct inode *inode, struct file *file){
	del_timer_sync(&my_timer); //Desactivar el temporizador 
	flush_scheduled_work(); //Esperar a que termine todo el trabajo planiﬁcado hasta el momento en la workqueue 
	cbuffer->size = 0; //Vaciar el buffer circular
	limpiar_lista(&mylist); //Vaciar la lista enlazada 
	consumidor = false; //Quitar consumidor

	module_put(THIS_MODULE);
    return 0;
}

static ssize_t modtimer_read(struct file *filp, char __user *buf, size_t len, loff_t *off){
	list_item_t *item = NULL;
	list_item_t *aux = NULL;
	
	
	
	char kbuf[100];
	char* dest=kbuf;

	/* "Adquiere" el mutex */
	if (down_interruptible(&mtx))
		return -EINTR;
		
	while(list_empty(&mylist)) {
		up(&mtx); /* "Libera" el mutex */
		/* Se bloquea en la cola */
		if (down_interruptible(&cola)){
			return -EINTR;
		}
		/* "Adquiere" el mutex */
		if (down_interruptible(&mtx))
		return -EINTR;
	}
	
	/* "Libera" el mutex */
	up(&mtx);
	
	
  	if (down_interruptible(&mtx))
		return -EINTR;
	 list_for_each_entry_safe(item, aux, &mylist, links){	
		dest += sprintf(dest, "%i\n", item->data);
         	list_del(&item->links);
         	vfree(item);
        
         if (copy_to_user(buf, kbuf, dest-kbuf))
    		return -EINVAL;

     }
	up(&mtx);
    return (dest-kbuf);
}

static struct file_operations modtimer_operations = {
    .read = modtimer_read,
    .open = modtimer_open,
    .release = modtimer_release
};

/* Funciones de inicializacion y descarga del modulo */
int init_module(void){
	/* Inicialización del buffer */  
	cbuffer = create_cbuffer_t(MAX_ITEMS_CBUF);

	if (!cbuffer) {
		return -ENOMEM;
	}
	
	/* Inicializacion a 1 del semáforo que permite acceso en exclusión mutua a la SC */
	sema_init(&mtx,1);
	
	/* Inicialización a 0 de los semáforos usados como colas de espera */
	sema_init(&cola,0);
	
	proc_entry = proc_create_data("modconfig",0666, NULL, &modconfig_operations, NULL);
	proc_entry = proc_create_data("modtimer",0666, NULL, &modtimer_operations, NULL);
	
	INIT_WORK(&my_work, my_wq_function);
	return 0;
}



void cleanup_module(void){
	remove_proc_entry("modconfig", NULL);
	remove_proc_entry("modtimer", NULL);
	
	destroy_cbuffer_t(cbuffer);
}

