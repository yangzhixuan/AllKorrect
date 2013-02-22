#include "Execute.h"
#include <unistd.h>
#include <dirent.h>
#include <sys/wait.h>
#include <sys/signal.h>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/user.h>
#include <cstdio>
#include <stdexcept>
#include <cmath>
#include <cassert>
#include <cstring>
#include <cerrno>
#include <cstdlib>
#include "Log.h"

namespace Execute {
static const double REALTIME_RATE = 1.5;
static const double MEMORY_LIMIT_RATE = 2;

uid_t NobodyUID;
gid_t NogroupGID;

static bool hasExec;

enum executeError {
	ERR_INVALID_SYSCALL, ERR_MLE
};

static const int ALLOWED_SYSCALL[] =
		{ SYS_getxattr, SYS_access, SYS_brk, SYS_close, SYS_execve,
				SYS_exit_group, SYS_fstat, SYS_futex, SYS_getrlimit, SYS_ioctl,
				SYS_ioperm, SYS_mmap, SYS_open, SYS_rt_sigaction,
				SYS_rt_sigprocmask, SYS_set_robust_list, SYS_set_thread_area,
				SYS_set_tid_address, SYS_stat, SYS_uname, SYS_write, SYS_read,
				SYS_mprotect, SYS_arch_prctl, SYS_munmap, SYS_clone };
static const int ALLOWED_SYSCALL_LOOSE[] = { SYS_readlink,
		SYS_openat/*Should Check it?*/, SYS_getdents, SYS_getgid, SYS_getegid,
		SYS_getuid, SYS_geteuid, SYS_setrlimit, SYS_lstat, SYS_vfork, SYS_wait4,
		SYS_unlink, SYS_getpid, SYS_writev };

static const char* ALLOWED_OPEN[] = { "/usr/", "/lib/", "/lib64/", "/etc/", "/proc/"};
static const char* ALLOWED_OPEN_LOOSE[] = {  "/sys/", "/tmp/" };

static const struct Arg* arg;
static struct Result* result;
static pid_t pid;

static void setRLimits(const struct Limit* limit) {

	struct rlimit rlimit;

	//File Size
	//SIGXFSZ
	if (limit->outputLimit >= 0) {
		rlimit.rlim_cur = rlimit.rlim_max = limit->outputLimit;
		setrlimit(RLIMIT_FSIZE, &rlimit);
	}

	//Total Memory
	//Doubled
	if (limit->memoryLimit >= 0) {
		rlimit.rlim_cur = rlimit.rlim_max = limit->memoryLimit
				* MEMORY_LIMIT_RATE;
		setrlimit(RLIMIT_AS, &rlimit);
	}

	//No Core File
	rlimit.rlim_cur = rlimit.rlim_max = 0;
	setrlimit(RLIMIT_CORE, &rlimit);

	//Execute Time
	//To send SIGXCPU
	if (limit->timeLimit >= 0) {
		rlimit.rlim_cur = ceil(limit->timeLimit / 1000.0);
		//To SIGKILL the program when he ignored SIGXCPU
		rlimit.rlim_max = rlimit.rlim_cur + 1;
		setrlimit(RLIMIT_CPU, &rlimit);
	}

	//NICE
	//The real limit set is 20 - rlim_cur
	rlimit.rlim_cur = rlimit.rlim_max = 20;
	setrlimit(RLIMIT_NICE, &rlimit);

	//Number of processes
	if (limit->processLimit >= 0) {
		rlimit.rlim_cur = rlimit.rlim_max = limit->processLimit;
		setrlimit(RLIMIT_NPROC, &rlimit);
	}
}

static void doChild() {
	//Set gid & uid
	setgid(arg->gid);
	setuid(arg->uid);

	//Set work directory
	chdir(arg->cwd);

	//Set limits
	setRLimits(&arg->limit);

	//Redirect standard I/O
	freopen(arg->inputFile, "r", stdin);
	freopen(arg->outputFile, "w", stdout);
	freopen(arg->errorFile, "w", stderr);

	//Trace Me!
	ptrace(PTRACE_TRACEME, 0, NULL, NULL);

	execvp(arg->command, arg->argv);
	exit(-1);
}

int getMemoryUsed(pid_t pid) {
	FILE *fps;
	char ps[32];
	int memory;

	sprintf(ps, "/proc/%d/statm", pid);
	fps = fopen(ps, "r");
	int i;
	for (i = 0; i < 6; i++)
		fscanf(fps, "%d", &memory);
	fclose(fps);

	int pagesize = getpagesize();
	memory *= pagesize;
	return memory;
}

static bool isAllNumberic(const char* s) {
	for (; *s; s++) {
		if (!('0' <= *s && *s <= '9'))
			return 0;
	}
	return 1;
}

static void killTree(pid_t pid) {
	DIR* proc = opendir("/proc");
	if (proc) {
		struct dirent * sub;
		while ((sub = readdir(proc)) != NULL) {
			if (sub->d_type == DT_DIR && isAllNumberic(sub->d_name)) {
				char statName[100];
				strcpy(statName, "/proc/");
				strcat(statName, sub->d_name);
				strcat(statName, "/stat");
				FILE* statFile = fopen(statName, "r");
				if (statFile) {
					int ppid;
					fscanf(statFile, "%*s%*s%*s%d", &ppid);
					fclose(statFile);
					if (ppid == pid) {
						killTree(atoi(sub->d_name));
					}
				}
			}
		}
		closedir(proc);
	}
	kill(pid, SIGKILL);
}

static bool isSyscallAllowed(int syscall) {
	for (int callno : ALLOWED_SYSCALL) {
		if (callno == syscall) {
			return true;
		}
	}
	return false;
}

static bool isLooseSyscallAllowed(int syscall) {
	for (int callno : ALLOWED_SYSCALL_LOOSE) {
		if (callno == syscall) {
			return true;
		}
	}
	//ERR("Forbi: %d", syscall);
	return false;
}

static std::string peekString(pid_t pid, char*addr) {
	union {
		long l;
		char c[sizeof(long)];
	} peeker;
	int i, j;

	std::string result;

	for (i = 0;; i++) {
		peeker.l = ptrace(PTRACE_PEEKDATA, pid, addr + sizeof(long) * i, NULL);
		for (j = 0; j < (int) sizeof(long); j++) {
			if (peeker.c[j] == 0) {
				return result;
			}
			result.push_back(peeker.c[j]);
		}
	}
}

static bool checkOpen(std::string path) {
	for (const char* allowed : ALLOWED_OPEN) {
		if (strlen(allowed) <= path.size()
				&& path.substr(0, strlen(allowed)) == allowed)
			return true;
	}
	return false;
}

static bool checkLooseOpen(std::string path) {
	for (const char* allowed : ALLOWED_OPEN_LOOSE) {
		if (strlen(allowed) <= path.size()
				&& path.substr(0, strlen(allowed)) == allowed)
			return true;
	}
	return false;
}

static bool checkSyscall(pid_t pid) {
	struct user_regs_struct regs;
	ptrace(PTRACE_GETREGS, pid, NULL, &regs);

	long syscall = regs.orig_rax;
	/*
	 printf("Syscall(Ret=%ld): %ld (%ld,%ld,%ld)\n", regs.rax, syscall, regs.rdi,
	 regs.rsi, regs.rdx);
	 if (syscall == SYS_access) {
	 char buf[100];
	 puts(peekString(pid, buf, 100, (char*) regs.rdi));
	 } else if (syscall == SYS_write) {
	 char buf[100];
	 puts(peekString(pid, buf, 100, (char*) regs.rsi));
	 }
	 */

	if (arg->limit.limitSyscall && !isSyscallAllowed(syscall)
			|| !arg->limit.limitSyscall && !isSyscallAllowed(syscall)
					&& !isLooseSyscallAllowed(syscall)) {
		ERR("Caught forbidden syscall %ld", syscall);
		errno = ERR_INVALID_SYSCALL;
		return 0;
	}

	std::string openingFile;
	switch (syscall) {
	case SYS_open:
		openingFile = peekString(pid, (char*) regs.rdi);
		if (arg->limit.limitSyscall && !checkOpen(openingFile)
				|| !arg->limit.limitSyscall && !checkOpen(openingFile)
						&& !checkLooseOpen(openingFile)) {
			ERR("Caught opening forbidden file %s", openingFile.c_str());
			errno = ERR_INVALID_SYSCALL;
			return false;
		}
		break;
	case SYS_execve:
		if (arg->limit.limitSyscall && hasExec) {
			ERR("Try to exec again");
			errno = ERR_INVALID_SYSCALL;
			return false;
		}
		hasExec = true;
		break;
	case SYS_brk:
	case SYS_mmap:
	case SYS_munmap:
		result->memory = getMemoryUsed(pid);
		if (arg->limit.memoryLimit >= 0
				&& result->memory > arg->limit.memoryLimit) {
			errno = ERR_MLE;
			return 0;
		}
		break;
	}

	return 1;
}

static void alarmHandler(int signo) {
	kill(pid, SIGUSR1);
	alarm(1);
}

static void parentLoop(pid_t pid) {
	for (;;) {
		struct rusage rusage;
		int status;

		//Wait for my child
		wait4(pid, &status, 0, &rusage);

		if (result->type != UNKNOWN) {
			assert(WIFEXITED(status) || WIFSIGNALED(status));
			return;
		}

		result->time = rusage.ru_utime.tv_sec * 1000
				+ rusage.ru_utime.tv_usec / 1000;

		if (arg->limit.timeLimit >= 0 && result->time > arg->limit.timeLimit) {
			result->type = TLE;
			killTree(pid);
		}

		if (WIFEXITED(status)) {
			int exitStatus = WEXITSTATUS(status);
			result->exitStatus = exitStatus;
			if (exitStatus == 0) {
				result->type = SUCCESS;
			} else {
				result->type = FAILURE;
			}
			return;
		} else if (WIFSIGNALED(status)) {
			//assert(WTERMSIG(status)==SIGKILL);
			result->type = CRASHED;
			result->exitStatus = WTERMSIG(status);
			return;
		} else if (WIFSTOPPED(status)) {
			int signo = WSTOPSIG(status);
			switch (signo) {
			case SIGURG:
			case SIGCHLD:
			case SIGWINCH:
				//Ignore
				break;
			case SIGTRAP:
				//He invoked a syscall
				if (!checkSyscall(pid)) {
					switch (errno) {
					case ERR_INVALID_SYSCALL:
						result->type = VIOLATION;
						break;
					case ERR_MLE:
						result->type = MLE;
						break;
					}
					killTree(pid);
				}
				break;
			case SIGXFSZ:
				result->type = OLE;
				killTree(pid);
				break;
			case SIGXCPU:
				result->type = TLE;
				killTree(pid);
				break;
			case SIGUSR1:
				//Real time too long
				result->type = TLE;
				killTree(pid);
				break;
			case SIGSEGV:
				result->type = MEM_VIOLATION;
				killTree(pid);
				break;
			case SIGFPE:
				result->type = MATH_ERROR;
				killTree(pid);
				break;
			default:
				result->type = CRASHED;
				killTree(pid);
				break;
			}
		} else {
			ERR("Not End or Stop!");
			killTree(pid);
		}

		ptrace(PTRACE_SYSCALL, pid, NULL, NULL);
	}
}

void Execute(const struct Arg* _arg, struct Result* _result) {
	arg = _arg;
	result = _result;

	memset(result, 0, sizeof(struct Result));
	result->type = UNKNOWN;
	hasExec = false;

	pid = fork();
	if (pid == 0) {
		doChild();
	} else {
		//Real Time Alarm to prevent infinite sleep
		if (arg->limit.timeLimit >= 0) {
			signal(SIGALRM, alarmHandler);
			alarm(ceil(REALTIME_RATE * arg->limit.timeLimit / 1000.0));
		}
		parentLoop(pid);
		alarm(0);
	}
}

void Init() {
	struct passwd *nobody = getpwnam("nobody");
	if (nobody == NULL) {
		throw std::runtime_error("Cannot find 'nobody'.");
	}
	NobodyUID = nobody->pw_uid;
	LOG("Got uid of nobody=%d", NobodyUID);

	struct group* nogroup = getgrnam("nogroup");
	if (nogroup == NULL) {
		ERR("Cannot find 'nogroup' let's find 'nobody' group.");
		nogroup = getgrnam("nobody");
		if (nogroup == NULL) {
			throw std::runtime_error(
					"Cannot find 'nogroup' or 'nobody' group.");
		}
	}
	NogroupGID = nogroup->gr_gid;
	LOG("Got gid of nogroup=%d", NogroupGID);
}
}
