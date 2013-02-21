#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include "Execute.h"

namespace Message {
static const size_t MAX_BODY_SIZE = 100 * 1024 * 1024;

enum Type {
	EXIT, EXEC, EXEC_REPLY,PUT_BLOB,OK,GET_BLOB,GET_BLOB_REPLY,
	MOVE_BLOB2FILE,MOVE_BLOB2BLOB,MOVE_FILE2FILE,MOVE_FILE2BLOB,
	COPY_BLOB2FILE,COPY_BLOB2BLOB,COPY_FILE2FILE,COPY_FILE2BLOB,
	HAS_BLOB,HAS_FILE,HAS_BLOB_REPLY,HAS_FILE_REPLY
};

struct Message {
	enum Type type;
	uint32_t size;
	std::vector<char> body;
};

enum Restriction {
	STRICT, LOOSE
};

struct MsgExec {
	std::string cmd;
	int argc;
	std::vector<std::string> arg;
	long long memoryLimit;
	long long outputLimit;
	int timeLimit;
	Restriction restriction;
	std::string input;
};

struct MsgExecReply {
	int exitStatus;
	Execute::ResultType type;
	std::string output, error;
	long long memory;
	int time;
};

struct MsgPutBlob{
	std::string name;
	int len;
	const char* buf;
};

struct MsgGetBlob{
	std::string name;
};

struct MsgCopyMove{
	std::string oldName;
	std::string newName;
};

extern Message Next(int sock);
extern void Send(int sock, const Message& msg);
extern MsgExec ToMsgExec(const Message& msg);
extern Message FromMsgExecReply(const MsgExecReply& result);
extern MsgPutBlob ToMsgPutBlob(const Message& msg);
extern MsgGetBlob ToMsgGetBlob(const Message& msg);
extern MsgCopyMove ToMsgCopyMove(const Message& msg);
}
