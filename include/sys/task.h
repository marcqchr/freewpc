
#ifndef _SYS_TASK_H
#define _SYS_TASK_H

#include <env.h>
#include <sys/types.h>

#define TASK_FREE		0
#define TASK_USED		1
#define TASK_BLOCKED	2

#define NUM_TASKS				16

#define TASK_STACK_SIZE		43

typedef uint8_t task_gid_t;

typedef uint8_t task_ticks_t;

typedef void (*task_function_t) (uint16_t) __NORETURN__;

typedef struct
{
	task_gid_t		gid;
	uint8_t        unused;
	uint16_t			pc;
	uint16_t			next;
	uint16_t			x;
	uint16_t			y;
	uint16_t			s;
	uint16_t			u;
	uint8_t			delay;
	uint8_t			asleep;
	uint8_t			state;
	uint8_t			a;
	uint8_t			b;
	uint16_t			arg;
	uint8_t			stack[TASK_STACK_SIZE];
} task_t;

typedef task_t *task_pid_t;

/********************************/
/*     Function Prototypes      */
/********************************/

void task_init (void);
void task_yield (void);
void task_create (task_function_t fn, uint16_t arg);
void task_create_gid (task_gid_t, task_function_t fn, uint16_t arg);
void task_create_gid1 (task_gid_t, task_function_t fn, uint16_t arg);
void task_recreate_gid (task_gid_t, task_function_t fn, uint16_t arg);
task_gid_t task_getgid (void);
void task_setgid (task_gid_t gid);
void task_sleep (task_ticks_t ticks);
void task_exit (void) __NORETURN__;
task_t *task_find_gid (task_gid_t);
void task_kill_gid (task_gid_t);

#endif /* _SYS_TASK_H */
