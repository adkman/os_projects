#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/mman.h>
#include <stdbool.h>

#define BUFF_SIZE 40
#define DECIMAL_BASE 10
#define errExit(msg)        \
    do                      \
    {                       \
        perror(msg);        \
        exit(EXIT_FAILURE); \
    } while (0)

static int page_size;

int main(int argc, const char *argv[])
{
    int srcPort = -1, dstPort = -1;
    int client_sock = 0, server_sock = 0;
    struct sockaddr_in sock_addr;
    int addrlen = sizeof(sock_addr);
    int opt = 1;
    char buffer[BUFF_SIZE] = {0};
    char *token;

    int num_pages = -1;
    bool isFirstProc = false;
    char *addr;
    unsigned long len = 0;

    /* Check for appropriate number of arguments */
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s src-port dst-port\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /* Parse the arguments */
    errno = 0;
    srcPort = strtol(argv[1], NULL, DECIMAL_BASE);
    if (errno != 0)
    {
        fprintf(stderr, "Error parsing src-port: error code - %d\n", errno);
        exit(EXIT_FAILURE);
    }
    errno = 0;
    dstPort = strtol(argv[2], NULL, DECIMAL_BASE);
    if (errno != 0)
    {
        fprintf(stderr, "Error parsing dst-port: error code - %d\n", errno);
        exit(EXIT_FAILURE);
    }
    errno = 0;

    /* Create a socket for this process to communicate */
    if ((client_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Socket creation error \n");
        return -1;
    }

    /* Try to connect to the destination */

    memset(&sock_addr, '0', sizeof(sock_addr));
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_port = htons(dstPort);

    if (inet_pton(AF_INET, "127.0.0.1", &sock_addr.sin_addr) <= 0)
    {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    if (connect(client_sock, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) < 0)
    {
        printf("\nConnection Failed to %d\n", dstPort);
	printf("\nThis is the first process!\n");
        isFirstProc = true;
        server_sock = client_sock;
    }
    else
    {
        printf("\nConnection succeeded to %d\n", dstPort);
        printf("\nThis is the second process!\n");
        printf("\nProcesses paired!\n");
    }

    if (isFirstProc)
    {
        /* Listening on the source port for connections from second process */
        if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                       &opt, sizeof(opt)))
        {
            errExit("setsockopt");
        }

        sock_addr.sin_family = AF_INET;
        sock_addr.sin_addr.s_addr = INADDR_ANY;
        sock_addr.sin_port = htons(srcPort);

        if (bind(server_sock, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) < 0)
        {
            errExit("bind failed");
        }

        if (listen(server_sock, 3) < 0)
        {
            errExit("listen failed");
        }

        if ((client_sock = accept(server_sock, (struct sockaddr *)&sock_addr,
                                    (socklen_t *)&addrlen)) < 0)
        {
            errExit("accept failed");
        }
        else
        {
            printf("\nProcesses paired!\n");
        }

        /* Ask the user for number of pages to be allocated */
        printf("\n> How many pages would you like to allocate (> 0)?\n");
        if (scanf("%d", &num_pages) == EOF)
	{
            errExit("scanf");
        }

        page_size = sysconf(_SC_PAGE_SIZE);
        len = num_pages * page_size;

        /* create the memory mapping of the computed length */
        addr = mmap(NULL, len, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (addr == MAP_FAILED)
        {
            errExit("First proc mmap");
        }

        /* print the address of memory region and size */
        printf("\nAddress returned by mmap() = %p\n", addr);
        printf("Size of mapped region = %lu\n\n", len);

        /* send the address and size to the second process over socket */
        sprintf(buffer, "%lu %lu", (unsigned long) addr, len);
        send(client_sock, buffer, strlen(buffer), 0);
    }
    else
    {
        /* Second process */

        /* Receive the address and size from the first process */
        if (read(client_sock, buffer, BUFF_SIZE) < 0)
        {
            printf("\nRead Failed \n");
            return -1;
        }

        token = strtok(buffer, " ");
        addr = (char *) strtoul(token, NULL, DECIMAL_BASE);

        token = strtok(NULL, " ");
	errno = 0;
        len = strtol(token, NULL, DECIMAL_BASE);
        if (errno < 0)
        {
            fprintf(stderr, "Error in converting str to int for token %s with error code %d\n", token, errno);
            exit(EXIT_FAILURE);
        }

        /* create the memory mapping of the computed length */
        addr = mmap(addr, len, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (addr == MAP_FAILED)
        {
            errExit("Second proc mmap");
        }

        /* print the address of mmapped region and size */
        printf("\nAddress returned by mmap() = %p\n", addr);
        printf("Size of mapped region = %lu\n\n", len);
    }

    return 0;
}
