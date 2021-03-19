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

char msi_map[3] = {'M', 'S', 'I'};

enum State {M, S, I};

enum State *msi_array;

enum Instruction {FETCH, INVALIDATE};

enum MessageType {REQUEST, RESPONSE, ERR_RESPONSE};

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

struct info
{
    long uffd;
    long sockfd;
    char* addr;
    unsigned long len;
    int num_pages;
};

struct message
{
    enum MessageType msgType;
    enum Instruction instr;
    long pageNum;
    char buffer[4096];
};

static int page_size;

void set_socket_blocking_mode(int sockfd, bool isBlocking)
{
    int flags = fcntl(sockfd, F_GETFL, 0);
    flags = isBlocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
    fcntl(sockfd, F_SETFL, flags);
}

static void *
request_listener_thread(void *arg)
{
    struct info *info = (struct info *)arg;

    set_socket_blocking_mode(info->sockfd, false);

    while (true)
    {
        struct message msg;

        pthread_mutex_lock(&mutex); 
        errno = 0;
        if (read(info->sockfd, &msg, sizeof(struct message)) < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                pthread_mutex_unlock(&mutex);
                continue;
            }
            else
            {
                fprintf(stderr, "\nRead Failed %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            if (msg.msgType != REQUEST)
            {
                pthread_mutex_unlock(&mutex);
                continue;
            }
        }
        pthread_mutex_unlock(&mutex);

        if (msg.instr == FETCH)
        {
            if (msi_array[msg.pageNum] == I)
            {
                msg.msgType = ERR_RESPONSE;
                if (send(info->sockfd, &msg, sizeof(struct message), 0) < 0)
                {
                    fprintf(stderr, "\nSend Failed %s\n", strerror(errno));
                    exit(EXIT_FAILURE);
                }
            }
            else
            {
                msg.msgType = RESPONSE;
                memcpy(msg.buffer, &info->addr[msg.pageNum * page_size], page_size);
                if (send(info->sockfd, &msg, sizeof(struct message), 0) < 0)
                {
                    fprintf(stderr, "\nSend Failed %s\n", strerror(errno));
                    exit(EXIT_FAILURE);
                }
                msi_array[msg.pageNum] = S;
            }
        }
        else if (msg.instr == INVALIDATE)
        {
            msi_array[msg.pageNum] = I;
            madvise(&info->addr[msg.pageNum * page_size], page_size, MADV_DONTNEED);
        }
    }

    errExit("Should not reach here");
}

static void *
fault_handler_thread(void *arg)
{
    static struct uffd_msg uffd_msg;
    long uffd;
    struct uffdio_copy uffdio_copy;
    struct uffdio_zeropage uffdio_zeropage;
    ssize_t nread;

    struct info *info = (struct info*)arg;
    uffd = info->uffd;

    for (;;)
    {
        struct pollfd pollfd;
        int nready;
        struct message msg;
        unsigned long pageAddr;

        pollfd.fd = uffd;
        pollfd.events = POLLIN;
        nready = poll(&pollfd, 1, -1);
        if (nready == -1)
        {
            errExit("poll");
        }

        nread = read(uffd, &uffd_msg, sizeof(uffd_msg));
        if (nread == 0)
        {
            errExit("EOF on userfaultfd!");
        }
        if (nread == -1)
        {
            errExit("read");
        }

        if (uffd_msg.event != UFFD_EVENT_PAGEFAULT)
        {
            fprintf(stderr, "Unexpected event on userfaultfd\n");
            exit(EXIT_FAILURE);
        }

        /* Assuming pagefault will only happen when MSI state is Invalid */
        pageAddr = (unsigned long)uffd_msg.arg.pagefault.address & 
                    ~(page_size - 1);
        msg.pageNum = (pageAddr - (unsigned long)info->addr) / page_size;

        msg.msgType = REQUEST;
        if (uffd_msg.arg.pagefault.flags == UFFD_PAGEFAULT_FLAG_WRITE)
        {
            msg.instr = INVALIDATE;
            if (send(info->sockfd, &msg, sizeof(msg), 0) < 0)
            {
                fprintf(stderr, "\nSend Failed %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            msi_array[msg.pageNum] = M;
        }
        else    /* Pagefault on read */
        {
            msg.instr = FETCH;

            printf("  [X] PAGEFAULT %ld\n", msg.pageNum);

            pthread_mutex_lock(&mutex);
            set_socket_blocking_mode(info->sockfd, true);
            if (send(info->sockfd, &msg, sizeof(struct message), 0) < 0)
            {
                fprintf(stderr, "\nSend Failed %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }

            /* Assuming that the read call will get appropriate response for the FETCH request */
            if (read(info->sockfd, &msg, sizeof(struct message)) < 0)
            {
                fprintf(stderr, "\nRead Failed %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            set_socket_blocking_mode(info->sockfd, false);
            pthread_mutex_unlock(&mutex);
        }

        if (uffd_msg.arg.pagefault.flags == UFFD_PAGEFAULT_FLAG_WRITE 
                || msg.msgType == ERR_RESPONSE)     /* The page was Invalid at other process too (in case of read)*/
        {
            uffdio_zeropage.range.start = pageAddr;
            uffdio_zeropage.range.len = page_size;
            uffdio_zeropage.mode = 0;
            uffdio_zeropage.zeropage = 0;

            if (ioctl(uffd, UFFDIO_ZEROPAGE, &uffdio_zeropage) == -1)
                errExit("ioctl-UFFDIO_ZEROPAGE");

            if (uffdio_zeropage.zeropage != page_size)
                errExit("Error in UFFDIO_ZEROPAGE copying");
        }
        else                        /* Got the page content from other process */
        {
            msi_array[msg.pageNum] = S;

            uffdio_copy.src = (__u64)msg.buffer;
            uffdio_copy.dst = pageAddr;
            uffdio_copy.len = page_size;
            uffdio_copy.mode = 0;
            uffdio_copy.copy = 0;

            if (ioctl(uffd, UFFDIO_COPY, &uffdio_copy) == -1)
                errExit("ioctl-UFFDIO_COPY");

            if (uffdio_copy.copy != page_size)
                errExit("Error in UFFDIO_COPY copying");
        }
    }
}

void handle_read_page(char* baseAddr, int pageNum)
{
    char* pageAddr = &baseAddr[page_size * pageNum];
    char page_buffer[page_size];

    memcpy(page_buffer, pageAddr, page_size);   /* Reading page contents */
    if (msi_array[pageNum] == I)    /* So that next access will again trigger pagefault */
    {
        madvise(pageAddr, page_size, MADV_DONTNEED);
        printf("  [*] Page %i:\n%s\n", pageNum, "");
    }
    else
    {
        printf("  [*] Page %i:\n%s\n", pageNum, page_buffer);
    }
}

void handle_write_page(char* baseAddr, int pageNum, char* page_buffer)
{
    char* pageAddr = &baseAddr[page_size * pageNum];
    if (msi_array[pageNum] == S)    /* So that page fault handler will send INVALIDATE to other process */
    {
        madvise(pageAddr, page_size, MADV_DONTNEED);
    }
    memcpy(pageAddr, page_buffer, strlen(page_buffer) + 1);  /* Writing on the page */
    printf("  [*] Page %i:\n%s\n", pageNum, pageAddr);  /* Printing updated contents of page */
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
    pthread_t thr, thr_request;
    int s;

    char op;
    int pageNum;
    struct info *info = malloc(sizeof(struct info));

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
        fprintf(stderr, "Error parsing src-port: error code - %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    errno = 0;
    dstPort = strtol(argv[2], NULL, DECIMAL_BASE);
    if (errno != 0)
    {
        fprintf(stderr, "Error parsing dst-port: error code - %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    page_size = sysconf(_SC_PAGE_SIZE);

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
        sprintf(buffer, "%lu %lu", (unsigned long)addr, len);
        send(client_sock, buffer, strlen(buffer), 0);

        if (getchar() == EOF)   /* handles the \n left by scanf */
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
        addr = (char *)strtoul(token, NULL, DECIMAL_BASE);

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

    /* Initializing the MSI array as Invalid for every page */
    msi_array = malloc(num_pages * sizeof(enum State));
    for (int i = 0; i < num_pages; i++)
    {
        msi_array[i] = I;
    }

    /* registering mmapped region with userfaultfd */
    uffd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK);
    if (uffd == -1)
        errExit("userfaultfd");

    uffdio_api.api = UFFD_API;
    uffdio_api.features = 0;
    if (ioctl(uffd, UFFDIO_API, &uffdio_api) == -1)
        errExit("ioctl-UFFDIO_API");

    uffdio_register.range.start = (unsigned long)addr;
    uffdio_register.range.len = len;
    uffdio_register.mode = UFFDIO_REGISTER_MODE_MISSING;
    if (ioctl(uffd, UFFDIO_REGISTER, &uffdio_register) == -1)
        errExit("ioctl-UFFDIO_REGISTER");

    info->sockfd = client_sock;
    info->addr = addr;
    info->len = len;
    info->num_pages = num_pages;
    info->uffd = uffd;

    /* Creating a thread to monitor and handle the requests from other process */
    s = pthread_create(&thr_request, NULL, request_listener_thread, (void *)info);
    if (s != 0)
    {
        errno = s;
        errExit("pthread_create request listener");
    }
    
    /* Creating a thread to monitor and handle the pagefault events */
    s = pthread_create(&thr, NULL, fault_handler_thread, (void *)info);
    if (s != 0)
    {
        errno = s;
        errExit("pthread_create userfaultfd listener");
    }

    /* Repeatedly ask user */
    while (1)
    {
        char page_buffer[page_size];

        printf("> Which command should I run? (r:read, w:write, v:view msi array): ");
        errno = 0;
        if (fgets(buffer, BUFF_SIZE, stdin) == NULL)
        {
            errExit("fgets error command");
        }

        op = buffer[0];
        if (op != 'r' && op != 'w' && op!= 'v')
        {
            fprintf(stderr, "Invalid operation: only 'r', 'w' and 'v' is supported");
            exit(EXIT_FAILURE);
        }


        printf("> For which page? (0-%i, or -1 for all): ", num_pages - 1);

        errno = 0;
        if (fgets(buffer, BUFF_SIZE, stdin) == NULL)
        {
            errExit("fgets error page");
        }

        errno = 0;
        pageNum = strtol(buffer, NULL, DECIMAL_BASE);
        if (errno != 0 || (pageNum == 0 && buffer[0] != '0'))
        {
            fprintf(stderr, "Invalid page number: error code %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        if (pageNum >= num_pages || pageNum < -1)
        {
            fprintf(stderr, "Invalid page number: Should be (0-%i, or -1 for all)\n", num_pages - 1);
            exit(EXIT_FAILURE);
        }

        if (op == 'v')
        {
            if (pageNum == -1)
            {
                for (int i = 0; i < num_pages; i++)
                {
                    printf("  MSI state for Page %i: %c\n", i, msi_map[msi_array[i]]);
                }
            }
            else
            {
                printf("  MSI state for Page %i: %c\n", pageNum, msi_map[msi_array[pageNum]]);
            }
        }
        else if (op == 'w')
        {
            printf("> Type your new message: ");
            errno = 0;
            if (fgets(page_buffer, page_size, stdin) == NULL)
            {
                errExit("fgets error content");
            }

            page_buffer[strlen(page_buffer) - 1] = '\0';

            if (pageNum == -1)
            {
                for (int i = 0; i < num_pages; i++)
                {
                    handle_write_page(addr, i, page_buffer);
                }
            }
            else
            {
                handle_write_page(addr, pageNum, page_buffer);
            }
        }
        else if (op == 'r')
        {
            if (pageNum == -1)
            {
                for (int i = 0; i < num_pages; i++)
                {
                    handle_read_page(addr, i);
                }
            }
            else
            {
                handle_read_page(addr, pageNum);
            }
        }
    }

    free(msi_array);
    free(info);
    return 0;
}
