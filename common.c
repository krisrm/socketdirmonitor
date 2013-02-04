#include "dirapp.h"
#include <time.h>
#include <sys/stat.h>

#define S_IREAD __S_IREAD 
#define S_IWRITE __S_IWRITE 
#define S_IEXEC __S_IEXEC 
#define S_ISVTX __S_ISVTX 

char* timeF(time_t t){
	//return nicely formatted time string
	char* str;
	if (t == -1){
		t = time(NULL);
	}
	str = asctime(localtime(&t));
	str[strlen(str)-1] = '\0'; //chop newline
	return str;
}

char* permOfFile(mode_t mode){
	//return nicely formatted file permissions
	//http://www.cs.odu.edu/~cs476/unix/codelectures/lstat.c
	int i;
	char *p;
	static char perms[10];

	p = perms;
	strcpy(perms, "---------");

	for (i=0; i < 3; i++) {
		if (mode & (S_IREAD >> i*3))
			*p = 'r';
		p++;

		if (mode & (S_IWRITE >> i*3))
			*p = 'w';
		p++;

		if (mode & (S_IEXEC >> i*3))
			*p = 'x';
		p++;
	}

    if ((mode & S_ISUID) != 0)
        perms[2] = 's';

    if ((mode & S_ISGID) != 0)
        perms[5] = 's';

    if ((mode & S_ISVTX) != 0)
        perms[8] = 't';

    return(perms);

}


void die(const char* message){
    //print an error message, cleanup, and exit
    fprintf(stderr, "%s",message);
    cleanup();
    exit(1);
}

void slog(const char* message){
	//used to debug the server
	/*
	FILE *log; 
    log = fopen("log.txt","a+");
    fprintf(log,"%s",message); 
    fclose(log);
	*/
}

void quit(){
	//client exits peacefully
	cleanup();
	exit(0);
}


int checkPort(int portnum){
	//returns 0 if the port is invalid, 1 otherwise
	return (portnum > 1024 && portnum <= 65535);
}

