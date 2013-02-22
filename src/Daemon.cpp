#include "Daemon.h"
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <unistd.h>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <functional>
#include "Log.h"
#include "Message.h"
#include "FileSystem.h"
#include "Defer.h"
#include "BinaryStream.h"

namespace Daemon {
static const short PORT = 10010;
static const int MAX_PENDING = 5;

static bool Running;

static void interruptHandler(int signo) {
	LOG("Caught SIGINT! Stopping.");
	Running = false;
}

void Init() {
	struct sigaction sigact;
	memset(&sigact, 0, sizeof(struct sigaction));
	sigact.sa_handler = interruptHandler;
	sigaddset(&sigact.sa_mask, SIGQUIT);
	sigact.sa_flags = SA_RESETHAND;
	if (sigaction(SIGINT, &sigact, NULL) < 0) {
		throw std::runtime_error("Cannot register SIGINT handler");
	}
	LOG("Registered SIGINT handler");
}

void dealExec(int sock, std::string tmpDir, Message::Message& msg) {
	Message::MsgExec exec = Message::ToMsgExec(msg);

	//Input
	bool hasInput = !exec.input.empty();
	if (hasInput) {
		FileSystem::CheckString(exec.input);
		if (!FileSystem::HasBlob(FileSystem::Root + exec.input)) {
			throw std::runtime_error("Input blob not found");
		}
		exec.input = FileSystem::Root + exec.input;
		FileSystem::SetBlobReadOnly(exec.input);
	}

	//Output and Error
	std::string out = FileSystem::RandString(), err = FileSystem::RandString();
	std::string output = FileSystem::Root + out, error = FileSystem::Root + err;
	FileSystem::NewBlob(output);
	FileSystem::NewBlob(error);
	FileSystem::SetBlobWriteOnly(output);
	FileSystem::SetBlobWriteOnly(error);
	Defer restorePermissions([=]() {
		if(hasInput) {
			FileSystem::RestoreBlobPermission(exec.input);
		}
		FileSystem::RestoreBlobPermission(output);
		FileSystem::RestoreBlobPermission(error);
	});

	std::vector<char*> argv;
	std::string cmdLine = exec.cmd;
	argv.push_back((char*) exec.cmd.c_str());
	for (int i = 0; i < exec.argc; i++) {
		argv.push_back(const_cast<char*>(exec.arg[i].c_str()));
		cmdLine += " " + exec.arg[i];
	}
	LOG("EXEC %s", cmdLine.c_str());
	argv.push_back(NULL);

	Execute::Arg arg;
	arg.command = exec.cmd.c_str();
	arg.cwd = tmpDir.c_str();
	arg.argv = &argv[0];
	if (hasInput) {
		arg.inputFile = exec.input.c_str();
	} else {
		arg.inputFile = "/dev/null";
	}
	arg.outputFile = output.c_str();
	arg.errorFile = error.c_str();
	arg.gid = Execute::NogroupGID;
	arg.uid = Execute::NobodyUID;
	arg.limit.timeLimit = exec.timeLimit;
	arg.limit.memoryLimit = exec.memoryLimit;
	arg.limit.outputLimit = exec.outputLimit;

	switch (exec.restriction) {
	case Message::STRICT:
		arg.limit.limitSyscall = true;
		arg.limit.processLimit = 1;
		break;
	case Message::LOOSE:
		arg.limit.limitSyscall = false;
		arg.limit.processLimit = 20;
		break;
	}

	Execute::Result execResult;
	Execute::Execute(&arg, &execResult);
	Message::MsgExecReply reply;
	reply.error = err;
	reply.output = out;
	reply.exitStatus = execResult.exitStatus;
	reply.memory = execResult.memory;
	reply.time = execResult.time;
	reply.type = execResult.type;
	Message::Send(sock, Message::FromMsgExecReply(reply));
}

void dealPutBlob(int sock, std::string tmpDir, Message::Message& msg) {
	Message::MsgPutBlob putBlob = Message::ToMsgPutBlob(msg);
	LOG("PUT_BLOB %s", putBlob.name.c_str());
	FileSystem::CheckString(putBlob.name);
	FileSystem::PutBlob(FileSystem::Root + putBlob.name, putBlob.buf,
			putBlob.len);
	Message::Message reply;
	reply.type = Message::OK;
	reply.size = 0;
	Message::Send(sock, reply);
}

void dealGetBlob(int sock, std::string tmpDir, Message::Message& msg) {
	Message::MsgGetBlob getBlob = Message::ToMsgGetBlob(msg);
	LOG("GET_BLOB %s", getBlob.name.c_str());
	FileSystem::CheckString(getBlob.name);
	if (!FileSystem::HasBlob(FileSystem::Root + getBlob.name)) {
		throw std::runtime_error("Blob not exists");
	}
	Message::Message reply;
	reply.body = FileSystem::GetBlob(FileSystem::Root + getBlob.name);
	reply.type = Message::GET_BLOB_REPLY;
	reply.size = reply.body.size();
	Message::Send(sock, reply);
}

void dealCopyMove(int sock, std::string tmpDir, Message::Message& msg,
		void (*func)(std::string, std::string),
		std::function<std::string(std::string)> oldToFull,
		std::function<std::string(std::string)> newToFull) {
	Message::MsgCopyMove copyMove = Message::ToMsgCopyMove(msg);
	FileSystem::CheckString(copyMove.oldName);
	FileSystem::CheckString(copyMove.newName);
	func(oldToFull(copyMove.oldName), newToFull(copyMove.newName));
	Message::Message reply;
	reply.type = Message::OK;
	reply.size = 0;
	Message::Send(sock, reply);
}

void dealHasBlob(int sock, std::string tmpDir, Message::Message& msg) {
	BinaryStream stream(msg.body);
	std::string name;
	stream >> name;
	LOG("HAS_BLOB %s", name.c_str());
	FileSystem::CheckString(name);
	Message::Message reply;
	BinaryStream buf;
	if (FileSystem::HasBlob(FileSystem::Root + name)) {
		buf << (int) 1;
	} else {
		buf << (int) 0;
	}
	reply.type = Message::HAS_BLOB_REPLY;
	reply.body = buf.buffer;
	reply.size = reply.body.size();
	Message::Send(sock, reply);
}

void dealHasFile(int sock, std::string tmpDir, Message::Message& msg) {
	BinaryStream stream(msg.body);
	std::string name;
	stream >> name;
	LOG("HAS_FILE %s", name.c_str());
	FileSystem::CheckString(name);
	Message::Message reply;
	BinaryStream buf;
	if (FileSystem::HasBlob(tmpDir + name)) {
		buf << (int) 1;
	} else {
		buf << (int) 0;
	}
	reply.type = Message::HAS_FILE_REPLY;
	reply.body = buf.buffer;
	reply.size = reply.body.size();
	Message::Send(sock, reply);
}

std::string toFullBlob(std::string a) {
	return FileSystem::Root + a;
}

std::function<std::string(std::string)> toFullFile(std::string tmpDir) {
	return [=](std::string a) {
		return tmpDir+a;
	};
}

void serve(int sock) {
	Defer sockCloser([=]() {
		LOG("Client socket closed.");
		close(sock);
	});

	//Create a temp directory
	std::string tmpDir = FileSystem::NewTmpDir();
	LOG("Use tmp dir %s", tmpDir.c_str());

	Defer rmTmpDir([=]() {
		FileSystem::RecursiveRemove(tmpDir);
	});

	for (;;) {
		Message::Message msg = Message::Next(sock);

		switch (msg.type) {
		case Message::EXIT:
			goto EndSession;
		case Message::EXEC:
			dealExec(sock, tmpDir, msg);
			break;
		case Message::PUT_BLOB:
			dealPutBlob(sock, tmpDir, msg);
			break;
		case Message::GET_BLOB:
			dealGetBlob(sock, tmpDir, msg);
			break;
		case Message::HAS_BLOB:
			dealHasBlob(sock, tmpDir, msg);
			break;
		case Message::HAS_FILE:
			dealHasFile(sock, tmpDir, msg);
			break;
		case Message::MOVE_BLOB2FILE:
			dealCopyMove(sock, tmpDir, msg, FileSystem::MoveBlob2File,
					toFullBlob, toFullFile(tmpDir));
			break;
		case Message::MOVE_BLOB2BLOB:
			dealCopyMove(sock, tmpDir, msg, FileSystem::MoveBlob2Blob,
					toFullBlob, toFullBlob);
			break;
		case Message::MOVE_FILE2FILE:
			dealCopyMove(sock, tmpDir, msg, FileSystem::MoveFile2File,
					toFullFile(tmpDir), toFullFile(tmpDir));
			break;
		case Message::MOVE_FILE2BLOB:
			dealCopyMove(sock, tmpDir, msg, FileSystem::MoveFile2Blob,
					toFullFile(tmpDir), toFullBlob);
			break;
		case Message::COPY_BLOB2FILE:
			dealCopyMove(sock, tmpDir, msg, FileSystem::CopyBlob2File,
					toFullBlob, toFullFile(tmpDir));
			break;
		case Message::COPY_BLOB2BLOB:
			dealCopyMove(sock, tmpDir, msg, FileSystem::CopyBlob2Blob,
					toFullBlob, toFullBlob);
			break;
		case Message::COPY_FILE2FILE:
			dealCopyMove(sock, tmpDir, msg, FileSystem::CopyFile2File,
					toFullFile(tmpDir), toFullFile(tmpDir));
			break;
		case Message::COPY_FILE2BLOB:
			dealCopyMove(sock, tmpDir, msg, FileSystem::CopyFile2Blob,
					toFullFile(tmpDir), toFullBlob);
			break;
		default:
			throw std::runtime_error("Unknown message type.");
		}
	}
	EndSession:
	LOG("Client normal exit");
}

void Run() {
	LOG("Starting up daemon");
	LOG("Listening at %d", PORT);

	Running = true;

	int sock;
	if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		throw std::runtime_error("Cannot create server socket");
	}

	struct sockaddr_in serverAddr;
	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = INADDR_ANY;
	serverAddr.sin_port = htons(PORT);
	if (bind(sock, (struct sockaddr*) &serverAddr, sizeof(serverAddr)) < 0) {
		throw std::runtime_error("Cannot bind server socket");
	}

	if (listen(sock, MAX_PENDING) < 0) {
		throw std::runtime_error("Cannot listen");
	}

	LOG("Successfully listened.");
	while (Running) {
		int client;
		struct sockaddr_in clientAddr;
		socklen_t sockLen;

		LOG("Waiting for the next client.");
		client = accept(sock, (struct sockaddr*) &clientAddr, &sockLen);
		if (client < 0) {
			if (!Running)
				break;
			else
				throw std::runtime_error("Accept failure");
		}
		LOG("Client connected from %s:%hu",
				inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));

		try {
			struct timeval timeout;
			timeout.tv_sec = 5;
			timeout.tv_usec = 0;
			setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout,
					sizeof(timeout));
			setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout,
					sizeof(timeout));
			serve(client);
		} catch (std::runtime_error& e) {
			if (*e.what()) {
				ERR("%s", e.what());
			}
		} catch (...) {
			ERR("Unkown exception throwed");
		}
	}
	close(sock);
	LOG("Server socket closed");
}
}
