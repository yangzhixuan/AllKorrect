#pragma once
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
namespace Execute{
struct Limit{
	//In bytes
	long long memoryLimit;

	//In bytes
	long long outputLimit;

	//In ms
	int timeLimit;

	int processLimit;
	bool limitSyscall;
};

struct Arg{
	const char* command;
	char* const* argv;
	const char* cwd;
	const char* inputFile, *outputFile, *errorFile;
	uid_t uid;
	gid_t gid;
	struct Limit limit;
};

enum ResultType{
	UNKNOWN=-1,
	SUCCESS,
	FAILURE,
	CRASHED,
	TLE,
	MLE,
	OLE,
	VIOLATION,
	MATH_ERROR,
	MEM_VIOLATION,
};

struct Result{
	enum ResultType type;
	int exitStatus;
	int time;
	long long memory;
};

extern uid_t NobodyUID;
extern gid_t NogroupGID;

extern void Execute(const struct Arg* arg,struct Result* result);
extern void Init();
}
