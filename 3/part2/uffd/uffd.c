/* userfaultfd_demo.c

   Licensed under the GNU General Public License version 2 or later.
*/
#define _GNU_SOURCE
#include <sys/types.h>
#include <stdio.h>
#include <linux/userfaultfd.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <poll.h>

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE);	\
	} while (0)

static int page_size;

static void *
fault_handler_thread(void *arg)
{
	static struct uffd_msg msg;   /* Data read from userfaultfd */
	static int fault_cnt = 0;     /* Number of faults so far handled */
	long uffd;                    /* userfaultfd file descriptor */
	static char *page = NULL;
	struct uffdio_copy uffdio_copy;
	ssize_t nread;

	uffd = (long) arg;

	/* [H1]
	 * Explain following in here.
	 * 
	 * Here the thread is calling mmap to create a new memory mapping
	 * of 1 page with similar configuration as of the parent process
	 * and the start address is stored in the "page" variable
	 * 
	 */
	if (page == NULL) {
		page = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
			    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (page == MAP_FAILED)
			errExit("mmap");
	}

	/* [H2]
	 * Explain following in here.
	 * 
	 * This is an infinite loop where we keep on doing
	 * the polling and servicing for any pagefault event that occurrs
	 * on the memory region created in the parent.
	 * So the thread will not terminate by itself (if nothing goes
	 * wrong) only when the parent terminates.
	 */
	for (;;) {

		/* See what poll() tells us about the userfaultfd */

		struct pollfd pollfd;
		int nready;

		/* [H3]
		 * Explain following in here.
		 * 
		 * pollfd structure stores the fd to be polled for events
		 * and the requested event is set to be POLLIN which is an
		 * event when there is data to read on that descriptor.
		 * 
		 * the nfds is set to 1 as there is only one descriptor to 
		 * read.
		 * 
		 * This is a blocking call until there is any data available 
		 * for the registered event
		 * i.e. a pagefault occurs
		 * 
		 * as -1 is passed in the timeout field, it will wait for 
		 * infinite amount.
		 * 
		 * When a pagefault will occur, poll will return 1.
		 */
		pollfd.fd = uffd;
		pollfd.events = POLLIN;
		nready = poll(&pollfd, 1, -1);
		if (nready == -1)
			errExit("poll");

		printf("\nfault_handler_thread():\n");
		printf("    poll() returns: nready = %d; "
                       "POLLIN = %d; POLLERR = %d\n", nready,
                       (pollfd.revents & POLLIN) != 0,
                       (pollfd.revents & POLLERR) != 0);

		/* [H4]
		 * Explain following in here.
		 * 
		 * As uffd is a file descriptor, we can just use read call
		 * to read from it in the uffd_msg struct object.
		 * if it returns 0, that means EOF was reached.
		 */
		nread = read(uffd, &msg, sizeof(msg));
		if (nread == 0) {
			printf("EOF on userfaultfd!\n");
			exit(EXIT_FAILURE);
		}

		if (nread == -1)
			errExit("read");

		/* [H5]
		 * Explain following in here.
		 * 
		 * This checks whether the appropriate event (pagefault) was delivered
		 * on the userfaultfd.
		 * The UFFD_EVENT_PAGEFAULT event says that the pagefault details
		 * are available in the object's arg.pagefault field
		 */
		if (msg.event != UFFD_EVENT_PAGEFAULT) {
			fprintf(stderr, "Unexpected event on userfaultfd\n");
			exit(EXIT_FAILURE);
		}

		/* [H6]
		 * Explain following in here.
		 * 
		 * Here we print the pagefault details obtained from the object.
		 * 
		 * msg.arg.pagefault.flags - 
		 * 		if UFFD_PAGEFAULT_FLAG_WRITE it is a write fault
		 * 		if not then it is a read fault
		 * as we had registered with UFFDIO_REGISTER_MODE_MISSING flag in
		 * the parent process.
		 * 
		 * msg.arg.pagefault.address - has the address that triggered the 
		 * pagefault
		 */
		printf("    UFFD_EVENT_PAGEFAULT event: ");
		printf("flags = %llx; ", msg.arg.pagefault.flags);
		printf("address = %llx\n", msg.arg.pagefault.address);

		/* [H7]
		 * Explain following in here.
		 * 
		 * memset is used to fill memory with a constant byte.
		 * 
		 * Here, we fill the whole page with 'A' to 'T' (65 to 84)
		 * depending on the number of faults occurred.
		 * And then the fault_cnt is incremented.
		 */
		memset(page, 'A' + fault_cnt % 20, page_size);
		fault_cnt++;

		/* [H8]
		 * Explain following in here.
		 * 
		 * uffdio_copy structure stores the details of the memory to
		 * be copied.
		 * 
		 * src stores the address from where to copy data
		 * dst stores the address in the userfaultfd registered region
		 * to copy to.
		 * 		Here we set it to the address obtained from the uffd_msg object
		 * It is masked with ~(page_size - 1) i.e. 0xfffff000 which, I guess,
		 * will ensure that the destination address is the base address of that
		 * page in an event where the address in the pagefault was not the actual
		 * base address of the page so we can avoid copying to inaccessible 
		 * memory location.
		 * len stores the number of bytes to copy (here we want to copy the
		 * whole page)
		 * mode stores the flags (here we haven't passed any flags to modify
		 * the copy's behavior)
		 * copy stores the number of bytes copied which will be obtained after
		 * the copy completes (here we have initialized it to 0)
		 */
		uffdio_copy.src = (unsigned long) page;
		uffdio_copy.dst = (unsigned long) msg.arg.pagefault.address &
			~(page_size - 1);
		uffdio_copy.len = page_size;
		uffdio_copy.mode = 0;
		uffdio_copy.copy = 0;

		/* [H9]
		 * Explain following in here.
		 * 
		 * Now we initiate the UFFDIO_COPY operation through ioctl where we send
		 * the uffdio_copy object initialized above.
		 * 
		 * The operation will atomically copy the data and will wake up the 
		 * blocked thread (As we did not set UFFDIO_COPY_MODE_DONTWAKE flag in 
		 * the "mode" attribute of uffdio_copy)
		 * And will set the number of bytes copied in the "copy" attribute.
		 */
		if (ioctl(uffd, UFFDIO_COPY, &uffdio_copy) == -1)
			errExit("ioctl-UFFDIO_COPY");

		/* [H10]
		 * Explain following in here.
		 * 
		 * Here we are printing the actual number of bytes copied by the 
		 * UFFDIO_COPY operation.
		 */
		printf("        (uffdio_copy.copy returned %lld)\n",
                       uffdio_copy.copy);
	}
}

int
main(int argc, char *argv[])
{
	long uffd;          /* userfaultfd file descriptor */
	char *addr;         /* Start of region handled by userfaultfd */
	unsigned long len;  /* Length of region handled by userfaultfd */
	pthread_t thr;      /* ID of thread that handles page faults */
	struct uffdio_api uffdio_api;
	struct uffdio_register uffdio_register;
	int s;
	int l;

	/* [M1]
	 * Explain following in here.
	 * int argc stores the argument count which is the number of arguments
	 * passed to the program when executing it (including the program itself).
	 * For e.g. if this program is executed like "./uffd one two three"
	 * the argc will be 4.
	 * Also, it can be said that argc contains the number of strings in argv.
	 * char *argv[] stores the arguments passed, in a string array.
	 * (Again, the first value in argv will be the program itself).
	 * 
	 * Here, it is being checked whether argc is exactly 2 or not.
	 * (Whether there is exactly 1 command line argument (except the program 
	 * name) or not).
	 * If not, we print the usage which includes the program name in the 
	 * message obtained from argv[0] to stderr.
	 * 
	 * stderr is the Standard Error stream.
	 * fprintf(stderr, ...) will write the message to the standard error stream.
	 * exit(EXIT_FAILURE) will terminate the process with EXIT_FAILURE status
	 * (along with some cleanup) and the program returns with appropriate value
	 * EXIT_FAILURE is a macro which is the standard value for unsuccessful 
	 * termination.
	 */
	if (argc != 2) {
		fprintf(stderr, "Usage: %s num-pages\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	/* [M2]
	 * Explain following in here.
	 * 
	 * sysconf(_SC_PAGE_SIZE)
	 * sysconf is used to get system's configuration info at runtime.
	 * passing _SC_PAGE_SIZE will return the size of a page in bytes.
	 * 
	 * strtoul(const char *nptr, char **endptr, int base)
	 * returns an unsigned long int by converting value in nptr with
	 * the provided base.
	 * Here, we have passed argv[1] (the first command line argument 
	 * (excluding the program name)) as nptr and base as 0.
	 * 0 is a special value for the base where strtoul will decide 
	 * which base (16, 10, 8) to consider as per the content in nptr.
	 * 
	 * the "len" variable will have the length of the mapping to be created
	 * in the virtual address space of this process
	 * (number of pages) * page_size
	 */
	page_size = sysconf(_SC_PAGE_SIZE);
	len = strtoul(argv[1], NULL, 0) * page_size;

	/* [M3]
	 * Explain following in here.
	 * 
	 * syscall calls the specified (first argument) system call 
	 * which is the userfaultfd and passes two flags
	 * O_CLOEXEC and O_NONBLOCK as the parameter.
	 * O_NONBLOCK flag makes the reads from userfaultfd descriptor 
	 * non blocking.
	 * O_CLOEXEC flag enables the close-on-exec flag for the fd
	 * 
	 * The userfaultfd syscall will create a new userfaultfd object
	 * which can be used for the pagefault handling at user space
	 * application. It returns the file descriptor for the created
	 * object if successful
	 * 
	 * If the userfaultfd syscall fails it returns -1 
	 * we print error on stderr and terminate the program with 
	 * EXIT_FAILURE
	 */
	uffd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK);
	if (uffd == -1)
		errExit("userfaultfd");

	/* [M4]
	 * Explain following in here.
	 * 
	 * ioctl is used to configure or communicate with a device 
	 * associated to the file descriptor.
	 * second parameter is the ioctl number (UFFDIO_API)
	 * which enables the userfaultfd object created by the 
	 * syscall and a handshake happens between the kernel and user
	 * space to determine the version and features supported.
	 * The third parameter is the address of the uffdio_api object
	 * which is set with UFFD_API, here the kernel will set all the
	 * available features for this api in this object.
	 */
	uffdio_api.api = UFFD_API;
	uffdio_api.features = 0;
	if (ioctl(uffd, UFFDIO_API, &uffdio_api) == -1)
		errExit("ioctl-UFFDIO_API");

	/* [M5]
	 * Explain following in here.
	 * 
	 * void *mmap(void *addr, size_t length, int prot, int flags,
	 * 			int fd, off_t offset);
	 * mmap will create a new mapping in the virtual address space
	 * of this process
	 * *addr - NULL which means the kernel will decide the starting
	 * address of the new mapping
	 * length - len (number of pages * page size) to be mapped
	 * flags - 
	 * 		PROT_READ | PROT_WRITE - pages can be read/written to
	 * 		MAP_PRIVATE - creates a private copy-on-write mapping
	 * 						not visible to other processes
	 * 		MAP_ANONYMOUS - mapping not backed by any file
	 * 			so fd is -1 and offset is set 0
	 * 
	 * Here if the mapping fails the value of MAP_FAILED / (void *) -1
	 * is returned.
	 * 
	 */
	addr = mmap(NULL, len, PROT_READ | PROT_WRITE,
		    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (addr == MAP_FAILED)
		errExit("mmap");

	printf("Address returned by mmap() = %p\n", addr);

	/* [M6]
	 * Explain following in here.
	 * 
	 * Now we configure the memory address details (start and length)
	 * and mode of operation to the userfaultfd fd with the 
	 * UFFDIO_REGISTER operation.
	 * The mode selected is UFFDIO_REGISTER_MODE_MISSING
	 * which means the userfaultfd will track page faults on missing
	 * pages.
	 * 
	 * Now the kernel will forward the page faults occurring in 
	 * this memory range to the user space application (this process).
	 */
	uffdio_register.range.start = (unsigned long) addr;
	uffdio_register.range.len = len;
	uffdio_register.mode = UFFDIO_REGISTER_MODE_MISSING;
	if (ioctl(uffd, UFFDIO_REGISTER, &uffdio_register) == -1)
		errExit("ioctl-UFFDIO_REGISTER");

	/* [M7]
	 * Explain following in here.
	 * 
	 * Here we are now creating a thread with the POSIX pthread library
	 * thr will store the ID of the created thread
	 * NULL value means the default attributes will be used
	 * then the third argument registers the function to be threaded
	 * the fourth argument, uffd, will be passed to the fault_handler_thread
	 * method
	 */
	s = pthread_create(&thr, NULL, fault_handler_thread, (void *) uffd);
	if (s != 0) {
		errno = s;
		errExit("pthread_create");
	}

	/*
	 * [U1]
	 * Briefly explain the behavior of the output that corresponds with below section.
	 * 
	 * OUTPUT IS:
	 * -----------------------------------------------------
	 * fault_handler_thread():
	 *     poll() returns: nready = 1; POLLIN = 1; POLLERR = 0
	 *     UFFD_EVENT_PAGEFAULT event: flags = 0; address = 7f3664ebc000
	 *         (uffdio_copy.copy returned 4096)
	 * #1. Read address 0x7f3664ebc000 in main(): A
	 * #1. Read address 0x7f3664ebc400 in main(): A
	 * #1. Read address 0x7f3664ebc800 in main(): A
	 * #1. Read address 0x7f3664ebcc00 in main(): A
	 * 
	 * 
	 * Now, here we are trying to access the mmapped memory region (with 
	 * printf and trying to print it to the output its content at every KB
	 * (1024 Bytes).
	 * We use a while loop which increments the "l" variable by 1KB and checks 
	 * whether it is less than the total length of the memory region
	 * (which is in this case 4KB (pagesize) as we ran this program with 
	 * num_pages as 1).
	 * We are printing the address value at every KB starting from base addr
	 * along with the value of byte in (character form) stored there.
	 * 
	 * Now, for this output, we are trying to access the newly mapped memory
	 * region first time, hence a pagefault occurred and execution of the 
	 * parent processed paused, (which can be seen with 
	 * the fault_handler_thread() statements in the output.)
	 * It shows that a pagefault occurred at address 0x7f3664ebc000 and the
	 * UFFDIO_COPY operation succeeded and copied 4096 Bytes in the memory
	 * region and woke up the parent process.
	 * Which then printed the memory contents at that addresses
	 * 'A' as for the first pagefault, fault_cnt is 0 
	 * hence the byte copied was 'A' + 0%20 => 'A' (65)
	 * 
	 */
	printf("-----------------------------------------------------\n");
	l = 0x0;
	while (l < len) {
		char c = addr[l];
		printf("#1. Read address %p in main(): ", addr + l);
		printf("%c\n", c);
		l += 1024;
	}

	/*
	 * [U2]
	 * Briefly explain the behavior of the output that corresponds with below section.
	 * 
	 * OUTPUT:
	 * -----------------------------------------------------
	 * #2. Read address 0x7f3664ebc000 in main(): A
	 * #2. Read address 0x7f3664ebc400 in main(): A
	 * #2. Read address 0x7f3664ebc800 in main(): A
	 * #2. Read address 0x7f3664ebcc00 in main(): A
	 * 
	 * Now that we try to access again the same memory region, pagefault did not 
	 * occur as the page was recently accessed (so it was already loaded in the memory).
	 * We can see from the output that the same 'A' value is printed.
	 */
	printf("-----------------------------------------------------\n");
	l = 0x0;
	while (l < len) {
		char c = addr[l];
		printf("#2. Read address %p in main(): ", addr + l);
		printf("%c\n", c);
		l += 1024;
	}

	/*
	 * [U3]
	 * Briefly explain the behavior of the output that corresponds with below section.
	 * 
	 * OUTPUT:
	 * -----------------------------------------------------
	 * 
	 * fault_handler_thread():
	 *     poll() returns: nready = 1; POLLIN = 1; POLLERR = 0
	 *     UFFD_EVENT_PAGEFAULT event: flags = 0; address = 7f3664ebc000
	 *         (uffdio_copy.copy returned 4096)
	 * #3. Read address 0x7f3664ebc000 in main(): B
	 * #3. Read address 0x7f3664ebc400 in main(): B
	 * #3. Read address 0x7f3664ebc800 in main(): B
	 * #3. Read address 0x7f3664ebcc00 in main(): B
	 * 
	 * Here we can see that a pagefault occurred while accessing the same location.
	 * Before accessing the memory, we called the madvise(addr, len, MADV_DONTNEED).
	 * This will advise the kernel about the memory location addr with length len
	 * that we are done with this memory and maybe wont need to access it in near
	 * future. This will cause the kernel to unload the pages, and if they are 
	 * accessed again, it will result in a pagefault.
	 * (with madvise we explicitly tried to get a pagefault).
	 * 
	 * Now as we tried to access the memory, the pagefault occurred and the
	 * kernel loaded the pages into the memory and then delivered the event to 
	 * the userfaultfd.
	 * As we can see from the fault_handler_thread output,
	 * now the memory location is filled with 'B's and 
	 * UFFDIO_COPY operation copied 4KB memory to the 7f3664ebc000.
	 * 'A' + 1%20 => 'B' (66)
	 */
	printf("-----------------------------------------------------\n");
	if (madvise(addr, len, MADV_DONTNEED)) {
		errExit("fail to madvise");
	}
	l = 0x0;
	while (l < len) {
		char c = addr[l];
		printf("#3. Read address %p in main(): ", addr + l);
		printf("%c\n", c);
		l += 1024;
	}

	/*
	 * [U4]
	 * Briefly explain the behavior of the output that corresponds with below section.
	 * 
	 * -----------------------------------------------------
	 * #4. Read address 0x7f3664ebc000 in main(): B
	 * #4. Read address 0x7f3664ebc400 in main(): B
	 * #4. Read address 0x7f3664ebc800 in main(): B
	 * #4. Read address 0x7f3664ebcc00 in main(): B
	 * 
	 * Here again now we are accessing the same region, but no pagefault
	 * (as again the page was recently accessed loaded and is in memory).
	 * Hence the same value 'B' is in the memory.
	 */
	printf("-----------------------------------------------------\n");
	l = 0x0;
	while (l < len) {
		char c = addr[l];
		printf("#4. Read address %p in main(): ", addr + l);
		printf("%c\n", c);
		l += 1024;
	}

	/*
	 * [U5]
	 * Briefly explain the behavior of the output that corresponds with below section.
	 * 
	 * OUTPUT:
	 * -----------------------------------------------------
	 * 
	 * fault_handler_thread():
	 *     poll() returns: nready = 1; POLLIN = 1; POLLERR = 0
	 *     UFFD_EVENT_PAGEFAULT event: flags = 1; address = 7f3664ebc000
	 *         (uffdio_copy.copy returned 4096)
	 * #5. write address 0x7f3664ebc000 in main(): @
	 * #5. write address 0x7f3664ebc400 in main(): @
	 * #5. write address 0x7f3664ebc800 in main(): @
	 * #5. write address 0x7f3664ebcc00 in main(): @
	 * 
	 * Here we first do madvise first so that the page is unloaded and a pagefault 
	 * will occur.
	 * 
	 * But before accessing the address with printf, we are doing memset on it
	 * So when memset will try to access that page, a pagefault will occur
	 * and it will wait till the pagefault is serviced.
	 * After the page is loaded in memory by the kernel, the 
	 * fault_handler_thread which polls for such event will copy 'C' bytes in
	 * at the memory location.
	 * But now, after the successful UFFDIO_COPY operation, memset will wake up
	 * and copy '@' at the memory location.
	 * Hence, now when we print the value, we see '@'.
	 */
	printf("-----------------------------------------------------\n");
	if (madvise(addr, len, MADV_DONTNEED)) {
		errExit("fail to madvise");
	}
	l = 0x0;
	while (l < len) {
		memset(addr+l, '@', 1024);
		printf("#5. write address %p in main(): ", addr + l);
		printf("%c\n", addr[l]);
		l += 1024;
	}

	/*
	 * [U6]
	 * Briefly explain the behavior of the output that corresponds with below section.
	 * 
	 * OUTPUT:
	 * -----------------------------------------------------
	 * #6. Read address 0x7f3664ebc000 in main(): @
	 * #6. Read address 0x7f3664ebc400 in main(): @
	 * #6. Read address 0x7f3664ebc800 in main(): @
	 * #6. Read address 0x7f3664ebcc00 in main(): @
	 * 
	 * Now when we try to access it again no pagefault occurred and the same
	 * content got printed at that addresses => '@'.
	 */
	printf("-----------------------------------------------------\n");
	l = 0x0;
	while (l < len) {
		char c = addr[l];
		printf("#6. Read address %p in main(): ", addr + l);
		printf("%c\n", c);
		l += 1024;
	}

	/*
	 * [U7]
	 * Briefly explain the behavior of the output that corresponds with below section.
	 * 
	 * -----------------------------------------------------
	 * #7. write address 0x7f3664ebc000 in main(): ^
	 * #7. write address 0x7f3664ebc400 in main(): ^
	 * #7. write address 0x7f3664ebc800 in main(): ^
	 * #7. write address 0x7f3664ebcc00 in main(): ^
	 * 
	 * Here we are doing memset with '^' but as now again the page was already 
	 * loaded in the memory, no pagefault occurred and the memset operation
	 * straightaway filled the memory with '^'.
	 * And when we printed it, we got the same output.
	 */
	printf("-----------------------------------------------------\n");
	l = 0x0;
	while (l < len) {
		memset(addr+l, '^', 1024);
		printf("#7. write address %p in main(): ", addr + l);
		printf("%c\n", addr[l]);
		l += 1024;
	}

	/*
	 * [U8]
	 * Briefly explain the behavior of the output that corresponds with below section.
	 * 
	 * -----------------------------------------------------
	 * #8. Read address 0x7f3664ebc000 in main(): ^
	 * #8. Read address 0x7f3664ebc400 in main(): ^
	 * #8. Read address 0x7f3664ebc800 in main(): ^
	 * #8. Read address 0x7f3664ebcc00 in main(): ^
	 * 
	 * And again as per the output no pagefault occurred (as the page already loaded
	 * in the memory) and we got the content '^' printed in the output.
	 */
	printf("-----------------------------------------------------\n");
	l = 0x0;
	while (l < len) {
		char c = addr[l];
		printf("#8. Read address %p in main(): ", addr + l);
		printf("%c\n", c);
		l += 1024;
	}

	exit(EXIT_SUCCESS);
}
