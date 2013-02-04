#ifndef DIRAPP_H_
#define DIRAPP_H_

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

//MAX CLIENTS AND SERVERS HERE
#define MAX_CLIENTS 3
#define MAX_SERVERS 2
//MAX CLIENTS AND SERVERS HERE

//maximum number of entries to send at a time, default 254
#define MAX_SEND 254

//buffers
#define COMMENT_BUF 255
#define DIFF_BUF 255
#define DIR_NAME_BUF 255

//depth used for ftw() call
#define DIR_DEPTH 20 

//typedefs and structs
typedef struct stat STAT;       //redef struct stat as STAT

typedef struct dirstat{         //represents a file/directory name and a stat
	char name[DIR_NAME_BUF];        //name of the file/directory
	STAT stat;                      //stat of file/directory
	int paired;			            //is this in the other list?
} dirstat;

typedef struct diffentry {      //represents a difference between two dirstats
	#define T_ADD '+'               //add char
	#define T_REM '-'               //remove char
	#define T_MOD '!'               //modified char
	char type;		                //one of +,-,!         
	char name[DIR_NAME_BUF];		//file name
	char comment[COMMENT_BUF];		//entry comment
} diffentry;

typedef struct dirdiff {        //represents differences between two directories
	dirstat* old;                   //old dirstat list (malloc'd), starts empty
	int old_i;                      //size of old dirstat list
	dirstat* new;                   //new/current dirstat list
	int new_i;                      //size of new/current dirstat list
	const char* name;               //name of root directory
	diffentry* entries;             //list of diffentries
	int entries_i;                  //number of diffentries
	int entries_size;               //size of entries list
	int defined; 			        //set to 1 after first write
	char* error;			        //if there is an error, this will be set
} dirdiff;

//client
void startClient();
void cleanup();

//server
void startServer(char* portnumberCh, char* dirname, char* periodCh);

//dirdiff
int diffDirectory(dirdiff* d);
void printDiffEntries(dirdiff* d);
void sprintDiffEntry(dirdiff* d, int i, char* buf);
void fprintDiffEntries(FILE* stream, dirdiff* d);
void writeDiffEntries(int fd, dirdiff* d);
void freeAll(dirdiff* d);

//common and utility functions
char* timeF(time_t time);
char* permOfFile(mode_t mode);
int checkPort(int portnum);
void die(const char* message);
void slog(const char* message);
void quit();


#endif
