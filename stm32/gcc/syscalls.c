/* newlib 系统调用桩：printf 输出转到工程里已有的 fputc（USART1） */
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>

int _write(int fd, const char *buf, int len)
{
	int i;
	(void)fd;
	for (i = 0; i < len; i++)
		fputc(buf[i], stdout);
	return len;
}

void *_sbrk(int incr)
{
	extern char _end, _estack;
	static char *heap = &_end;
	char *prev = heap;
	uintptr_t limit = (uintptr_t)&_estack - 2048;   /* 为栈保留 2KB */
	if ((uintptr_t)(heap + incr) > limit) {
		errno = ENOMEM;
		return (void *)-1;
	}
	heap += incr;
	return prev;
}

int _close(int fd)                 { (void)fd; return -1; }
int _fstat(int fd, struct stat *s) { (void)fd; s->st_mode = S_IFCHR; return 0; }
int _isatty(int fd)                { (void)fd; return 1; }
int _lseek(int fd, int o, int w)   { (void)fd; (void)o; (void)w; return 0; }
int _read(int fd, char *b, int n)  { (void)fd; (void)b; (void)n; return 0; }
int _getpid(void)                  { return 1; }
int _kill(int pid, int sig)        { (void)pid; (void)sig; errno = EINVAL; return -1; }
void _exit(int code)               { (void)code; while (1); }
