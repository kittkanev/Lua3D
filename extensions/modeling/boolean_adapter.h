#pragma once
#include <vector>
#include <string>
#include <cstdint>
bool StudioBooleanMesh(const std::vector<float>& a,const std::vector<uint32_t>& ai,const std::vector<float>& b,const std::vector<uint32_t>& bi,int operation,std::vector<float>& vertices,std::vector<uint32_t>& indices,std::string& error);
