#pragma once
#include "scene_core.h"
#include <set>

struct Vec3 { float x, y, z; };
struct Bounds { Vec3 Min, Max; };
static const size_t MAX_EDIT_VERTICES = 200000;
static const size_t MAX_EDIT_INDICES = 1200000;

inline float WrapYaw(float degrees) {
    return std::isfinite(degrees) ? std::remainder(degrees, 360.0f) : 0.0f;
}
inline bool FiniteVec(Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }

// Same T * Rx * Ry * Rz * S order as the renderer, with no temporary allocations.
inline bool TransformPoint(const GameObject& o, Vec3 p, Vec3& result) {
    const double factor = 3.14159265358979323846 / 180.0;
    double x = double(p.x) * o.Scale[0], y = double(p.y) * o.Scale[1], z = double(p.z) * o.Scale[2];
    double a = o.Rotation[2] * factor, c = std::cos(a), s = std::sin(a);
    double t = c*x-s*y; y=s*x+c*y; x=t;
    a=o.Rotation[1]*factor; c=std::cos(a); s=std::sin(a);
    t=c*x+s*z; z=-s*x+c*z; x=t;
    a=o.Rotation[0]*factor; c=std::cos(a); s=std::sin(a);
    t=c*y-s*z; z=s*y+c*z; y=t;
    x+=o.Position[0]; y+=o.Position[1]; z+=o.Position[2];
    const double max = std::numeric_limits<float>::max();
    if (!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||std::abs(x)>max||std::abs(y)>max||std::abs(z)>max) return false;
    result={float(x),float(y),float(z)}; return true;
}
inline void ObjectMesh(const GameObject& o, std::vector<float>& vertices, std::vector<uint32_t>& indices) {
    if (!o.Vertices.empty()) { vertices=o.Vertices; indices=o.Indices; return; }
    std::vector<uint16_t> small;
    GeneratePrimitiveMeshData(o.Type, vertices, small);
    indices.assign(small.begin(), small.end());
}
inline bool WorldBounds(const GameObject& o, Bounds& b) {
    Vec3 lo={-.5f,-.5f,-.5f}, hi={.5f,.5f,.5f};
    if (o.Type==MeshType::Plane && o.Vertices.empty()) lo.y=hi.y=0;
    if (!o.Vertices.empty()) {
        lo=hi={o.Vertices[0],o.Vertices[1],o.Vertices[2]};
        for (size_t i=3;i<o.Vertices.size();i+=3) {
            lo.x=std::min(lo.x,o.Vertices[i]); hi.x=std::max(hi.x,o.Vertices[i]);
            lo.y=std::min(lo.y,o.Vertices[i+1]); hi.y=std::max(hi.y,o.Vertices[i+1]);
            lo.z=std::min(lo.z,o.Vertices[i+2]); hi.z=std::max(hi.z,o.Vertices[i+2]);
        }
    }
    if(o.Type==MeshType::Plane && hi.y-lo.y<.01f){lo.y-=.005f;hi.y+=.005f;}
    for(int corner=0;corner<8;++corner) {
        Vec3 p;
        if(!TransformPoint(o,{corner&1?hi.x:lo.x,corner&2?hi.y:lo.y,corner&4?hi.z:lo.z},p)) return false;
        if(corner==0) b.Min=b.Max=p;
        else {
            b.Min.x=std::min(b.Min.x,p.x); b.Max.x=std::max(b.Max.x,p.x);
            b.Min.y=std::min(b.Min.y,p.y); b.Max.y=std::max(b.Max.y,p.y);
            b.Min.z=std::min(b.Min.z,p.z); b.Max.z=std::max(b.Max.z,p.z);
        }
    }
    return true;
}

inline bool MakeEditable(GameObject& object, int subdivisions=0) {
    if(subdivisions<0 || subdivisions>4) return false;
    std::vector<float> vertices, colors=object.VertexColors;
    std::vector<uint32_t> indices;
    ObjectMesh(object,vertices,indices);
    for(int level=0;level<subdivisions;++level) {
        if(indices.size()>MAX_EDIT_INDICES/4 || vertices.size()/3>MAX_EDIT_VERTICES) return false;
        std::map<uint64_t,uint32_t> edges;
        std::vector<uint32_t> refined;
        refined.reserve(indices.size()*4);
        auto midpoint=[&](uint32_t a,uint32_t b)->uint32_t {
            if(a>b) std::swap(a,b);
            uint64_t key=(uint64_t(a)<<32)|b;
            auto existing=edges.find(key); if(existing!=edges.end()) return existing->second;
            uint32_t index=uint32_t(vertices.size()/3);
            for(int k=0;k<3;++k) vertices.push_back(float((double(vertices[a*3+k])+vertices[b*3+k])*.5));
            if(!colors.empty()) for(int k=0;k<3;++k) colors.push_back((colors[a*3+k]+colors[b*3+k])*.5f);
            edges[key]=index; return index;
        };
        for(size_t i=0;i<indices.size();i+=3) {
            uint32_t a=indices[i],b=indices[i+1],c=indices[i+2];
            uint32_t ab=midpoint(a,b),bc=midpoint(b,c),ca=midpoint(c,a);
            refined.insert(refined.end(),{a,ab,ca,ab,b,bc,ca,bc,c,ab,bc,ca});
        }
        if(vertices.size()/3>MAX_EDIT_VERTICES) return false;
        indices.swap(refined);
    }
    object.Vertices.swap(vertices); object.Indices.swap(indices); object.VertexColors.swap(colors);
    return true;
}

// Brush coordinates and radius are object-local. Mode 0=inflate, 1=smooth, 2=flatten Y.
inline bool SculptBrush(GameObject& object, Vec3 center, float radius, float strength, int mode) {
    if(!FiniteVec(center)||!std::isfinite(radius)||radius<=0||radius>10000||!std::isfinite(strength)||std::abs(strength)>10||mode<0||mode>2) return false;
    GameObject edited=object;
    if(!MakeEditable(edited)) return false;
    const auto& p=edited.Vertices;
    std::vector<double> sums(p.size(),0.0);
    std::vector<unsigned> counts(p.size()/3,0);
    for(size_t i=0;i<edited.Indices.size();i+=3) {
        uint32_t a=edited.Indices[i],b=edited.Indices[i+1],c=edited.Indices[i+2];
        if(mode==1) {
            uint32_t v[3]={a,b,c};
            for(int k=0;k<3;++k) {
                ++counts[v[k]]; ++counts[v[k]];
                for(int j=0;j<3;++j) sums[v[k]*3+j]+=double(p[v[(k+1)%3]*3+j])+p[v[(k+2)%3]*3+j];
            }
        } else {
            double u[3],v[3]; for(int k=0;k<3;++k) {u[k]=double(p[b*3+k])-p[a*3+k];v[k]=double(p[c*3+k])-p[a*3+k];}
            double n[3]={u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]};
            for(uint32_t index:{a,b,c}) for(int k=0;k<3;++k) sums[index*3+k]+=n[k];
        }
    }
    std::vector<float> result=p;
    for(size_t i=0;i<p.size();i+=3) {
        double x=double(p[i])-center.x,y=double(p[i+1])-center.y,z=double(p[i+2])-center.z;
        double distance=std::sqrt(x*x+y*y+z*z); if(distance>=radius) continue;
        double weight=(1.0-distance/radius);weight*=weight;
        double length=std::sqrt(sums[i]*sums[i]+sums[i+1]*sums[i+1]+sums[i+2]*sums[i+2]);
        for(int k=0;k<3;++k) {
            double delta=0;
            if(mode==1 && counts[i/3]) delta=(sums[i+k]/counts[i/3]-p[i+k])*std::min(1.0f,std::abs(strength));
            else if(mode==2 && k==1) delta=(double(center.y)-p[i+k])*std::min(1.0f,std::abs(strength));
            else if(mode==0) delta=strength*(length>1e-15?sums[i+k]/length:(k==1?1.0:0.0));
            double value=p[i+k]+delta*weight;
            if(!std::isfinite(value)||std::abs(value)>std::numeric_limits<float>::max()) return false;
            result[i+k]=float(value);
        }
    }
    edited.Vertices.swap(result); object=std::move(edited);return true;
}

inline int MergeObjects(std::vector<GameObject>& objects,const std::vector<int>& selection,const std::string& name) {
    std::set<int> unique(selection.begin(),selection.end());
    if(unique.size()<2||name.size()>255||name.find('\0')!=std::string::npos) return -1;
    for(int i:unique) if(i<0||size_t(i)>=objects.size()) return -1;
    GameObject merged(name); merged.Color[0]=merged.Color[1]=merged.Color[2]=1;
    double pivot[3]={0,0,0};
    for(int i:unique) for(int k=0;k<3;++k) pivot[k]+=objects[i].Position[k]/double(unique.size());
    for(int k=0;k<3;++k) merged.Position[k]=float(pivot[k]);
    merged.Metallic=objects[*unique.begin()].Metallic; merged.Roughness=objects[*unique.begin()].Roughness;
    for(int i:unique) {
        const auto& object=objects[i]; std::vector<float> p; std::vector<uint32_t> indices;
        ObjectMesh(object,p,indices);
        if((merged.Vertices.size()+p.size())/3>MAX_EDIT_VERTICES || merged.Indices.size()+indices.size()>MAX_EDIT_INDICES) return -1;
        uint32_t offset=uint32_t(merged.Vertices.size()/3);
        for(size_t v=0;v<p.size();v+=3) {
            Vec3 world;if(!TransformPoint(object,{p[v],p[v+1],p[v+2]},world))return -1;
            double coords[3]={double(world.x)-pivot[0],double(world.y)-pivot[1],double(world.z)-pivot[2]};
            for(int k=0;k<3;++k) {
                if(!std::isfinite(coords[k])||std::abs(coords[k])>std::numeric_limits<float>::max()) return -1;
                merged.Vertices.push_back(float(coords[k]));
                merged.VertexColors.push_back(object.Color[k]*(object.VertexColors.empty()?1.0f:object.VertexColors[v+k]));
            }
        }
        for(auto index:indices) merged.Indices.push_back(offset+index);
    }
    // Reserve before erasing, so allocation failure leaves the source scene intact.
    objects.reserve(objects.size()+1);
    for(auto i=unique.rbegin();i!=unique.rend();++i) objects.erase(objects.begin()+*i);
    merged.Selected=true; objects.push_back(std::move(merged)); return int(objects.size())-1;
}

inline bool SetProperty(GameObject& object,const std::string& name,const ObjectProperty& value) {
    if(name.empty()||name.size()>64||name.find('\0')!=std::string::npos||value.TextValue.size()>1024) return false;
    if(value.Type==ObjectProperty::Number && (!std::isfinite(value.NumberValue)||std::abs(value.NumberValue)>std::numeric_limits<float>::max())) return false;
    if(object.Properties.size()>=256&&object.Properties.find(name)==object.Properties.end())return false;
    object.Properties[name]=value;return true;
}

struct CharacterConfig {
    float Height=2, Width=.65f, HeadSize=.45f;
    float BodyColor[3]={.22f,.6f,.95f};
    float SkinColor[3]={.95f,.74f,.5f};
};
inline int CreateCharacter(std::vector<GameObject>& objects,const std::string& name,const CharacterConfig& config) {
    if(name.size()>255||name.find('\0')!=std::string::npos)return -1;
    if(!std::isfinite(config.Height)||!std::isfinite(config.Width)||!std::isfinite(config.HeadSize)||config.Height<.5f||config.Height>10||config.Width<.2f||config.Width>5||config.HeadSize<.1f||config.HeadSize>config.Height*.45f) return -1;
    for(int k=0;k<3;++k) if(!std::isfinite(config.BodyColor[k])||!std::isfinite(config.SkinColor[k])||config.BodyColor[k]<0||config.BodyColor[k]>1||config.SkinColor[k]<0||config.SkinColor[k]>1) return -1;
    std::vector<GameObject> parts;
    const float h=config.Height,w=config.Width,head=config.HeadSize;
    auto part=[&](const char* label,float x,float y,float sx,float sy,float sz,bool skin) {
        parts.emplace_back(label);auto& o=parts.back();o.Position[0]=x;o.Position[1]=y;
        o.Scale[0]=sx;o.Scale[1]=sy;o.Scale[2]=sz;
        for(int k=0;k<3;++k)o.Color[k]=skin?config.SkinColor[k]:config.BodyColor[k];
    };
    part("Head",0,h-head*.5f,head,head,head,true);
    part("Torso",0,h*.58f,w,h*.35f,w*.55f,false);
    part("Left Arm",-w*.72f,h*.58f,w*.28f,h*.35f,w*.4f,true);
    part("Right Arm",w*.72f,h*.58f,w*.28f,h*.35f,w*.4f,true);
    part("Left Leg",-w*.25f,h*.2f,w*.38f,h*.4f,w*.5f,false);
    part("Right Leg",w*.25f,h*.2f,w*.38f,h*.4f,w*.5f,false);
    int result=MergeObjects(parts,{0,1,2,3,4,5},name);if(result<0)return -1;
    GameObject character=std::move(parts[result]);
    // Bake the merged pivot, keeping the character's origin at its feet.
    for(size_t i=0;i<character.Vertices.size();i+=3) for(int k=0;k<3;++k) character.Vertices[i+k]+=character.Position[k];
    character.Position[0]=character.Position[1]=character.Position[2]=0;
    character.Dynamic=true;character.Character=true;character.Restitution=0;character.Mass=3;
    objects.push_back(std::move(character));return int(objects.size())-1;
}

struct PhysicsState { double Accumulator=0; float Gravity=-9.81f; bool Ground=true; };
inline void PhysicsStep(std::vector<GameObject>& objects,float dt,const PhysicsState& state) {
    for(auto& o:objects) if(o.Dynamic) {
        o.Grounded=false;
        o.Velocity[1]+=state.Gravity*dt;
        for(int k=0;k<3;++k) o.Position[k]=float(std::max(-1000000.0,std::min(1000000.0,double(o.Position[k])+double(o.Velocity[k])*dt)));
        Bounds b;if(!WorldBounds(o,b))continue;
        if(state.Ground&&b.Min.y<0) {
            o.Position[1]-=b.Min.y;
            if(o.Velocity[1]<0)o.Velocity[1]=-o.Velocity[1]*o.Restitution;
            o.Grounded=true;
            float drag=std::max(0.0f,1-o.Friction*dt);o.Velocity[0]*=drag;o.Velocity[2]*=drag;
        }
    }
    for(size_t i=0;i<objects.size();++i)for(size_t j=i+1;j<objects.size();++j) {
        auto& a=objects[i];auto& b=objects[j];if(!a.Dynamic&&!b.Dynamic)continue;
        Bounds ba,bb;if(!WorldBounds(a,ba)||!WorldBounds(b,bb))continue;
        float overlap[3]={std::min(ba.Max.x,bb.Max.x)-std::max(ba.Min.x,bb.Min.x),std::min(ba.Max.y,bb.Max.y)-std::max(ba.Min.y,bb.Min.y),std::min(ba.Max.z,bb.Max.z)-std::max(ba.Min.z,bb.Min.z)};
        if(overlap[0]<=0||overlap[1]<=0||overlap[2]<=0)continue;
        int axis=overlap[1]<overlap[0]?1:0;if(overlap[2]<overlap[axis])axis=2;
        const float centersA[3]={ba.Min.x*.5f+ba.Max.x*.5f,ba.Min.y*.5f+ba.Max.y*.5f,ba.Min.z*.5f+ba.Max.z*.5f};
        const float centersB[3]={bb.Min.x*.5f+bb.Max.x*.5f,bb.Min.y*.5f+bb.Max.y*.5f,bb.Min.z*.5f+bb.Max.z*.5f};
        float sign=centersA[axis]<centersB[axis]?-1.0f:1.0f;
        float ia=a.Dynamic?1/std::max(.01f,a.Mass):0,ib=b.Dynamic?1/std::max(.01f,b.Mass):0,total=ia+ib;
        a.Position[axis]+=sign*overlap[axis]*ia/total;b.Position[axis]-=sign*overlap[axis]*ib/total;
        float relative=(a.Velocity[axis]-b.Velocity[axis])*sign;
        if(relative<0) {
            float impulse=-(1+std::min(a.Restitution,b.Restitution))*relative/total;
            a.Velocity[axis]+=impulse*sign*ia;b.Velocity[axis]-=impulse*sign*ib;
        }
        if(axis==1){if(a.Dynamic&&sign>0)a.Grounded=true;if(b.Dynamic&&sign<0)b.Grounded=true;}
    }
}
inline void AdvancePhysics(std::vector<GameObject>& objects,PhysicsState& state,float frameDt) {
    if(!std::isfinite(frameDt)||frameDt<0||!std::isfinite(state.Gravity))return;
    const double step=1.0/120;
    state.Accumulator+=std::min(frameDt,.1f);
    int count=0;while(state.Accumulator>=step&&count++<12){PhysicsStep(objects,float(step),state);state.Accumulator-=step;}
    if(state.Accumulator>=step)state.Accumulator=std::fmod(state.Accumulator,step);
}

inline bool InverseTransformPoint(const GameObject& o,Vec3 point,Vec3& result) {
    for(float scale:o.Scale)if(!std::isfinite(scale)||std::abs(scale)<1e-8f)return false;
    double x=double(point.x)-o.Position[0],y=double(point.y)-o.Position[1],z=double(point.z)-o.Position[2];
    const double factor=-3.14159265358979323846/180;
    double a=o.Rotation[0]*factor,c=std::cos(a),s=std::sin(a),t=c*y-s*z;z=s*y+c*z;y=t;
    a=o.Rotation[1]*factor;c=std::cos(a);s=std::sin(a);t=c*x+s*z;z=-s*x+c*z;x=t;
    a=o.Rotation[2]*factor;c=std::cos(a);s=std::sin(a);t=c*x-s*y;y=s*x+c*y;x=t;
    result={float(x/o.Scale[0]),float(y/o.Scale[1]),float(z/o.Scale[2])};return FiniteVec(result);
}
inline Vec3 Subtract(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline Vec3 Cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline double Dot(Vec3 a,Vec3 b){return double(a.x)*b.x+double(a.y)*b.y+double(a.z)*b.z;}
inline bool RaycastObject(const GameObject& o,Vec3 origin,Vec3 direction,Vec3& localHit,float& distance) {
    Vec3 localOrigin,end;if(!InverseTransformPoint(o,origin,localOrigin)||!InverseTransformPoint(o,{origin.x+direction.x,origin.y+direction.y,origin.z+direction.z},end))return false;
    Vec3 ray=Subtract(end,localOrigin);
    std::vector<float> temporary;std::vector<uint32_t> temporaryIndices;
    const std::vector<float>* vertices=&o.Vertices;const std::vector<uint32_t>* indices=&o.Indices;
    if(vertices->empty()){ObjectMesh(o,temporary,temporaryIndices);vertices=&temporary;indices=&temporaryIndices;}
    double best=distance;bool hit=false;
    for(size_t i=0;i<indices->size();i+=3) {
        auto vertex=[&](uint32_t index)->Vec3 {return {(*vertices)[index*3],(*vertices)[index*3+1],(*vertices)[index*3+2]};};
        Vec3 a=vertex((*indices)[i]),b=vertex((*indices)[i+1]),c=vertex((*indices)[i+2]);
        Vec3 edge1=Subtract(b,a),edge2=Subtract(c,a),p=Cross(ray,edge2);
        double determinant=Dot(edge1,p);if(std::abs(determinant)<1e-10)continue;
        Vec3 offset=Subtract(localOrigin,a);double u=Dot(offset,p)/determinant;if(u<0||u>1)continue;
        Vec3 q=Cross(offset,edge1);double v=Dot(ray,q)/determinant;if(v<0||u+v>1)continue;
        double t=Dot(edge2,q)/determinant;if(t<=0||t>=best)continue;
        best=t;hit=true;
    }
    if(hit){distance=float(best);localHit={localOrigin.x+ray.x*distance,localOrigin.y+ray.y*distance,localOrigin.z+ray.z*distance};}
    return hit;
}

inline bool SnapObject(GameObject& object,float step) {
    if(!std::isfinite(step)||step<=0)return false;
    float snapped[3];
    for(int k=0;k<3;++k){double value=std::round(double(object.Position[k])/step)*step;if(!std::isfinite(value)||std::abs(value)>std::numeric_limits<float>::max())return false;snapped[k]=float(value);}
    std::copy(snapped,snapped+3,object.Position);return true;
}
