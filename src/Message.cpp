#include "Message.h"
#include <stdexcept>
#include <sys/socket.h>
#include "Log.h"
#include "BinaryStream.h"
namespace Message {
static void recvAll(int sock, void* buf, size_t len) {
	size_t received = 0;
	while (received < len) {
		int cur = recv(sock, (char*) buf + received, len - received, 0);
		if (cur <= 0) {
			ERR("recv returned %d, cannot fill buffer.", cur);
			throw std::runtime_error("");
		}
		received += cur;
	}
}

static void sendAll(int sock, const void* buf, int len) {
	if (send(sock, buf, len, 0) != len) {
		throw std::runtime_error("send didn't send all the data.");
	}
}

Message Next(int sock){
	char buf[8];
	recvAll(sock,buf,8);
	Message result;
	result.type=*(Type*)buf;
	result.size=*(uint8_t*)(buf+4);

	DBG("Just recved type=%d size=%u\n",result.type,result.size);

	if(result.size>MAX_BODY_SIZE){
		throw std::runtime_error("Received message body too large");
	}

	result.body.resize(result.size);
	recvAll(sock,&result.body[0],result.size);
	DBG("Finished a msg");
	return result;
}

void Send(int sock,const Message& msg){
	sendAll(sock,&msg.type,4);
	sendAll(sock,&msg.size,4);
	sendAll(sock,&msg.body[0],msg.size);
}

MsgExec ToMsgExec(const Message& msg){
	BinaryStream stream(msg.body);
	MsgExec result;
	stream>>result.cmd>>result.argc;
	for(int i=0;i<result.argc;i++){
		std::string tmp;
		stream>>tmp;
		result.arg.push_back(tmp);
	}
	stream>>result.memoryLimit>>result.outputLimit>>result.timeLimit;
	result.restriction=stream.Read<Restriction>();
	stream>>result.input;
	return result;
}

Message FromMsgExecReply(const MsgExecReply& result){
	BinaryStream stream;
	stream<<result.exitStatus;
	stream.Write(result.type);
	stream<<result.output<<result.error<<result.memory<<result.time;
	Message msg;
	msg.type=EXEC_REPLY;
	msg.size=stream.Length();
	msg.body=stream.buffer;
	return msg;
}

MsgPutBlob ToMsgPutBlob(const Message& msg){
	BinaryStream stream(msg.body);
	MsgPutBlob result;
	stream>>result.name>>result.len;
	result.buf=&msg.body[stream.readingPointer];
	return result;
}

MsgGetBlob ToMsgGetBlob(const Message& msg){
	BinaryStream stream(msg.body);
	MsgGetBlob result;
	stream>>result.name;
	return result;
}

MsgCopyMove ToMsgCopyMove(const Message& msg){
	BinaryStream stream(msg.body);
	MsgCopyMove result;
	stream>>result.oldName>>result.newName;
	return result;
}
}
