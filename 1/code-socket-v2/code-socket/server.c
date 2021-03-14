#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>

#define PORT 5984
#define BUFF_SIZE 4096

int main(int argc, const char *argv[])
{
	int server_fd, new_socket;
	struct sockaddr_in address;
	int opt = 1;
	int addrlen = sizeof(address);
	char buffer[BUFF_SIZE] = {0};
	char *hello = "Hello from server";

	/* [S1]
	 * Explain the following here.
	 * 
	 * int socket(domain, type, protocol)
	 * 
	 * Creates a socket endpoint for communication
	 * and returns descriptor for the server socket.
	 * AF_INET indicates that IPv4 family will be used for communication,
	 * SOCK_STREAM indicates the socket is of TCP type,
	 * 0 indicates that IP protocol will be used.
	 * 
	 * returns a non negative integer if success,
	 * otherwise -1.
	 * Hence if socket fails, the program prints the descriptive error
	 * message to stderr
	 * for e.g.
	 * socket failed: Invalid flags in type.
	 * and exits with failure if socket creation fails.
	 */
	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket failed");
		exit(EXIT_FAILURE);
	}

	/* [S2]
	 * Explain the following here.
	 * 
	 * int setsockopt(int sockfd, int level, int optname, 
	 * 					const void *optval, socklen_t optlen)
	 * 
	 * Manipulates the server socket file descriptor with 
	 * SOL_SOCKET as level (which manipulates options at socket API level)
	 * SO_REUSEADDR as an option to permit reuse of local addresses.
	 * SO_REUSEPORT as an option to permit reuse of ports so that multiple
	 * sockets on the same host can bind to the same port.
	 * optval (here the variable opt) is set to 1 to enable the provided 
	 * options.
	 * 
	 * returns 0 if success,
	 * otherwise -1.
	 * Hence if setsockopt fails, the program prints the descriptive error
	 * message to stderr
	 * for e.g.
	 * setsockopt: The file descriptor sockfd does not refer to a socket.
	 * and exits with failure.
	 */
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
		       &opt, sizeof(opt))) {
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

	/* [S3]
	 * Explain the following here.
	 * 
	 * Here the sockaddr_in object is configured.
	 * The sin_family is configured with IPV4 family
	 * sin_addr.s_addr is configured with INADDR_ANY (0.0.0.0) which means that
	 * the server will listen on all IPs of the host
	 * sin_port is configured with port 5984 so that the server will listen on
	 * 5984 port.
	 */
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons( PORT );

	/* [S4]
	 * Explain the following here.
	 * 
	 * int bind(int sockfd, const struct sockaddr *addr,
	 *            socklen_t addrlen);
	 * 
	 * Assigns name to the socket.
	 * Meaning, assigns the address details from the sockaddr_in object
	 * to the server_fd.
	 * 
	 * returns 0 if success,
	 * otherwise -1.
	 * Hence if bind fails, the program prints the descriptive error
	 * message to stderr
	 * for e.g.
	 * bind failed: The socket is already bound to an address.
	 * and exits with failure.
	 */
	if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
		perror("bind failed");
		exit(EXIT_FAILURE);
	}

	/* [S5]
	 * Explain the following here.
	 * 
	 * int listen(int sockfd, int backlog)
	 * 
	 * This will mark the server_fd as passive, meaning server_fd will wait 
	 * for an incoming connection.
	 * 3 (backlog) will configure that there can be only 3 connections waiting
	 * in queue for the server_fd.
	 * 
	 * returns 0 if success,
	 * otherwise -1.
	 * Hence if listen fails, the program prints the descriptive error
	 * message to stderr
	 * for e.g.
	 * listen: The argument sockfd is not a valid file descriptor.
	 * and exits with failure.
	 */
	if (listen(server_fd, 3) < 0) {
		perror("listen");
		exit(EXIT_FAILURE);
	}

	/* [S6]
	 * Explain the following here.
	 * 
	 * int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
	 * 
	 * This will accept the connection from the first entry from the queue,
	 * extract the socket information from it and set it to new_socket object.
	 * This will also extract the address info from the client and set it into
	 * the address object.
	 * 
	 * returns a non negative integer (file descriptor of the accepted socket)
	 * if success, otherwise -1.
	 * Hence if accept fails, the program prints the descriptive error
	 * message to stderr
	 * for e.g.
	 * accept: sockfd is not an open file descriptor.
	 * and exits with failure.
	 */
	if ((new_socket = accept(server_fd, (struct sockaddr *)&address,
				 (socklen_t*)&addrlen)) < 0) {
		perror("accept");
		exit(EXIT_FAILURE);
	}

	/* [S7]
	 * Explain the following here.
	 * 
	 * This will make server program to wait for a character input from user.
	 * This is helpful when we want to pause the execution of a process and 
	 * resume it on manual input.
	 */
	printf("Press any key to continue...\n");
	getchar();

	/* [S8]
	 * Explain the following here.
	 * 
	 * ssize_t read(int fd, void *buf, size_t count)
	 * 
	 * Reads 1024 bytes from the new_socket (client) and stores 
	 * it into buffer object which is a char array.
	 * Then prints to stdout the contents of buffer.
	 * The server first waits until it receives some data from the
	 * accepted client.
	 * 
	 * returns the number of bytes read if success,
	 * otherwise -1.
	 * Hence if read fails, the program prints the descriptive error
	 * message to stderr
	 * for e.g.
	 * read: fd is not a valid file descriptor or is not open for reading.
	 * and exits with failure.
	 */
	if (read(new_socket, buffer, 1024) < 0) {
		perror("read");
		exit(EXIT_FAILURE);
	}
	printf("Message from a client: %s\n", buffer);

	/* [S9]
	 * Explain the following here.
	 * 
	 * ssize_t send(int sockfd, const void *buf, size_t len, int flags)
	 * 
	 * This sends the contents of hello object (Hello from server)
	 * to the client represented by the new_socket descriptor.
	 * Here we are not capturing whether the data was sent successfully
	 * or not.
	 * Then "Hello message sent" is printed on stdout
	 * and returns successfully from the server.
	 */
	send(new_socket, hello, strlen(hello), 0);
	printf("Hello message sent\n");
	return 0;
}
