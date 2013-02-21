#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include "Execute.h"
#include "Log.h"
#include "Daemon.h"
#include "FileSystem.h"

#ifndef __x86_64__
#error "AllKorrect is designed for x64 only"
#endif

int main(void) {
	LOG("AllKorrect Starting up");
	LOG("My uid=%d euid=%d gid=%d egid=%d",
			getuid(), geteuid(), getgid(), getegid());

	if (geteuid() != 0) {
		throw std::runtime_error("AllKorrect must be run as root.");
	}

	Daemon::Init();
	Execute::Init();
	FileSystem::Init();

	//DBG("rnd: %s",RandString());

	Daemon::Run();
	//FileSystem::RecursiveRemove("/var/cache/allkorrect");
/*
	const char* cmd = "./test";
	char* const argv[] = { (char*) cmd, "Main", NULL };
	struct Execute::Arg arg;
	arg.command = cmd;
	arg.argv = argv;
	arg.cwd = "/home/phone";
	arg.inputFile = "/dev/null";
	arg.outputFile = "/tmp/out";
	arg.errorFile = "/tmp/err";
	arg.uid = Execute::NobodyUID;
	arg.gid = Execute::NogroupGID;
	arg.limit.memoryLimit = 1024 * 1024 * 1024;
	arg.limit.timeLimit = 2000;
	arg.limit.outputLimit = 100 * 1024 * 1024;
	arg.limit.processLimit = 1;
	arg.limit.limitSyscall = true;

	struct Execute::Result result;
	Execute::Execute(&arg, &result);
	DBG("exitStatus:%d", result.exitStatus);
	DBG("type:%d", result.type);
	DBG("time:%d", result.time);
	DBG("memory:%lld", result.memory);
*/

	LOG("AllKorrect stopped.");
	return EXIT_SUCCESS;
}
