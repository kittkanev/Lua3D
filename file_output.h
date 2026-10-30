#pragma once
#include <cstdio>
#include <memory>
#include <streambuf>
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
// Keep file creation/permissions under our control; tinygltf's MinGW file
// writer passes _O_CREAT without a mode argument in the vendored version.
inline std::FILE* OpenBinaryOutput(const std::string& filename) {
#ifdef _WIN32
    int length=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,filename.c_str(),-1,nullptr,0);
    if(length<=0)return nullptr;
    std::wstring wide(size_t(length),L'\0');
    if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,filename.c_str(),-1,&wide[0],length))return nullptr;
    return _wfopen(wide.c_str(),L"wb");
#else
    return std::fopen(filename.c_str(),"wb");
#endif
}
class BinaryOutputBuffer : public std::streambuf {
public:
    explicit BinaryOutputBuffer(std::FILE* file):file_(file){}
protected:
    std::streamsize xsputn(const char* data,std::streamsize count) override {
        return count>0?std::streamsize(std::fwrite(data,1,size_t(count),file_)):0;
    }
    int_type overflow(int_type value) override {
        if(traits_type::eq_int_type(value,traits_type::eof()))return traits_type::not_eof(value);
        return std::fputc(traits_type::to_char_type(value),file_)==EOF?traits_type::eof():value;
    }
    int sync() override {return std::fflush(file_)==0?0:-1;}
private:
    std::FILE* file_;
};
