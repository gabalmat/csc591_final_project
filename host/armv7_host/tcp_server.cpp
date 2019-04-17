#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h> 

#define PORT     		8080
#define NUM_ELEMENTS	1024

int main()
{
	int listenfd = 0, connfd = 0;
	struct sockaddr_in servaddr, client;
	socklen_t client_size;
	
	float *client_data, *arr_ptr;
	
	int mem_size, received; 

	// int total_received;
	
	mem_size = NUM_ELEMENTS * sizeof(float);
	client_data = (float *)malloc(mem_size);
	
	// Create socket for accepting connections
	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("Bind()\n");
        exit(3);
	}
	
	memset(&servaddr, 0, sizeof(servaddr));
	
	// Server information
	servaddr.sin_family = AF_INET;	// IPv4
	servaddr.sin_addr.s_addr = INADDR_ANY;
	servaddr.sin_port = htons(PORT);
	
	// Bind the socket with the server address
	if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
		perror("bind failed\n");
		exit(EXIT_FAILURE);
	}
	
	// Listen for connection from 1 client
	if (listen(listenfd, 1) != 0)
    {
        perror("Listen()\n");
        exit(4);
    }
    
	client_size = sizeof(client);

    // Accept a connection
    connfd = accept(listenfd, (struct sockaddr *)&client, &client_size);

	// Recieve the data all at one time
	received = recv(connfd, client_data, mem_size, MSG_WAITALL);

	printf("Recieved %d bytes\n", received);

	if (received != mem_size) {
		perror("recv() failed.\n");
	}
    
    //total_received = 0;
    //received = 0;
    //arr_ptr = client_data;
   
    //while (received != 0 && total_received < mem_size) {
		//int i;
		
		//received = recv(connfd, arr_ptr + total_received, mem_size - total_received, 0);

		//i = 0;
		//while (i < NUM_ELEMENTS) {
			//printf("indx = %d: %f\n", i, client_data[i]);
			//i++;
		//}

		//if (received == -1) {
			//perror("recv() failed.\n");
			//break;
		//}
		
		//if (received > 0) total_received += received;
	//}

	// send the data to OpenCL kernel
	
	
	int i;
	float sum, average;
	sum = 0;
	for (i = 0; i < NUM_ELEMENTS; ++i) {
		//printf("value: %f\n", client_data[i]);

		sum += client_data[i];
	}
	
	average = sum / NUM_ELEMENTS;
	
	// Send message back to client
	if (send(connfd, &average, sizeof(float), 0) < 0) {
		perror("Send()\n");
        exit(7);
	}

	free(client_data);
	
	close(connfd);
	close(listenfd);
	return 0;
	
}

