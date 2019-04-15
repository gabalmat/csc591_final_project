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
	char *hello = "Hello from FPGA!";
	struct sockaddr_in servaddr, cliaddr;
	
	// Create the socket file descriptor
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket creation failed on server");
		exit(EXIT_FAILURE);
	}
	
	memset(&servaddr, 0, sizeof(servaddr));
	memset(&cliaddr, 0, sizeof(cliaddr));
	
	// Server information
	servaddr.sin_family = AF_INET;	// IPv4
	servaddr.sin_addr.s_addr = INADDR_ANY;
	servaddr.sin_port = htons(PORT);
	
	// Bind the socket with the server address
	if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
		perror("bind failed");
		exit(EXIT_FAILURE);
	}
	
	int cliaddr_size, n;
	
	cliaddr_size = sizeof(cliaddr);
	n = recvfrom(sockfd, (char *)buffer, MAXLINE, 0, (struct sockaddr *)&cliaddr, &cliaddr_size);
	
	buffer[n] = '\0';
	printf("Received message: %s from ip address: %s from port: %d\n", 
				buffer,
				inet_ntoa(cliaddr.sin_addr),
				ntohs(cliaddr.sin_port));
	
	sendto(sockfd, hello, (strlen(hello)+1), 0, (struct sockaddr *)&cliaddr, cliaddr_size);
		
	printf("Hello message sent.\n");

	return 0;
}
