#include "dirapp.h"
const char* usage = "Usage: dirapp portnumber dirname period\n";

int main(int argc, char** argv){
    
    if (argc == 1){
        startClient();
		return 0;
    } else if (argc != 4){
        die(usage);
    }
    startServer(argv[1],argv[2],argv[3]);
	return 0;
}
