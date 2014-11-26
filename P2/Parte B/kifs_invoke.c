#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/syscall.h>
#ifdef __i386__
#define SYS_kifs 	353
#else
#define SYS_kifs 	316
#endif

#define USER_BUFFER_MAX 1000
#define N 2000

enum
{
	KIFS_READ_OP=0,
	KIFS_WRITE_OP=1
};

/* Print usage of the command */
void usage(int argc,char* argv[]){
	printf("Usage: %s [-r|-w] <entry_name> [value]\n",argv[0]);
}

/* Wrapper to invoke sys_kifs() via syscall() */
int kifs(const char* entry_name,unsigned int op_mode, char* user_buffer,unsigned int maxchars)
{
	return syscall(SYS_kifs,entry_name,op_mode,user_buffer,maxchars); 
}

int main(int argc, char *argv[])
{
        int ret;
 	char buf[USER_BUFFER_MAX+1]="";
	char optc;
	int kifs_op=-1;
	char *entryname;
	int nchars=0;
 
	/* Loop to process options */
        while ((optc = getopt(argc, argv, "+hrw")) != -1) {
                switch (optc) {
                case 'r':
                      	kifs_op=KIFS_READ_OP;  
			break;
                case 'w':
                      	kifs_op=KIFS_WRITE_OP;  
			break;
                case 'h':
                	usage(argc,argv);	
			return 0; 
			break;
                default:
                        fprintf(stderr, "Opcion incorrecta: %c\n", optc);
                        exit(1);
                }
        }
       
	/* Make sure at least the entry name is provided. 
	 * Otherwise, exit with an error code. */
	if (argv[optind]==NULL || kifs_op==-1)
        {
               	usage(argc,argv); 
		return 1;
        }

	entryname=argv[optind];

	if (kifs_op==KIFS_WRITE_OP)
	{
		/* If the write operation was requested, 
			a string must be provided as well
			in the command line
		*/
		if (argv[optind+1]==NULL)
		{	
			usage(argc,argv);
			return 2;
		}

		nchars=strlen(argv[optind+1]);
		ret=kifs(entryname,KIFS_WRITE_OP,argv[optind+1],nchars);

		/* The return value should be the number of characters read/written.
		    If this is not the case, report an error */
		if (ret<0)
		{
			fprintf(stderr,"Error when writing to entry: '%s' (return value: %d)\n",entryname,ret);
			perror("Error message");	
			return 3;
		}	
	}else{

		nchars=USER_BUFFER_MAX;
		ret=kifs(entryname,KIFS_READ_OP,buf,nchars);
		
		/* The return value should be the number of characters read/written.
		    If this is not the case, report an error */
		if (ret<0)
		{
			fprintf(stderr,"Error when reading entry: '%s' (return value: %d)\n",entryname,ret);
			perror("Error message");	
			return 3;
		}	
		printf("*** READING %s ENTRY **\n",entryname);
		buf[ret]='\0';	
		printf("%s\n",buf);
		printf("*****\n");	
	}
	
	return 0;
}
