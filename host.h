#ifndef HOST_H_
#define HOST_H_

#define HOST_NAME_BUF 255
#define DIR_NAME_BUF 255
#define MAX_LINES 254

//typedefs and structs
typedef struct host {           //represents a host
	#define H_DISC 0                //disconnected status
	#define H_INIT 1                //initializing status
	#define H_CONN 2                //connected status
	#define H_DCING 3               //disconnecting status
	int status;                     //host's status code (H_xxxx)
	int pstep;                      //current step in protocol (defined by status)
	int expected;                   //number of bytes we're expecting from the host
    char lines[MAX_LINES][DIR_NAME_BUF];//buffer for lines (print when we're done)
    int cur_line;                   //current line number we're buffering
    int lines_i;                    //number of update lines we're receiving from the host
	char hostname[HOST_NAME_BUF];   //the hostname given on the command line
	int portnum;                    //port number given on the command line
	char dirname[DIR_NAME_BUF];     //directory given on the command line (recv'd from server)
	char period;                    //monitoring period given on the command line (recv'd from server)
	int sockfd;                     //socket file descriptor to communicate thru

} host;

void clearHosts(host* hosts, int size);
int selectHostFD(host* hosts, int i, int size);
int selectDiscHost(host* hosts, int size);
int activeHosts(host* hosts, int size);
void terminateClientConnection(host* h);
void initHost(host* h, char* hostname, int sockfd, int portno);
void writeToClients(host* hosts, int max, char* buf, int size);
#endif
