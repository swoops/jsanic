#ifndef ERROR

#ifndef EOF
	#define EOF -1
#else
	#if EOF != -1
		#error EOF != -1
	#endif
#endif

#define ERROR -2
#define IOERROR -3
#define FILEACCESS -4
#define MALLOCFAIL -5
#define ERROR_PTHREAD_FAIL -5

#endif
