#include "dirapp.h"

#define __USE_XOPEN_EXTENDED 1 //needed for ftw
#include <ftw.h>
#include <stdint.h>
#include <pwd.h>
#include <grp.h>

#include <unistd.h>
#define DEBUG 0


int dirmatch(dirstat* nd, dirstat* od);
void dircomp(dirdiff* d, dirstat* nd, dirstat* od);
void addDiffEntry(dirdiff* d, char type, const char* name, const char* comment);

dirstat* list;
int list_i;
int list_size;

static int store_list(const char *file_path, const struct stat *s,
             int fopts, struct FTW *file_desc){
	//used by nftw
	//walks the file tree and stores a list of stats
	if (file_desc->level == 0)//skip root
		return 0;
	//if (file_desc->level > 1) //only directly under the subdirectory is scanned
	//	return 0;
	if (list_size <= list_i){
		list_size = (list_size == 0) ? 5 : list_size*2;
		list = (dirstat*) realloc(list,sizeof(dirstat)*list_size);
	}
	dirstat* d = &(list[list_i]);
	d->paired = 0;
	strcpy(d->name,file_path);
	memcpy(&d->stat,s,sizeof(STAT));
	list_i++;
	return 0;
}

int diffDirectory(dirdiff* d){
#if DEBUG == 2
		printf("diff called\n",d->name);
#endif	
	if (d->defined != 1){
		//making a new diff
#if DEBUG
		printf("New diff for %s :\n",d->name);
#endif
		list_i = 0;
		list_size = 0;
		int check = nftw(d->name,(__nftw_func_t)store_list,DIR_DEPTH,FTW_PHYS);
	
		d->old = malloc(list_i*sizeof(dirstat));
		d->old_i = list_i;
		memcpy(d->old, list, list_i*sizeof(dirstat));
		//free(list);
		if (check != 0){
			return check;
		} else {
			d->defined = 1;
			return 0;
		}
	}

	list_i = 0;
	list_size = 0;
	int check = nftw(d->name,(__nftw_func_t)store_list,DIR_DEPTH,FTW_PHYS);
	
	if (check != 0){
		return check;
	}

	d->entries_i = 0;
	d->entries = malloc(5*sizeof(diffentry));
	d->entries_size = 5;

	char comment[COMMENT_BUF];

	for (int n = 0; n < list_i; n++){
		dirstat* nd = &list[n];
		for (int o =0; o < d->old_i; o++){
			dirstat* od = &d->old[o];
#if DEBUG == 2 
			printf("Comparing %s to %s\n",list[n].name,d->old[o].name);
#endif
			if (dirmatch(nd,od)){
				od->paired = nd->paired = 1;
				dircomp(d,nd,od);
			}	
		}
		if (!nd->paired){
			//new file
			strncpy(comment,"added on ",10);
			strncat(comment,timeF(nd->stat.st_mtime),COMMENT_BUF-9);
			addDiffEntry(d,T_ADD,nd->name,comment);
#if DEBUG == 2
			printf("new file %s\n", nd->name);
#endif
		} else {
			nd->paired = 0;		//for next run-thru
		}
	}
	for (int o =0; o < d->old_i; o++){
		dirstat* od = &d->old[o];
		if (!od->paired){
			//deleted file
			//char * time = asctime(localtime(&od->stat.st_mtime));
			//time[strlen(time)-1] = '\0';
			strncpy(comment,"deleted on ",12);
			strncat(comment,timeF(-1),COMMENT_BUF-12);
			addDiffEntry(d,T_REM,od->name,comment);
			od->paired = 0;
#if DEBUG == 2
			printf("deleted file %s\n", od->name);
#endif
		} 	
	}
	
	//update old list
	d->old = realloc(d->old,list_i*sizeof(dirstat));
	d->old_i = list_i;
	memcpy(d->old, list, list_i*sizeof(dirstat));
	//free(list);
	return 0;
}

int dirmatch(dirstat* nd, dirstat* od){
	//checks if two dirstats are equal (inode and device are equal)
	return (nd->stat.st_ino == od->stat.st_ino && nd->stat.st_dev == nd->stat.st_dev);
	
}

void dircomp(dirdiff* d, dirstat* nd, dirstat* od){
	//compares two dirstats and adds a diffentry to d if there is a change
	
	char comment[COMMENT_BUF];
	char tmp[COMMENT_BUF];
	int changed = 0;

    memset(comment,0,COMMENT_BUF);
    memset(tmp,0,COMMENT_BUF);

	//access time
	if (nd->stat.st_atime != od->stat.st_atime){
		snprintf(tmp,COMMENT_BUF,"accessed on %s, ",timeF(nd->stat.st_atime));
		strncat(comment,tmp,COMMENT_BUF-strlen(comment));	
		changed = 1;
	}

	//size
	if (nd->stat.st_size != od->stat.st_size){
		snprintf(tmp,COMMENT_BUF,"changed size from %dB to %dB, ",
				(int)od->stat.st_size,(int)nd->stat.st_size);
		strncat(comment,tmp,COMMENT_BUF-strlen(comment));
		changed = 1;
	}

	//ownership, group
	if (nd->stat.st_uid != od->stat.st_uid){
		snprintf(tmp,COMMENT_BUF,"owner changed to %s, ",
				getpwuid(nd->stat.st_uid)->pw_name);
		strncat(comment,tmp,COMMENT_BUF-strlen(comment));	
		changed = 1;
	}
	if (nd->stat.st_gid != od->stat.st_gid){
		snprintf(tmp,COMMENT_BUF,"owner's group changed to %s, ",
				getgrgid(nd->stat.st_gid)->gr_name);
		strncat(comment,tmp,COMMENT_BUF-strlen(comment));	
		changed = 1;
	}

	//access mode
	if (nd->stat.st_mode != od->stat.st_mode){
		snprintf(tmp,COMMENT_BUF,"access mode changed to %9.9s, ",
				permOfFile(nd->stat.st_mode));
		strncat(comment,tmp,COMMENT_BUF-strlen(comment));	
		changed = 1;
	}

	if (changed){
		comment[strlen(comment)-2]='\0'; //remove trailing comma and space
		addDiffEntry(d,T_MOD,nd->name, comment);
	}
}

void addDiffEntry(dirdiff* d, char type, const char* name, const char* comment){
	//adds a diffentry to d
	if (d->entries_size <= d->entries_i){
		d->entries_size *= 2;
		d->entries = (diffentry*) realloc(d->entries,sizeof(diffentry)*d->entries_size);
	}
	diffentry* de = &d->entries[d->entries_i];
	de->type = type;
	strncpy(de->name,name,DIR_NAME_BUF);
	strncpy(de->comment,comment,COMMENT_BUF);
	d->entries_i++;
}

void printDiffEntries(dirdiff* d){
	fprintDiffEntries(stdout, d);
}

void sprintDiffEntry(dirdiff* d, int i, char* buf){
	diffentry* de = &d->entries[i];
	snprintf(buf,DIFF_BUF,"%c %s %s", de->type,de->name,de->comment);

}

void fprintDiffEntries(FILE* stream, dirdiff* d){
    char buf[DIFF_BUF];
	for (int i = 0; i < d->entries_i; i++){
		sprintDiffEntry(d,i,buf);
		fprintf(stream,"%s\n",buf);
	}
}

void writeDiffEntries(int fd, dirdiff* d){
	//write all diff entries to a file descriptor
    char buf[DIFF_BUF];
    unsigned char chunk;
    unsigned char size;
    int total = d->entries_i;
#if DEBUG > 1
    if (total > 0){
        printf("START %d\n",total);
    }
#endif
    for (int n = 0; n < total;n+=MAX_SEND){
        chunk = (unsigned char) MAX_SEND;
        if (total - n < MAX_SEND){
            chunk = total -n;
        }
#if DEBUG > 1
        printf("SIZE: %d\n",chunk);
#endif
        write(fd,&chunk,1);

        for (int i = 0; i < chunk; i++){
            sprintDiffEntry(d,n+i,buf);
            size = (char) strlen(buf);
            write(fd,&size,1);
            write(fd,buf,size);
            
#if DEBUG > 1
            printf("ENTRY %d (%d) %s\n",n+i, size, buf);
#endif      
        }
    }
#if DEBUG > 1
    if (total > 0){
        printf("END\n");
    }
#endif
}


void freeAll(dirdiff* d){
	//free memory once
	if (d->defined){
		free(list);
		free(d->old);
		free(d->entries);
        d->defined = 0;
	}
}

