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
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <stdbool.h>
#include <linux/userfaultfd.h>
#include <pthread.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>

#define BUFF_SIZE 40
#define DECIMAL_BASE 10
#define errExit(msg)        \
    do                      \
    {                       \
        perror(msg);        \
        exit(EXIT_FAILURE); \
    } while (0)

static int page_size;

void read_all_pages_and_print(char* base_addr, int num_pages)
{
    char page_buffer[page_size];
    int i, l;

    for (i = 0, l = 0x0; i < num_pages; i++, l += page_size)
    {
        memcpy(page_buffer, &base_addr[l], page_size);
        printf("  [*] Page %i:\n%s\n", i, page_buffer);
    }
}

void write_all_pages(char* base_addr, int num_pages, char* buffer)
{
    int i, l;
    for (i = 0, l = 0x0; i < num_pages; i++, l += page_size)
    {
        memcpy(&base_addr[l], buffer, strlen(buffer) + 1);
    }
}

static void *
fault_handler_thread(void *arg)
{
    static struct uffd_msg msg;
    long uffd;
    struct uffdio_zeropage uffdio_zeropage;
    ssize_t nread;

    uffd = (long) arg;

    for (;;) {
        struct pollfd pollfd;
        int nready;

        pollfd.fd = uffd;
        pollfd.events = POLLIN;
        nready = poll(&pollfd, 1, -1);
        if (nready == -1)
            errExit("poll");

        nread = read(uffd, &msg, sizeof(msg));
        if (nread == 0) {
            errExit("EOF on userfaultfd!");
        }
        if (nread == -1)
            errExit("read");

        if (msg.event != UFFD_EVENT_PAGEFAULT) {
            fprintf(stderr, "Unexpected event on userfaultfd\n");
            exit(EXIT_FAILURE);
        }

        printf("  [X] PAGEFAULT\n");

        uffdio_zeropage.range.start = (unsigned long) msg.arg.pagefault.address &
            ~(page_size - 1);
        uffdio_zeropage.range.len = page_size;
        uffdio_zeropage.mode = 0;
        uffdio_zeropage.zeropage = 0;

        if (ioctl(uffd, UFFDIO_ZEROPAGE, &uffdio_zeropage) == -1)
            errExit("ioctl-UFFDIO_ZEROPAGE");

        if (uffdio_zeropage.zeropage != page_size)
            errExit("Error in UFFDIO_ZEROPAGE copying");
    }
}

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

    long uffd;
    struct uffdio_api uffdio_api;
    struct uffdio_register uffdio_register;
    pthread_t thr;
	int s;

    char op;
    int pageNum;
    char *page_buffer;

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
        fprintf(stderr, "\n Socket creation error \n");
        exit(EXIT_FAILURE);
    }

    /* Try to connect to the destination */

    memset(&sock_addr, '0', sizeof(sock_addr));
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_port = htons(dstPort);

    if (inet_pton(AF_INET, "127.0.0.1", &sock_addr.sin_addr) <= 0)
    {
        fprintf(stderr, "\nInvalid address/ Address not supported \n");
        exit(EXIT_FAILURE);
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

    page_size = sysconf(_SC_PAGE_SIZE);

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
        printf("\n> How many pages would you like to allocate (> 0)? ");
        if (scanf("%d", &num_pages) == EOF)
            errExit("scanf");

        len = num_pages * page_size;

        /* create the memory mapping of the computed length */
        addr = mmap(NULL, len, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (addr == MAP_FAILED)
            errExit("First proc mmap");

        /* print the address of memory region and size */
        printf("\nAddress returned by mmap() = %p\n", addr);
        printf("Size of mapped region = %lu\n\n", len);

        /* send the address and size to the second process over socket */
        sprintf(buffer, "%lu %lu", (unsigned long) addr, len);
        send(client_sock, buffer, strlen(buffer), 0);

        if (getchar() == EOF) 
            errExit("Got EOF at getchar");
    }
    else
    {
        /* Second process */

        /* Receive the address and size from the first process */
        if (read(client_sock, buffer, BUFF_SIZE) < 0)
        {
            fprintf(stderr, "\nRead Failed \n");
            exit(EXIT_FAILURE);
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
        num_pages = len / page_size;

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

    /* registering mmapped region with userfaultfd */

    uffd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK);
    if (uffd == -1)
        errExit("userfaultfd");

    uffdio_api.api = UFFD_API;
    uffdio_api.features = 0;
    if (ioctl(uffd, UFFDIO_API, &uffdio_api) == -1)
        errExit("ioctl-UFFDIO_API");

    uffdio_register.range.start = (unsigned long) addr;
    uffdio_register.range.len = len;
    uffdio_register.mode = UFFDIO_REGISTER_MODE_MISSING;
    if (ioctl(uffd, UFFDIO_REGISTER, &uffdio_register) == -1)
        errExit("ioctl-UFFDIO_REGISTER");

    s = pthread_create(&thr, NULL, fault_handler_thread, (void *) uffd);
    if (s != 0) {
        errno = s;
        errExit("pthread_create");
    }

    page_buffer = malloc(page_size);

    /* Repeatedly ask user */
    while (1)
    {
        printf("> Which command should I run? (r:read, w:write): ");
		errno = 0;
        if (fgets(buffer, 2, stdin) == NULL)
            errExit("fgets error command");

        op = buffer[0];
        if (op != 'r' && op != 'w')
        {
            fprintf(stderr, "Invalid operation: only 'r' or 'w' is supported");
            continue;
        }


        printf("> For which page? (0-%i, or -1 for all): ", num_pages - 1);
        if (getchar() == EOF)
            errExit("Got EOF at getchar");

		errno = 0;
        if (fgets(buffer, BUFF_SIZE, stdin) == NULL)
            errExit("fgets error page");

        errno = 0;
        pageNum = strtol(buffer, NULL, DECIMAL_BASE);
        if (errno != 0 || (pageNum == 0 && buffer[0] != '0'))
        {
            fprintf(stderr, "Invalid page number: error code %d\n", errno);
            continue;
        }

        if (pageNum >= num_pages || pageNum < -1)
        {
            fprintf(stderr, "Invalid page number: Should be (0-%i, or -1 for all)\n", num_pages - 1);
            continue;
        }


        if (op == 'w')
        {
            printf("> Type your new message: ");
			errno = 0;
            if (fgets(page_buffer, page_size, stdin) == NULL)
                errExit("fgets error content");

            page_buffer[strlen(page_buffer) - 1] = '\0';

            /* write the data in buffer into the pageNum(s) */
            if (pageNum == -1)
            {
                write_all_pages(addr, num_pages, page_buffer);
            }
            else
            {
                memcpy(&addr[0x0 + page_size * pageNum], page_buffer, strlen(page_buffer) + 1);
            }
        }
        if (pageNum == -1)
        {
            read_all_pages_and_print(addr, num_pages);
        }
        else
        {
            memcpy(page_buffer, &addr[0x0 + page_size * pageNum], page_size);
            printf("  [*] Page %i:\n%s\n", pageNum, page_buffer);
        }
    }

    return 0;
}
