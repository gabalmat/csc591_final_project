#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 
  
#define PORT     8080
#define MAXLINE 1024

int main()
{
	int sockfd;
	char buffer[MAXLINE];
	char *hello = "Hello from client";
	char *server_ip = "192.168.2.11";
	char *client_ip = "127.0.0.1";
	struct sockaddr_in servaddr, cliaddr;
	
	// Create socket file descriptor
	if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
        perror("socket creation failed"); 
        exit(EXIT_FAILURE); 
    }
    
    memset(&servaddr, 0, sizeof(servaddr));
	memset(&cliaddr, 0, sizeof(cliaddr));

	// Client information
	cliaddr.sin_family = AF_INET;
	cliaddr.sin_port = htons(PORT);
	cliaddr.sin_addr.s_addr = INADDR_ANY;

	// Bind the client to port 8080
	if (bind(sockfd, (struct sockaddr *)&cliaddr, sizeof(cliaddr)) < 0) {
		perror("bind failed");
		exit(EXIT_FAILURE);
	}
    
    // Server information
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    servaddr.sin_addr.s_addr = inet_addr(server_ip);
    
    int n, servaddr_size;
	servaddr_size = sizeof(servaddr);
    
    sendto(sockfd, hello, (strlen(hello)+1), 0, 
			(struct sockaddr *) &servaddr, servaddr_size); 
    printf("Hello message sent.\n");
    
    n = recvfrom(sockfd, (char *)buffer, MAXLINE,  
                0, (struct sockaddr *) &servaddr, &servaddr_size); 
    buffer[n] = '\0'; 
    printf("Server : %s\n", buffer); 
  
    close(sockfd); 
    return 0;	
}
