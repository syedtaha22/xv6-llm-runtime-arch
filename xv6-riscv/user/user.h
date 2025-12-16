#ifdef LAB_MMAP
typedef unsigned long size_t;
typedef long int off_t;
#endif

//for variable argument functions (sprintf, sscanf)
#include<stdarg.h>

#define SBRK_ERROR ((char *)-1)

struct stat;

// system calls
int fork(void);
int exit(int) __attribute__((noreturn));
int wait(int*);
int pipe(int*);
int write(int, const void*, int);
int read(int, void*, int);
int close(int);
int kill(int);
int exec(const char*, char**);
int open(const char*, int);
int mknod(const char*, short, short);
int unlink(const char*);
int fstat(int fd, struct stat*);
int link(const char*, const char*);
int mkdir(const char*);
int chdir(const char*);
int dup(int);
// User stubs for reading cycle, time, and instret CSRs
uint64 rdcycle(void);
uint64 rdtime(void);
uint64 rdinstret(void);
int getpid(void);
char* sys_sbrk(int,int);
int pause(int);
int uptime(void);
#ifdef LAB_NET
int bind(uint16);
int unbind(uint16);
int send(uint16, uint32, uint16, char *, uint32);
int recv(uint16, uint32*, uint16*, char *, uint32);
#endif
#ifdef LAB_PGTBL
int ugetpid(void);
uint64 pgpte(void*);
void kpgtbl(void);
#endif

// Shared memory functions
#define SHM_PERSIST 0x01
#define SHM_RDONLY 0x01
#define SHM_RDWR 0x02
#define IPC_CREAT 0x1000
#define IPC_EXCL 0x2000
#define IPC_RMID 0
int shmget(const char*, uint64, int);
void* shmat(int, void*, int);
int shmdt(void*);
int shmctl(int, int, void*);

// RAM usage function
uint64 getramused(void);

int yield(void);

// Threads and mutexes
int thread_create(void (*start_routine)(void*), void *arg);
int thread_join(int thread_id);
void thread_exit(void);

int mutex_init(int *mutex);
void mutex_lock(int *mutex);
void mutex_unlock(int *mutex);

// ulib.c
int stat(const char*, struct stat*);
char* strcpy(char*, const char*);
void *memmove(void*, const void*, int);
char* strchr(const char*, char c);
int strcmp(const char*, const char*);
char* gets(char*, int max);
uint strlen(const char*);
void* memset(void*, int, uint);
int atoi(const char*);
int memcmp(const void *, const void *, uint);
void *memcpy(void *, const void *, uint);
char* sbrk(int);
char* sbrklazy(int);
// String utility prototypes for milestone 2
int xsprintf(char *out, const char *fmt, ...);
int xsnprintf(char *out, int size, const char *fmt, ...);
int xvsnprintf(char *out, int size, const char *fmt, va_list ap);
int xsscanf(const char *s, const char *fmt, ...);
int xisprint(int c);
int xisspace(int c);
#ifdef LAB_LOCK
int statistics(void*, int);
#endif

// printf.c
void fprintf(int, const char*, ...) __attribute__ ((format (printf, 2, 3)));
void printf(const char*, ...) __attribute__ ((format (printf, 1, 2)));
void vprintf(int fd, const char *fmt, va_list ap);
void printf_set_indent(int);
void printf_reset_indent(void);


// umalloc.c
void* malloc(uint);
void free(void*);
