/*********************************************************************/
/* utils.h - Generic utilities                                       */
/*                                                                   */
/* Copyright (C) 1996-1996 MB Computrex           Released under GPL */
/*********************************************************************/

#if !defined(__UTILS_H)
#define __UTILS_H

#define FOUND_NONE	-1
#define FOUND_AMBIGOUS	-2

#define FGETLINE_BUFLEN	4096
#define COPY_BUFSIZE	8192

/* Prototypes */
extern	char *fgetline(FILE *, int *);
extern	void fputline(FILE *, const char *);
extern	BOOLEAN check_file(const char *);
extern	char *get_unique_tmpname();
extern	BOOLEAN	delete_file(const char *);
extern	BOOLEAN	copy_file(const char *, const char *);
extern	int detach();
extern	int stripblank();
extern	char *strcatf(char *dst, const char *fmt, ...);
extern	BOOLEAN str_isdigit(char *str);
extern	void safe_free(void *);
extern	void *safe_malloc(size_t len);
extern	char *str_redup(char *str, const char *newstr);

#endif
