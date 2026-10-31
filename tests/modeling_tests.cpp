#include "../scene_tools.h"
#include "../mesh_modeling.h"
#include <cassert>
#include <iostream>
static double Volume(const GameObject& o){auto m=Model::FromObject(o);double v=0;for(size_t i=0;i<m.Indices.size();i+=3)v+=Dot(Model::Point(m,m.Indices[i]),Cross(Model::Point(m,m.Indices[i+1]),Model::Point(m,m.Indices[i+2])))/6;return std::abs(v);}
int main(){
    GameObject cube("Cube");auto mesh=Model::FromObject(cube);Model::Polygonize(mesh);assert(mesh.Faces.size()==6);Model::ToObject(cube,mesh);assert(std::abs(Volume(cube)-1)<1e-6);
    auto original=cube;StudioModifier array;array.Type=StudioModifier::Array;array.Offset[0]=2;Model::AddModifier(cube,array);assert(cube.Faces.size()==12&&cube.BaseMesh.Faces.size()==6);Model::ApplyModifiers(cube);assert(cube.Modifiers.empty());
    GameObject other("Other");other.Position[0]=.5f;auto u=Model::Boolean(original,other,0),d=Model::Boolean(original,other,1),i=Model::Boolean(original,other,2);assert(std::abs(Volume(u)-1.5)<1e-5&&std::abs(Volume(d)-.5)<1e-5&&std::abs(Volume(i)-.5)<1e-5);
    auto bevel=mesh;Model::Bevel(bevel,.1f);GameObject beveled("Beveled");Model::ToObject(beveled,bevel);std::cerr<<"bevel volume "<<Volume(beveled)<<"\n";assert(Volume(beveled)<1&&Volume(beveled)>.5);
    assert(std::abs(Volume(Model::Boolean(beveled,original,2))-Volume(beveled))<1e-5);
    auto extruded=mesh;Model::Extrude(extruded,{0},.4f);assert(extruded.Faces.size()==10);Model::Check(extruded);
    auto subdivided=mesh;Model::Subdivide(subdivided,1);assert(subdivided.Faces.size()==48);Model::Check(subdivided);
    bool failed=false;try{GameObject plane("Plane",MeshType::Plane);Model::Boolean(original,plane,0);}catch(const std::exception&){failed=true;}assert(failed);
    failed=false;try{auto invalid=mesh;invalid.Faces[0][0]=99999;Model::Check(invalid);}catch(const std::exception&){failed=true;}assert(failed);
    std::cout<<"PASS: polygon topology, modifier base preservation, closed bevel, extrude, Loop subdivision, CSG volumes and invalid input\n";
}
