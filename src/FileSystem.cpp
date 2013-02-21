#include "FileSystem.h"

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <string>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <sstream>
#include "Log.h"

namespace FileSystem {
static const int RANDSTR_LEN = 10;

std::string Root;

void Init() {
	srand(time(NULL));
	RecursiveRemove("/var/cache/allkorrect");

	if (mkdir("/var/cache/allkorrect", 0711) < 0) {
		throw std::runtime_error("Cannot create cache directory");
	}
	LOG("Cache directory created at /var/cache/allkorrect");

	Root = "/var/cache/allkorrect/";
}

std::string RandString() {
	std::ostringstream buf;
	buf << '_';
	for (int i = 0; i < RANDSTR_LEN; i++) {
		int id = rand() % (10 + 26 + 26);
		if (id < 10)
			buf << char('0' + id);
		else if (id < 10 + 26)
			buf << char('a' + (id - 10));
		else
			buf << char('A' + (id - 10 - 26));
	}
	return buf.str();
}

void RecursiveRemove(std::string dirname) {
	DIR *dir;
	struct dirent *entry;
	dir = opendir(dirname.c_str());
	if (dir == NULL) {
		return;
	}

	while ((entry = readdir(dir)) != NULL) {
		if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
			std::string path = dirname + '/' + entry->d_name;
			if (entry->d_type == DT_DIR) {
				RecursiveRemove(path);
			} else {
				unlink(path.c_str());
			}
		}

	}
	closedir(dir);
	rmdir(dirname.c_str());
}
std::string NewTmpDir() {
	std::string tmpDir = Root + FileSystem::RandString() + '/';
	if (mkdir(tmpDir.c_str(), 0777) < 0) {
		throw std::runtime_error("Cannot create tmp dir");
	}
	chmod(tmpDir.c_str(),0777);
	return tmpDir;
}

void NewBlob(std::string path) {
	int fd = open(path.c_str(), O_CREAT, 0700);
	if (fd < 0) {
		throw std::runtime_error("Cannot create blob");
	}
	close(fd);
}

void SetBlobReadOnly(std::string file) {
	if (chmod(file.c_str(), 0744) < 0) {
		throw std::runtime_error("Cannot set blob read only");
	}
}

void SetBlobWriteOnly(std::string file) {
	if (chmod(file.c_str(), 0722) < 0) {
		throw std::runtime_error("Cannot set blob write only");
	}
}

void SetBlobAllAccess(std::string file){
	if(chmod(file.c_str(),0777)<0){
		throw std::runtime_error("Cannot set blob all access");
	}
}

void RestoreBlobPermission(std::string file) {
	if (chmod(file.c_str(), 0700) < 0) {
		throw std::runtime_error("Cannot restore blob permission");
	}
}

bool HasBlob(std::string file) {
	struct stat sts;
	if (stat(file.c_str(), &sts) == -1) {
		if (errno == ENOENT) {
			return false;
		} else
			throw std::runtime_error("Cannot check whether blob exists");
	}
	return true;
}

void PutBlob(std::string name, const char* buf, size_t len) {
	int fd = open(name.c_str(), O_CREAT | O_WRONLY, 0700);
	if (fd < 0) {
		throw std::runtime_error("put blob open failure");
	}
	if (write(fd, buf, len) != (int) len) {
		close(fd);
		throw std::runtime_error("Cannot put blob");
	}
	close(fd);
}

std::vector<char> GetBlob(std::string name) {
	std::ifstream fin(name.c_str());
	return std::vector<char>((std::istreambuf_iterator<char>(fin)),
			std::istreambuf_iterator<char>());
}

void MoveBlob2File(std::string blob, std::string file) {
	SetBlobAllAccess(blob);
	if (rename(blob.c_str(), file.c_str()) < 0) {
		throw std::runtime_error("Cannot move blob to file");
	}
}

void MoveBlob2Blob(std::string blob1, std::string blob2) {
	if (rename(blob1.c_str(), blob2.c_str()) < 0) {
		throw std::runtime_error("Cannot move blob to blob");
	}
}

void MoveFile2Blob(std::string file, std::string blob) {
	if (rename(file.c_str(), blob.c_str()) < 0) {
		throw std::runtime_error("Cannot move file to blob");
	}
	RestoreBlobPermission(blob);
}

void MoveFile2File(std::string file1, std::string file2) {
	if (rename(file1.c_str(), file2.c_str()) < 0) {
		throw std::runtime_error("Cannot move file to file");
	}
}

void CopyFile(std::string oldName,std::string newName){
	std::string cmd="cp "+oldName+" "+newName;
	if(system(cmd.c_str())!=0){
		throw std::runtime_error("Cannot copy file");
	}
}

void CopyBlob2File(std::string blob, std::string file) {
	CopyFile(blob,file);
	SetBlobAllAccess(file);
}

void CopyBlob2Blob(std::string blob1, std::string blob2) {
	CopyFile(blob1,blob2);
	RestoreBlobPermission(blob2);
}

void CopyFile2Blob(std::string file, std::string blob) {
	CopyFile(file,blob);
	RestoreBlobPermission(blob);
}

void CopyFile2File(std::string file1, std::string file2) {
	CopyFile(file1,file2);
	SetBlobAllAccess(file2);
}

void CheckString(std::string str) {
	for (char c : str) {
		if (!('0' <= c && c <= '9' || 'a' <= c && c <= 'z'
				|| 'A' <= c && c <= 'Z' || c == '-' || c == '_' || c == '.')) {
			DBG("char is %c", c);
			throw std::runtime_error("Invalid character in blob name");
		}
	}
}

}
