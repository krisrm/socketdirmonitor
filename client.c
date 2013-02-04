#include "dirapp.h"
#include "host.h"

#include <strings.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <stdarg.h>
#include <signal.h>

#define xprintf(...) my_rl_printf(__VA_ARGS__)

//function prototypes
void my_rl_printf(char *fmt, ...);
void addHost(char* hostname, int portnumber);
void removeHost(char* hostname, int portnumber);
void listHosts();
void execCommand(char* command);
void handleMessage(char* buf, host* h);
void protocolError(host* h);
void terminateAll();

const char* help = "Commands:\n\t(a)dd <hostname> <portnumber>\n\t(r)emove <hostname> <portnumber>\n\t(l)ist\n\t(q)uit/exit";

host hosts[MAX_SERVERS];
fd_set inputs;
int fdmax;
int reprint;


void startClient(){
    char buf[DIFF_BUF];
    fd_set read_fds;
	struct timeval tv;

	//select timeout
    tv.tv_sec = 0;
    tv.tv_usec = 100;

	//clear the FDsets
    FD_ZERO(&inputs);
    FD_ZERO(&read_fds);
    
	//clear the hosts
    clearHosts(hosts,MAX_SERVERS);

	//add STDIN_FILENO to the input fdset
	FD_SET(STDIN_FILENO,&inputs); 
    fdmax = STDIN_FILENO;

	//set signal handlers
	signal(SIGHUP, &terminateAll);

	//welcome!
	printf("Welcome to Directory Monitor!\n");
    printf("%s\n",help);

	//start the readline callback handler	
	rl_callback_handler_install("> ", &execCommand);
	//use history library
	using_history();



    while (1){
		//copy inputs
        read_fds = inputs;
		//do select
        if(select(fdmax+1, &read_fds, NULL, NULL, &tv) == -1){
            continue;
		}
        tv.tv_usec = 100;//reset select timer
		
		for(int i = 0; i <= fdmax; i++){
			if(FD_ISSET(i, &read_fds)){
				if (i == STDIN_FILENO){
					//input on STDIN
					//reprompt off
					reprint = 0;
					//read a character
					rl_callback_read_char();
					reprint = 1;
                } else {
					//input on a socket
					//clear out the buffer
					memset(buf,0,sizeof(buf));
                    int n = selectHostFD(hosts,i,MAX_SERVERS);
                    if((recv(i, buf, hosts[n].expected, 0)) <= 0){
						//socket has disconnected
                        xprintf("%s on %d disconnected unexpectedly\n", hosts[n].hostname,hosts[n].portnum);
                        hosts[n].status = H_DISC;
                        close(i);
                        FD_CLR(i, &inputs);
                    } else {
						//handle the receieved message
						handleMessage(buf,&hosts[n]);
                    }
                }
            }

        }

    }
}

void handleMessage(char* buf, host* h){
	//handle a message from the server

    //the first byte of the buffer, used often
	unsigned char b1 = (unsigned char) buf[0]; 

	if (h->status == H_INIT){
        //we're initializing the connection
		switch (h->pstep){
			case 0:
                //first char received is 0xFE
				if (b1 != 0xFE){
					protocolError(h);
				} else {
					h->pstep++;
				}
				break;
			case 1:
                //next char received is 0xED
				if (b1 != 0xED){
					protocolError(h);
				} else {
					h->pstep++;
				}
				break;
			case 2:
                //get the number of chars in the dirname
				h->expected = (int)b1;
				h->pstep++;
				break;
			case 3:
                //copy the dirname from the buffer
				strncpy(h->dirname,buf,h->expected);
				h->expected = 1;
				h->pstep++;
				break;
			case 4:
                //get the period from the first char; we're connected successfully
				h->period = b1;
				xprintf("Now monitoring %s every %d seconds.\n",h->dirname,h->period);
				h->status = H_CONN;
				h->pstep = 0;
				h->expected = 1;
				break;
		}
	} else if (h->status == H_CONN){
        //we're connected
		switch (h->pstep){
			case 0:
				//char should say how many lines to expect
				//or the error code (255)
				h->lines_i = b1;
				h->expected = 1;
                
				if (b1 == 255){
                    //error, jump to H_DCING pstep 1
					h->pstep = 1;
                    h->status = H_DCING;
                } else if (b1 > 0){
					//lines have been received
                    h->cur_line = 0;
					h->pstep++;
					//clear lines buffer
					memset(h->lines, 0, MAX_LINES*DIR_NAME_BUF);
                }
				
				break;
			case 1:
				//length of line
				h->expected = (int)b1;
				h->pstep++;
				break;
			case 2:
				//receive a line
				h->lines_i--;
				sprintf(h->lines[h->cur_line],"%s",buf);
                h->cur_line++;
				h->expected = 1;
				if (h->lines_i == 0){
					//done receiving
					//print change header
					xprintf("%d Change(s) on %s:%d (%s):\n", h->cur_line, h->hostname, h->portnum, h->dirname);

					//print lines
                    for (int i =0; i < h->cur_line;i++){
                        xprintf("\t%s\n",h->lines[i]);
                    }
					h->pstep = 0;
                } else 
					h->pstep = 1; //receive the next line
				break;
		}
	} else if (h->status == H_DCING){
        //we're in the process of disconnecting
		switch (h->pstep){
			case 0:
				if (b1 == 255){
					h->expected = 1;
					h->pstep++;
				}
                break;
			case 1:
				//length of Goodbye/error message
				h->expected = (int)b1;
				h->pstep++;
				break;
			case 2:
				//we've received the Goodbye/error message
				h->status = H_DISC;
				FD_CLR(h->sockfd,&inputs);
				close(h->sockfd);
				if (strncmp("Goodbye",buf,7) == 0){
					//graceful disconnect
					xprintf("Disconnected from %s on %d\n",h->hostname,h->portnum);
				}else{
					//error
					xprintf("Server Error (%s): %s\n",h->hostname,buf);
				}
				break;
		}
	}
}

void protocolError(host* h){
	//protocol error while initializing; display an error and close the socket
	fprintf(stderr,"Protocol error: 0xFE and 0xED not sent. Aborting.\n");
	h->status = H_DISC;
	close(h->sockfd);
	FD_CLR(h->sockfd,&inputs);
}



void terminateAll(){
	//kill all connections with servers
	for (int n = 0; n < MAX_SERVERS; n++){
		if (hosts[n].status != H_DISC){
			terminateClientConnection(&hosts[n]);
		}
	}
}

void execCommand(char* command){
	//parse and execute a command

	add_history(command); //add command to history
	if (strncmp(command,"q",1) == 0 || strncmp(command,"quit",4) == 0 || strncmp(command,"exit",5) == 0 ){
		//quit
		quit();
	}
	else if (strncmp(command,"a",1) == 0 || strncmp(command,"add ",4) == 0){
		//add
		char hostname[HOST_NAME_BUF];
		int portnumber;
		char add[3];
		if (sscanf(command,"%s %s %d",add,hostname,&portnumber) == 3){
			addHost(hostname,portnumber);
		} else {
			xprintf("Usage: add <hostname> <portnumber>\n");
		}

	}
	else if (strncmp(command,"r",1) == 0 || strncmp(command,"remove ",7) == 0){
		//remove
		char hostname[HOST_NAME_BUF];
		int portnumber;
		char remove[3];
		if (sscanf(command,"%s %s %d",remove,hostname,&portnumber) == 3){
			removeHost(hostname,portnumber);    
		} else {
			xprintf("Usage: remove <hostname> <portnumber>\n");
		}
	} 
	else if (strncmp(command,"l",1) == 0 || strncmp(command,"list",4) == 0){
		//list
		listHosts();
	} 
    //DEBUG COMMANDS
	/*else if (strncmp(command,"t",1) == 0){
		addHost("localhost",9000);
	}
    else if (strncmp(command,"s",1) == 0){
        removeHost("localhost",9000);
    }*/
	else {
		//print help if improper or no command
		xprintf("%s\n",help);
	}
}



void addHost(char* hostname, int portnumber){
	//add a host by a name and port number

	if (!checkPort(portnumber)){
		xprintf("Port number not correct\n");
		return;
	}

    for (int n = 0; n < MAX_SERVERS; n++){
        if (hosts[n].status == H_CONN 
                && strncmp(hostname,hosts[n].hostname,strlen(hosts[n].hostname))== 0 
                && hosts[n].portnum == portnumber){
            xprintf("Already connected to %s on port %d\n",hostname,portnumber);
            return;
        }
    }

	if (activeHosts(hosts,MAX_SERVERS) >= MAX_SERVERS){
		xprintf("Unable to add server; maximum number reached (%d)\n",MAX_SERVERS);
		return;
	}

	xprintf("Attempting to connect to %s on port %d\n",hostname,portnumber);

	struct sockaddr_in serv_addr;
	struct hostent *server;
	int sockfd;
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0){
		fprintf(stderr,"Error opening socket!\n");
		return;
	}
	server = gethostbyname(hostname);
	if (server == NULL) {
		fprintf(stderr,"Error, host with name %s does not exist!\n",hostname);
		return;
	}
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr_list[0], 
			(char *)&serv_addr.sin_addr.s_addr,
			server->h_length);
	serv_addr.sin_port = htons(portnumber);
	if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0){
		fprintf(stderr,"Error, could not connect (make sure server is running on %s %d)\n",hostname,portnumber);
		return;
	}

	//select the first unconnected host struct
	int n = selectDiscHost(hosts,MAX_SERVERS);

	host* h = &hosts[n];
    initHost(h, hostname, sockfd, portnumber);

	//listen to the socket
	FD_SET(h->sockfd,&inputs);
	if (sockfd > fdmax){
		fdmax = sockfd;
	}

}

void removeHost(char* hostname, int portnumber){
	//disconnect from a host
	if (!checkPort(portnumber)){
		xprintf("Port number not correct\n");
		return;
	}
	for (int i =0; i < MAX_SERVERS; i++){
		host* h = &hosts[i];
		if (strncmp(hostname,h->hostname,strlen(h->hostname)) == 0
				&& h->portnum == portnumber){
			terminateClientConnection(h);
			return;
		}
	}

	//host not found
	xprintf("Not connected to %s on port %d. Remove failed\n",hostname,portnumber);
}

void listHosts(){
	//print a list of hosts
	int aH = activeHosts(hosts,MAX_SERVERS);
	if (aH == 0){
		xprintf("Not currently connected to any hosts\n");
		return;
	}	
	xprintf("Connected to %d of %d hosts:\n",aH, MAX_SERVERS);
	for (int i = 0; i < MAX_SERVERS; i++){
		host* h = &hosts[i];
		if (h->status == H_CONN)
			xprintf("%s on port %d (Directory: %s Period: %d seconds)\n",h->hostname,h->portnum,h->dirname,h->period);
	}
}

void cleanup(){
    //cleanup code for the client
	terminateAll();
}



void my_rl_printf(char *fmt, ...){
	//Jean-Bernard Pellerin helped me with this
	//modified from his version of xprintf to re-prompt 
	//with the current text on the command line
	//uses global var reprint to say whether or not we
	//need to re-prompt (turned off during execCommand)
    char *saved_line;
    int saved_point;
    if (reprint){
		saved_point = rl_point;
    	saved_line = rl_copy_text(0, rl_end);
        rl_save_prompt();
        rl_replace_line("", 0);
        rl_redisplay();
    }

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    if (reprint){
        rl_restore_prompt();
        rl_replace_line(saved_line, 0);
        rl_point = saved_point;
        rl_redisplay();
    }
}
