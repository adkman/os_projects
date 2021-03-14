#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>

#define PORT 5984
#define BUFF_SIZE 4096

int main(int argc, const char *argv[])
{
	int sock = 0;
	struct sockaddr_in serv_addr;
	char *hello = "Hello from client";
	char buffer[BUFF_SIZE] = {0};

	/* [C1]
	 * Explain the following here.
	 * 
	 * int socket(domain, type, protocol)
	 * 
	 * Creates a socket endpoint for communication
	 * and returns a descriptor that will be used to communicate with server.
	 * AF_INET indicates that IPv4 family will be used for communication,
	 * SOCK_STREAM indicates the socket is of TCP type,
	 * 0 indicates that IP protocol will be used.
	 * 
	 * returns a non negative integer if success,
	 * otherwise -1.
	 * Hence if socket creation fails, the program prints error on stdout
	 * for e.g. 
	 * 
	 *  Socket creation error 
	 * 
	 * and returns -1.
	 */
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("\n Socket creation error \n");
		return -1;
	}

	/* [C2]
	 * Explain the following here.
	 * 
	 * The serv_addr object is filled with '0's.
	 * The sin_family is configured with IPV4 family
	 * sin_port is configured with port 5984 so that the client will
	 * connect to the server's 5984 port.
	 */
	memset(&serv_addr, '0', sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);

	/* [C3]
	 * Explain the following here.
	 * 
	 * int inet_pton(int af, const char *src, void *dst)
	 * 
	 * Here the IPV4 address (127.0.0.1) is converted from text to
	 * binary form and stored in the serv_addr.sin_addr object.
	 * As the address is 127.0.0.1 (localhost), the client is trying
	 * to connect to a server running on the same host.
	 * 
	 * returns 1 if success,
	 * 0 if src is not a valid address,
	 * -1 if address family is invalid.
	 * Hence if conversion fails, the program prints error on stdout
	 * for e.g. 
	 * 
	 *  Invalid address/ Address not supported
	 * 
	 * and returns -1.
	 */
	if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
		printf("\nInvalid address/ Address not supported \n");
		return -1;
	}

	/* [C4]
	 * Explain the following here.
	 * 
	 * int connect(int sockfd, const struct sockaddr *addr,
	 *             socklen_t addrlen)
	 * 
	 * This connects the sock with server's address in order to connect
	 * to the server. This will send a connection request to the server.
	 * 
	 * returns 0 if success,
	 * otherwise -1.
	 * Hence if connect fails, the program prints error on stdout
	 * for e.g. 
	 * 
	 * Connection Failed 
	 * 
	 * and returns -1.
	 */
	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		printf("\nConnection Failed \n");
		return -1;
	}


	/* [C5]
	 * Explain the following here.
	 * 
	 * This will make client program to wait for a character input from user.
	 * This is helpful when we want to pause the execution of a process and 
	 * resume it on manual input.
	 */
	printf("Press any key to continue...\n");
	getchar();

	/* [C6]
	 * Explain the following here.
	 * 
	 * ssize_t send(int sockfd, const void *buf, size_t len, int flags)
	 * 
	 * This sends the contents of hello object (Hello from client)
	 * to the server represented by the sock descriptor.
	 * Here we are not capturing whether the data was sent successfully
	 * or not.
	 * Then "Hello message sent" is printed on stdout.
	 */
	send(sock, hello, strlen(hello), 0);
	printf("Hello message sent\n");

	/* [C7]
	 * Explain the following here.
	 * 
	 * ssize_t read(int fd, void *buf, size_t count)
	 * 
	 * Reads 1024 bytes from the sock (server) and stores 
	 * it into buffer object which is a char array.
	 * Then prints to stdout the contents of buffer.
	 * Here the client waits until it receives something from
	 * the server.
	 * Then the client program returns successfully.
	 * 
	 * returns the number of bytes read if success,
	 * otherwise -1.
	 * 
	 * If connect fails, the program prints error on stdout
	 * for e.g. 
	 * 
	 * Read Failed 
	 * 
	 * and returns -1.
	 */
	if (read(sock , buffer, 1024) < 0) {
		printf("\nRead Failed \n");
		return -1;
	}
	printf("Message from a server: %s\n", buffer);
	return 0;
}
