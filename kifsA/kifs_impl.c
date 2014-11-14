#include <linux/kernel.h>
#include <linux/module.h>
#include <asm/uaccess.h>
#include <asm-generic/errno.h>
#include <linux/list.h>
#include <linux/kifs.h>
+
#define __user
#define KIFS_DEBUG
#ifdef KIFS_DEBUG
+#define DBG(format, arg...) do { \
		printk(KERN_DEBUG "%s: " format "\n" , __func__ , ## arg); \
	} while (0)
#else
#define DBG(format, arg...) /* */
#endif

#define MAX_KIFS_ENTRY 10
kifs_entry_t entry_buff[MAX_KIFS_ENTRY];

#define CLIP_SIZE 50
char clipboard[CLIP_SIZE];

LIST_HEAD(entry_list);
LIST_HEAD(free_list);

int list_kifs (char __user *buffer, unsigned int maxchars, void *data){
	kifs_entry_t *elem = NULL;
	int used = 0;
	int tot_used = 0;
	char kbuff[MAX_KIFS_ENTRY_NAME_SIZE+1] = "";

	DBG("[KIFS] evento listado");

	list_for_each_entry(elem, &entry_list, links){
        DBG("\t entry: %."MAX_KIFS_ENTRY_NAME_SIZE_STR"s\n", elem->entryname);
		used = sprintf(kbuff, "%."MAX_KIFS_ENTRY_NAME_SIZE_STR"s\n", elem->entryname);
		tot_used += used;
		if(tot_used > maxchars){
		    DBG("[KIFS] espacio insuficiente en buffer de usuario.");
            return -ENOSPC;
        }

		if(copy_to_user(buffer, kbuff, used)){
            DBG("[KIFS] Error al copiar a buffer de usuario");
			return -EFAULT;
        }

		buffer += used;
	}

	copy_to_user(buffer-1, '\0', 1);

	return tot_used;
}

int clipboard_kifs_read (char __user *buffer, unsigned int maxchars, void *data){
	int used = 0;
	if((used = strlen(clipboard)) <= maxchars){
		used = used - copy_to_user(buffer, clipboard, used);
	}else
		return -ENOSPC;
	return used;
}

int clipboard_kifs_write (const char __user *buffer, unsigned int maxchars, void *data){
	int used = 0;
	if(maxchars < CLIP_SIZE){
		used = maxchars - copy_from_user(clipboard, buffer, maxchars);
		clipboard[used] = '\0';
	}else
		return -ENOSPC;

	return used;
}


void init_kifs_entry_set(void){
	int i;
	memset(entry_buff,0,MAX_KIFS_ENTRY*sizeof(kifs_entry_t));
	memset(clipboard, 0, CLIP_SIZE);

	for(i=0; i<MAX_KIFS_ENTRY; i++){
		list_add(&(entry_buff[i].links),&free_list);
	}

	create_kifs_entry("list",list_kifs,NULL,NULL);
	create_kifs_entry("clipboard",clipboard_kifs_read,clipboard_kifs_write,NULL);
}


asmlinkage long sys_kifs(const char __user *entry_name, unsigned int op_mode, char __user *buffer, unsigned int maxchars){
	kifs_entry_t *elem = NULL;
	char kbuff[MAX_KIFS_ENTRY_NAME_SIZE];
	memset(kbuff,0,MAX_KIFS_ENTRY_NAME_SIZE);

	strncpy_from_user(kbuff, entry_name, MAX_KIFS_ENTRY_NAME_SIZE);

	list_for_each_entry(elem, &entry_list, links){
        if(strncmp(elem->entryname, kbuff, MAX_KIFS_ENTRY_NAME_SIZE) == 0){
            switch(op_mode){
                case KIFS_READ_OP:
                    if(elem->read_kifs == NULL)
                        return -EPERM;

                    return elem->read_kifs(buffer,maxchars,elem->data);
                case KIFS_WRITE_OP:
                    if(elem->write_kifs == NULL)
                        return -EPERM;

                    return elem->write_kifs(buffer,maxchars,elem->data);
                default:
                    return -EINVAL;
            }
        }

	}
	return -EINVAL;
}



kifs_entry_t* create_kifs_entry(const char *entryname, read_kifs_t *read_kifs, write_kifs_t *write_kifs, void* data){
	kifs_entry_t *ret;

	if(list_empty(&free_list)){
		DBG("[KIFS] Nos hemos quedado sin entradas :(");
		return NULL;
	}

		list_for_each_entry(ret, &entry_list, links){
        if(strncmp(ret->entryname, entryname, MAX_KIFS_ENTRY_NAME_SIZE) == 0)
            return NULL; //La entrada ya existe
    }

    ret = NULL;

	list_move(free_list.next,&entry_list);

	ret = list_first_entry(&entry_list,kifs_entry_t,links);

	ret->read_kifs = read_kifs;
	ret->write_kifs = write_kifs;
   ret->data = data;
	memset(ret->entryname,0,MAX_KIFS_ENTRY_NAME_SIZE);

	strncpy(ret->entryname, entryname, MAX_KIFS_ENTRY_NAME_SIZE);

	return ret;
}
EXPORT_SYMBOL(create_kifs_entry);

int remove_kifs_entry(const char* entry_name){
	kifs_entry_t *ptr;

	list_for_each_entry(ptr,&entry_list,links){
		if(strncmp(entry_name,ptr->entryname, MAX_KIFS_ENTRY_NAME_SIZE) == 0){
			list_del(&(ptr->links));
			list_add(&(ptr->links),&free_list);
			return 0;
		}
	}

	return -EINVAL;
}

EXPORT_SYMBOL(remove_kifs_entry);
