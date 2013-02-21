#pragma once
#include <string>
#include <vector>
namespace FileSystem {
extern std::string Root;
extern void Init();
extern std::string RandString();
extern void RecursiveRemove(std::string dirname);
extern std::string NewTmpDir();
extern void NewBlob(std::string path);
extern void SetBlobReadOnly(std::string file);
extern void SetBlobWriteOnly(std::string file);
extern void SetBlobAllAccess(std::string file);
extern void RestoreBlobPermission(std::string file);
extern bool HasBlob(std::string file);
extern void PutBlob(std::string name,const char* buf,size_t len);
extern std::vector<char> GetBlob(std::string name);
extern void CheckString(std::string str);

extern void MoveBlob2File(std::string blob,std::string file);
extern void MoveBlob2Blob(std::string,std::string);
extern void MoveFile2Blob(std::string,std::string);
extern void MoveFile2File(std::string,std::string);

extern void CopyBlob2File(std::string blob,std::string file);
extern void CopyBlob2Blob(std::string,std::string);
extern void CopyFile2Blob(std::string,std::string);
extern void CopyFile2File(std::string,std::string);
}
