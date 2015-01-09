#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>
#include <asm-generic/uaccess.h>
#include <asm-generic/errno.h>
#include <linux/semaphore.h>
#include <linux/proc_fs.h>
#include "cbuffer.h"
/*
 *  Lecturas o escritura de mas de BUF_LEN -> Error
 *  Al abrir un FIFO en lectura se BLOQUEA hasta habrir la escritura y viceversa
 *  El PRODUCTOR se BLOQUEA si no hay hueco
 *  El CONSUMIDOR se BLOQUEA si no tiene todo lo que pide
 *  Lectura a FIFO VACIO sin PRODUCTORES -> EOF 0
 *  Escritura a FIFO sin CONSUMIDOR -> Error
 */
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Creacion de una entrada /proc/fifoproc en el sistema de fichero /proc");
MODULE_AUTHOR("Miguel Angel Pérez");

int init_module(void);
void cleanup_module(void);
static int fifoproc_open(struct inode *, struct file *);
static int fifoproc_release(struct inode *, struct file *);
static ssize_t fifoproc_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t fifoproc_write(struct file * ,const char __user * ,size_t ,loff_t *);


#define MAX_ITEMS_CBUF 50



cbuffer_t* cbuffer;

int num_prod = 0;
int num_cons = 0;

int num_bloq_prod = 0;
int num_bloq_cons = 0;


DEFINE_SEMAPHORE(mutex);
struct semaphore sem_prod, sem_cons;




#define cond_wait(mutex, cond, count,InterruptHandler) \
    do { \
        count++; \
        up(mutex); \
          if (down_interruptible(cond)){ \
            down(mutex); \
            count--; \
            up(mutex); \
            InterruptHandler \
            return -EINTR; \
        } \
          if (down_interruptible(mutex)){ \
            InterruptHandler \
            return -EINTR; \
        }\
    } while(0)

#define cond_signal(cond) up(cond)

#define InterruptHandler

static struct proc_dir_entry *proc_entry;
 

static struct file_operations proc_entry_fops = {
    .read = fifoproc_read,
    .write = fifoproc_write,
    .open = fifoproc_open,
    .release = fifoproc_release,
};



int init_module(void)
{
    cbuffer = create_cbuffer_t(MAX_ITEMS_CBUF);

    if (!cbuffer) {
    return -ENOMEM;
  }

  

    sema_init(&sem_cons, 0);
    sema_init(&sem_prod, 0);
    sema_init(&mutex, 1);

    proc_entry = proc_create_data("fifoproc",0666, NULL, &proc_entry_fops, NULL);
    
      if (proc_entry == NULL) {
      destroy_cbuffer_t(cbuffer);
      printk(KERN_INFO "Fifoproc: No puedo crear la entrada en proc\n");
      return  -ENOMEM;
  }
      
  printk(KERN_INFO "Fifoproc: Cargado el Modulo.\n");

    return 0;
}

void cleanup_module(void)
{
    remove_proc_entry("fifoproc", NULL);
    destroy_cbuffer_t(cbuffer);
    printk(KERN_INFO "Fifoproc: Modulo descargado.\n");
}



static int fifoproc_open(struct inode *inode, struct file *file)
{
    char is_cons = file->f_mode & FMODE_READ;

    printk(KERN_INFO "Pipe abierto para %s con lectores %d, escritores %d", 
            (file->f_mode & FMODE_READ)? "lectura": "escritura",
            num_cons, num_prod);

    // INICIO SECCIÓN CRÍTICA >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
    if (down_interruptible(&mutex)) 
        return -EINTR;

    if (is_cons){ 
        // Eres consumidor
        num_cons++;
        while(num_bloq_prod){
            num_bloq_prod--;
            up(&sem_prod);
        }
        
        while(!num_prod)
            cond_wait(&mutex, &sem_cons, num_bloq_cons, InterruptHandler { 
                    down(&mutex);
                    num_cons--;
                    up(&mutex);
                }
            );

    }else{ 
        // Eres un productor.
        num_prod++;
        while(num_bloq_cons){
            num_bloq_cons--;
            up(&sem_cons);
	}
        
        while(!num_cons)
            cond_wait(&mutex, &sem_prod, num_bloq_prod, InterruptHandler { 
                    down(&mutex);
                    num_prod--;
                    up(&mutex);
                }
            );
    }

    up(&mutex);
        
    // FIN SECCIÓN CRÍTICA <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
    try_module_get(THIS_MODULE);

    return 0;
}


static int fifoproc_release(struct inode *inode, struct file *file)
{
    
    // INCIO SECCIÓN CRÍTICA >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
    if (down_interruptible(&mutex)) 
        return -EINTR;

    if (file->f_mode & FMODE_READ){
        num_cons--;
	if(num_cons == 0) // Por si hay productores durmiendo, levántalos. 
            while(num_bloq_prod){
	        num_bloq_prod--;
                cond_signal(&sem_prod);
	    }
    }else 
        num_prod--;


    if( !(num_prod || num_cons) )
        cbuffer->size = 0;

    up(&mutex);
    // FIN SECCIÓN CRÍTICA <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
    
    printk(KERN_INFO "Pipe de %s cerrado con lectores %d, escritores %d", 
            (file->f_mode & FMODE_READ)? "lectura": "escritura",
            num_cons, num_prod);

    module_put(THIS_MODULE);

    return 0;
}



static ssize_t fifoproc_read (struct file *filp, char __user *buff, size_t length, loff_t *offset)
{
    char *kbuff;
    printk(KERN_INFO "Quiero leer %lu bytes", length);
    printk(KERN_INFO "Escritores esperando %d", num_bloq_prod);

    if (length > MAX_ITEMS_CBUF){
        printk(KERN_INFO "[ERROR] Lectura demasiado grande");
        return -EINVAL;
    }

    if((kbuff = vmalloc(length)) == NULL){
        printk(KERN_INFO "[ERROR] no se ha podido alojar memoria dinámica");
        return -ENOMEM;
    }

    // INICIO SECCIÓN CRÍTICA >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
    if (down_interruptible(&mutex)){ 
        printk(KERN_INFO "[INT] Interrumpido al intentar acceder a la SC");
        vfree(kbuff);
        return -EINTR;
    }

    // Si el pipe esta vacio y no hay productores -> EOF
    if (num_prod == 0 && is_empty_cbuffer_t(cbuffer)){
        up(&mutex);
        printk(KERN_INFO "Pipe vacio sin productores");
        vfree(kbuff);
        return 0;
    }

    // El consumidor se bloquea si no tiene lo que pide
    while (size_cbuffer_t(cbuffer) < length){
        cond_wait(&mutex, &sem_cons, num_bloq_cons, InterruptHandler {
                    vfree(kbuff);
                });
    
        if (num_prod == 0 && is_empty_cbuffer_t(cbuffer)){
            up(&mutex);
	    printk(KERN_INFO "Pipe vacio sin productores");
    	    vfree(kbuff);
	    return 0;
        }
    }

    remove_items_cbuffer_t(cbuffer, kbuff, length); 
    
    // Broadcast a todos los productores, hay nuevos huecos
    while(num_bloq_prod){
        cond_signal(&sem_prod);
        num_bloq_prod--;
    }

    up(&mutex);
    // FIN SECCIÓN CRÍTICA <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

    length -= copy_to_user(buff, kbuff, length);
    
    printk(KERN_INFO "[TERMINADO] escritores esperando %d", num_bloq_prod);

    vfree(kbuff);
    return length;
}


static ssize_t fifoproc_write (struct file *filp, const char __user *buff, size_t length, loff_t *offset)
{

    char *kbuff;

    printk(KERN_INFO "Quiero escribir %lu bytes", length);

    if (length > MAX_ITEMS_CBUF){
        printk(KERN_INFO "[ERROR] Demasiado para escribir");
        return -EINVAL;
    }

    if((kbuff = vmalloc(length)) == NULL){
        printk(KERN_INFO "[ERROR] no se ha podido alojar memoria dinámica");
        return -ENOMEM;
    }

    length -= copy_from_user(kbuff, buff, length);

    // INICIO SECCIÓN CRÍTICA >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
    if (down_interruptible(&mutex)){
        printk(KERN_INFO "[INT] Interrumpido al intentar acceder a la SC");
        vfree(kbuff);
        return -EINTR;
    }

    // Si escribe sin consumidores -> Error
    if (num_cons == 0){
        up(&mutex);
        printk(KERN_INFO "[ERROR] Escritura sin consumidor");
        vfree(kbuff);
        return -EPIPE;
    }

    // El productor se bloquea si no hay espacio
    while (num_cons > 0 && nr_gaps_cbuffer_t(cbuffer) < length)
        cond_wait(&mutex, &sem_prod, num_bloq_prod,InterruptHandler {
                    vfree(kbuff);
                });

    // Comprobamos que al salir de un posible wait sigue habiendo consumidores.
    if (num_cons == 0){
        up(&mutex);
	printk(KERN_INFO "[ERROR] Escritura sin consumidor.");
	vfree(kbuff);
	return -EPIPE;
    }
    
    insert_items_cbuffer_t(cbuffer, kbuff, length); 
    
    // Broadcast a todos los consumidores, ya hay algo.
    while(num_bloq_cons){
        cond_signal(&sem_cons);
        num_bloq_cons--;
    }

    up(&mutex);
    // FIN SECCIÓN CRÍTICA <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
    
    printk(KERN_INFO "[TERMINADO] lectores esperando %d", num_bloq_cons);

    vfree(kbuff);
    return length;
}
