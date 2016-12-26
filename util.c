#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

void *xcalloc(size_t nmemb, size_t size, const char *file, const int line, const char *func) {
	void *ptr = calloc(nmemb, size);
	if(ptr == NULL) {
		fprintf(stderr, "%s:%d: %s(): calloc(%lu, %lu) failed: %s\n",
			file, line, func, nmemb, size, strerror(errno));
		_exit(1);
	}
	return ptr;
}

void *xrealloc(void *ptr, size_t size, const char *file, const int line, const char *func) {
	ptr = realloc(ptr, size);
	if(ptr == NULL) {
		fprintf(stderr, "%s:%d: %s(): realloc(%lu) failed: %s\n",
			file, line, func, size, strerror(errno));
		_exit(1);
	}
	return ptr;
}
