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
#include <sys/time.h>
#include "fpga_host.h"

#define PORT     		8080
#define NUM_ELEMENTS	1048576

int main()
{
	int listenfd = 0, connfd = 0;
	struct sockaddr_in servaddr, client;
	socklen_t client_size;
	struct timeval kernel_start_time, kernel_end_time, transfer_start_time, transfer_end_time;
	
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
    
    // Start the transfer timer
    gettimeofday(&transfer_start_time, NULL);

	// Recieve the data all at one time
	received = recv(connfd, client_data, mem_size, MSG_WAITALL);
	
	// transfer end time
	gettimeofday(&transfer_end_time, NULL);
	printf("Data transfer time: %ld ms \n", 
		((transfer_end_time.tv_sec * 1000000 + transfer_end_time.tv_usec) - (transfer_start_time.tv_sec * 1000000 + transfer_start_time.tv_usec)) / 1000);

	printf("Recieved %d bytes\n", received);

	if (received != mem_size) {
		perror("recv() failed.\n");
	}
    
    // Start the kernel timer
    //gettimeofday(&kernel_start_time, NULL);
    
	// Send data to OpenCL kernel and save the result in 'average'
	float average = get_average(NUM_ELEMENTS, client_data);
	
	// kernel end time
	//gettimeofday(&kernel_end_time, NULL);
	//printf("Kernel execution time: %ld ms \n", 
		//((kernel_end_time.tv_sec * 1000000 + kernel_end_time.tv_usec) - (kernel_start_time.tv_sec * 1000000 + kernel_start_time.tv_usec)) / 1000);
	
	// Send the value for 'average' back to client
	if (send(connfd, &average, sizeof(float), 0) < 0) {
		perror("Send()\n");
        exit(7);
	}

	free(client_data);
	
	close(connfd);
	close(listenfd);
	return 0;
	
}

