#pragma once
#include <vector>
#include <string>
#include <stdexcept>
class BinaryStream
{
public:
	size_t readingPointer;
	std::vector<char> buffer;
	BinaryStream()
		:readingPointer(0){}

	BinaryStream(const std::vector<char>& other):readingPointer(0),buffer(other){}

	void Resize(size_t size){
		buffer.resize(size);
	}

	void Write(const void* buf,size_t size){
		buffer.insert(buffer.end(),(const char*)buf,(const char*)buf+size);
	}

	template<typename T>
	void Write(const T& t){
		Write(&t,sizeof(T));
	}

	BinaryStream& operator<<(char c){
		Write(c);
		return *this;
	}
	BinaryStream& operator<<(unsigned short s){
		Write(s);
		return *this;
	}
	BinaryStream& operator<<(int i){
		Write(i);
		return *this;
	}
	BinaryStream& operator<<(long long l){
		Write(l);
		return *this;
	}
	BinaryStream& operator<<(unsigned long long ul){
		Write(ul);
		return *this;
	}
	BinaryStream& operator<<(const std::string& s){
		size_t len=s.size();
		while(len>127){
			Write<char>(0x80 | (len&0x7F));
			len>>=7;
		}
		Write<char>(len);

		Write(s.data(),s.size());
		return *this;
	}

	void* ToBytes(){
		return &buffer[0];
	}
	size_t Length(){
		return buffer.size();
	}

	const void* Read(size_t len){
		if(readingPointer+len>buffer.size())
			throw std::runtime_error("BinaryStream::Read");
		const void* result=&buffer[readingPointer];
		readingPointer+=len;
		return result;
	}

	template<typename T>
	T Read(){
		return *(const T*)Read(sizeof(T));
	}

	BinaryStream& operator >>(char& c){
		c=Read<char>();
		return *this;
	}
	BinaryStream& operator >>(unsigned short& s){
		s=Read<unsigned short>();
		return *this;
	}
	BinaryStream& operator >>(int& i){
		i=Read<int>();
		return *this;
	}
	BinaryStream& operator >>(long long& l){
		l=Read<long long>();
		return *this;
	}
	BinaryStream& operator >>(unsigned long long& l){
		l=Read<unsigned long long>();
		return *this;
	}
	BinaryStream& operator >>(std::string& s){
		size_t len,lenByteN;
		char lenByte;

		for(len=0,lenByteN=0,lenByte=Read<char>();lenByte & 0x80;lenByte=Read<char>()){
			len|=(lenByte&0x7F)<<lenByteN++*7;
		}
		len|=lenByte<<lenByteN*7;

		const char* str=(const char*)Read(len);
		s.assign(str,str+len);
		return *this;
	}
};

