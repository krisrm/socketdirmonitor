#include "host.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void clearHosts(host* hosts, int size){
	//clear a list of hosts (disconnected)
	memset(hosts,0,sizeof(host)*size);
	for (int i = 0; i < size; i++)
		hosts[i].status = H_DISC;
}

int selectHostFD(host* hosts, int i, int size){
    //select a host with the socket file descriptor equal to i
    int n;
    for(n = 0; n < size; n++){
        if (hosts[n].sockfd == i) break;
    }

    if (n >= size || hosts[n].sockfd != i) 
        return -1;
    return n;
}
int selectDiscHost(host* hosts, int size){
	//select the first disconnected host in a list
	//-1 if there is no disconnected host
    int n;
    for (n = 0; n < size; n++){
		if (hosts[n].status == H_DISC)
			break;
	}
    if (n >= size || hosts[n].status != H_DISC)
        return -1;
    return n;
}
int activeHosts(host* hosts, int size){
	//number of connected or initializing hosts in a list
	int r = 0;
	for (int i = 0; i < size; i++){
		if (hosts[i].status == H_INIT || hosts[i].status == H_CONN)
			r++;
	}
	return r;
}

void terminateClientConnection(host* h){
	//terminate connection with a host (according to protocol)
	h->status = H_DCING;
	h->pstep = 0;
	char w = 0xDE;
	write(h->sockfd,&w,1);
	w = 0xAD;
	write(h->sockfd,&w,1);
	h->expected = 1;
}

void initHost(host* h, char* hostname, int sockfd, int portno){
	//initialize a host's members
    memset(h,0,sizeof(host));
	h->status = H_INIT;
	strncpy(h->hostname,hostname,strlen(hostname));
	h->portnum = portno;
	h->sockfd = sockfd;
	h->pstep = 0;
	h->expected = 1;
}

void writeToClients(host* hosts, int max, char* buf, int size){
	//write a message to all hosts
    for (int n = 0; n < max; n++){
        if (hosts[n].status == H_CONN){
            write(hosts[n].sockfd,buf,size);
        }
    }

}
