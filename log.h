/*********************************************************************/
/* log.h - Logging utilities                                         */
/*                                                                   */
/* Copyright (C) 1996-2004 MB Computrex           Released under GPL */
/*********************************************************************/

#if !defined(_LOG_H)
#define _LOG_H

/* Error levels */
enum ErrorLevel {
  ERR_FATAL=0, ERR_CRIT, ERR_ERROR, ERR_WARNING, ERR_NOTICE, ERR_INFO
};

#if defined(TRUE) || defined(FALSE)
typedef int BOOLEAN;
#else
enum BOOLEAN {FALSE=0, TRUE};
typedef enum BOOLEAN BOOLEAN;
#endif

#define USE_STDERR 1	/* Bit 0 */
#define USE_SYSLOG 2	/* Bit 1 */
#define TIMESTAMP  4	/* Bit 2 */

extern void log_msg(enum ErrorLevel, const char *, ...);
extern void log_debug(int level, const char *, ...);

extern void logger_set_prefix(const char *stc);
extern void logger_set_level(int);
extern void logger_set_options(int);
extern void logger_set_logfile(const char *);

extern void logger_initialize(void);
extern void logger_shutdown(void);


#endif
