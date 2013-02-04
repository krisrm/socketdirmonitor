#define _POSIX_SOURCE

#include "dirapp.h"
#include "host.h"

#include <strings.h>

#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>

#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/prctl.h>

#define E_NONE 0
#define E_SHUP 1
//could add more errors?

void daemonize();
void clientConnect(int sock,char* dirname);
void writeUpdates();
void clientDisconnect(host* h, char* message);
void clientAllDisconnect(char* message);
void terminate();
void ssRestart();
void logseg();

void openDirmonitor();
void checkDirectory();
void stopChecking();

int period;
int portnumber;

dirdiff diff;
pid_t dirmonitor;
int fds[2];

host hosts[MAX_CLIENTS];
int sockInit;
int sockRW;
socklen_t clilen;
struct sockaddr_in serv_addr, cli_addr;
fd_set inputs;
int fdmax;
int error;

void startServer(char* portnumberCh, char* dirname, char* periodCh){
    portnumber = atoi(portnumberCh);
    period = atoi(periodCh);

    //create log file
    system("rm -f log.txt");
    system("touch log.txt");
    slog("Starting directory monitor server\n");
    if (period <= 0 || period > 255){
        die("Please specify a nonzero period, maximum 255\n");
    }
    if (checkPort(portnumber) == 0){
        die("Please specify a port number between 1024 and 65535\n");
    }
	if (strlen(dirname) > 255){
		die("Directory name is too long. Must be under 255 characters.\n");
	}
	STAT testdir;
	if (stat(dirname,&testdir) != 0){
		die("Directory is gone or not accessible.\n");
	}

    diff.name=dirname;

	printf("Starting server monitor on port %d for directory %s every %d second(s)\n",portnumber,dirname,period);

    //open the directory monitor first (avoids issues with chdir)
    openDirmonitor();

    //then fork the server as a daemon
    daemonize();

    //clear out the hosts list before starting
    clearHosts(hosts,MAX_CLIENTS);

    //create and bind the listening socket
    sockInit = socket(AF_INET, SOCK_STREAM, 0);

    if (sockInit < 0)
        slog("ERROR opening socket\n");
    bzero((char *) &serv_addr, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portnumber);

    //reuse this socket
    int yes = 1;
    setsockopt(sockInit, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    if (bind(sockInit, (struct sockaddr *) &serv_addr,
                sizeof(serv_addr)) < 0)
        slog("ERROR binding socket\n");
    listen(sockInit,5);
    clilen = sizeof(cli_addr);
    
    fd_set read_fds;
	struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100;

    FD_ZERO(&inputs);
    FD_ZERO(&read_fds);
    
    FD_SET(sockInit,&inputs);
    fdmax = sockInit;

    FD_SET(fds[0],&inputs);
    if (fds[0] > fdmax) fdmax = fds[0];

    error = E_NONE;

    while (1){
        read_fds = inputs;
        if(select(fdmax+1, &read_fds, NULL, NULL, &tv) == -1){
            slog("Select failed on server\n");
			continue;
		} 

        //check for an error
        //doing this here ensures that the signal won't interrupt an update
        //send the error the next time we go thru an update
        if (error == E_SHUP){
            clientAllDisconnect("SIGHUP received by server, please reconnect");
            error = E_NONE;
        }

        tv.tv_usec = 100; //reset select timeout
        for(int i = 0; i <= fdmax; i++){
			if(FD_ISSET(i, &read_fds)){
				if (i == sockInit){
                    slog("Connecting to new client\n");
                    clientConnect(i,dirname);
                } else if (i == fds[0]){
                    slog("Updating client\n");
                    //updates from the dirmonitor process. Write them to connected clients.
                    writeUpdates();
                } else {
                    int n = selectHostFD(hosts,i,MAX_CLIENTS);
                    host* h = &hosts[n];
                    unsigned char r;
                    if((recv(i, &r, 1, 0)) <= 0){
                        slog("Client disconnected unexpectedly\n");
                        
                        hosts[n].status = H_DISC;
                        //start listening to connections again (add sockInit back)
                        FD_SET(sockInit,&inputs);
                        close(i);
                        FD_CLR(i, &inputs);
                    }
                    else{
                        slog("Client disconnected safely\n");
                        if (r == 0xDE && h->status == H_CONN){
                            h->status = H_DCING;
                        } else if (r == 0xAD && h->status == H_DCING){
                            clientDisconnect(h,"Goodbye");
                        } else {
                            //protocol error
                            h->status = H_DISC;
                        }

                        if (h->status == H_DISC){
                            close(h->sockfd);
                            FD_CLR(h->sockfd,&inputs);
                        }
                        
                    }
                }
            }

        }

        
    }

}

void daemonize(){
   	//turn the server into a daemon	
    pid_t pid, sid;

	pid = fork();
	if (pid < 0) {
		exit(EXIT_FAILURE);
	}
	if (pid > 0) {
		printf("Server successfully daemonized. Kill process id %d and %d when done.\n",pid,dirmonitor);
		exit(EXIT_SUCCESS);
	}

	umask(0);       

	sid = setsid();
	if (sid < 0) {
		exit(EXIT_FAILURE);
	}

	if ((chdir("/")) < 0) {
		exit(EXIT_FAILURE);
	}

    signal(SIGSEGV, &logseg);

	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
}

void openDirmonitor(){
	//opens the directory monitor process
    if (pipe(fds) == -1){
        slog("Could not open pipe\n");
    }
    dirmonitor = fork();
    
    if (dirmonitor < 0){
        slog("Could not fork directory monitor\n");
    } else if (dirmonitor == 0){
        //child
        close(fds[0]);                  //close read
		signal(SIGINT, &stopChecking);  //handle interrupt (Ctrl-C) signals
        signal(SIGTERM, &stopChecking); //handle term signals from the parent
		signal(SIGHUP, &stopChecking);	//handle SIGHUP
        checkDirectory();               //start checking directories
        
        return;
    }

    //parent
    close(fds[1]);                  	//close write
    signal(SIGINT,&terminate);
    signal(SIGHUP,&ssRestart);
}

void checkDirectory(){
    int worked = diffDirectory(&diff);
    if (worked != 0){
        unsigned char w = 255;
        write(fds[1],&w,1);
        stopChecking();
    }
    writeDiffEntries(fds[1],&diff);
    sleep(period);
    checkDirectory();
}
void stopChecking(){
    close(fds[0]);
    freeAll(&diff);
    exit(0);
}


void clientConnect(int i,char* dirname){
    char buf[DIFF_BUF];
    int n = selectDiscHost(hosts,MAX_CLIENTS);
    if (n == -1){
        //hosts are all connected, don't accept...
        FD_CLR(i, &inputs);
        return;
    }
    sockRW = accept(sockInit, (struct sockaddr *) &cli_addr, &clilen);
    FD_SET(sockRW, &inputs);
    if(sockRW > fdmax){ 
        fdmax = sockRW;
    }
    hosts[n].status = H_INIT;
    hosts[n].sockfd = sockRW;

    //send initial connection protocol info
    char w = 0xFE;
    write(sockRW,&w,1);
    w = 0xED;
    write(sockRW,&w,1);
    snprintf(buf,DIFF_BUF,"%c%s",(char)strlen(dirname),dirname);
    write(sockRW,&buf,buf[0]+1);
    w = (char) period;
    write(sockRW,&w,1);
    hosts[n].status = H_CONN;
}


void writeUpdates(){
    //write updates to all clients
    char buf[DIFF_BUF];
    int expected = 1;
    int total = 0;
    unsigned char w;
    read(fds[0], &w, expected); //read from dirmonitor
    total = (int) w; //total number of lines

    //error received from dirmonitor
    if (total == 255){
        clientAllDisconnect("Directory removed or inaccessible");
        terminate();
        return;
    }

    writeToClients(hosts,MAX_CLIENTS,&w,1);
    for(int a =0; a < total; a++){
        w = 1;
        read(fds[0], &w,w);
        writeToClients(hosts,MAX_CLIENTS,&w,1);
        memset(buf,0,sizeof(buf));
        read(fds[0], &buf, w);
        writeToClients(hosts,MAX_CLIENTS,buf,w);
    }
}

void clientDisconnect(host* h, char* message){
    //disconnect a client with a message
    char w = 255;
    write(h->sockfd,&w,1);
    w = (char) strlen(message);
    write(h->sockfd,&w,1);
    write(h->sockfd,message,w);
    h->status = H_DISC;
}

void clientAllDisconnect(char* message){
    //disconnect all clients with a message (error)
    for (int n = 0; n < MAX_CLIENTS; n++){
        if (hosts[n].status == H_CONN)
            clientDisconnect(&hosts[n],message);
    }
}

void ssRestart(){
    error= E_SHUP;
    signal(SIGHUP,&ssRestart);
}


void terminate(){
    //terminate gracefully
    clientAllDisconnect("Server interrupted");
    //kill dirmonitor
    kill(dirmonitor,SIGTERM);

    //close all inputs
    for(int i = 0; i <= fdmax; i++){
        if(FD_ISSET(i, &inputs)){
            close(i);
        }
    }
    exit(0);
}
void logseg(){
    //log a segfault on the server
    slog("Segmentation fault\n");
    exit(1);
}
