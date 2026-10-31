#include "../scene_tools.h"
#include "../editor_transform.h"
#include <cassert>
#include <iostream>

int main() {
    {GameObject source("Matrix");source.Position[0]=5;source.Position[1]=2;source.Rotation[0]=25;source.Rotation[1]=63;source.Rotation[2]=-41;source.Scale[0]=2;source.Scale[1]=3;source.Scale[2]=.75f;StudioMatrix matrix=StudioObjectMatrix(source);GameObject restored("Restored");assert(StudioApplyMatrix(restored,matrix));StudioMatrix same=StudioObjectMatrix(restored);for(int i=0;i<16;i++)assert(std::abs(matrix.M[i]-same.M[i])<.0001f);StudioMatrix unit=StudioMultiply(matrix,StudioInverse(matrix));for(int i=0;i<16;i++)assert(std::abs(unit.M[i]-(i%5==0?1:0))<.0001f);auto grid=StudioPlanGrid(10000,123456,-34567);assert(grid.CenterX>120000&&grid.CenterZ<-30000&&grid.Extent>10000);StudioObjectHistory history;std::vector<GameObject> objects={source};history.Capture(objects);objects[0].Position[0]=99;history.Capture(objects);assert(history.Undo(objects)&&objects[0].Position[0]==5);assert(history.Redo(objects)&&objects[0].Position[0]==99);}

    assert(ParseTestTimeoutMs(nullptr) == 0);
    assert(ParseTestTimeoutMs("") == 0);
    assert(ParseTestTimeoutMs("0") == 0);
    assert(ParseTestTimeoutMs("2") == 2000);
    assert(ParseTestTimeoutMs("-1") == 0);
    assert(ParseTestTimeoutMs("2junk") == 0);
    assert(ParseTestTimeoutMs(" 2") == 0);
    assert(ParseTestTimeoutMs("999999999999999999999999") == 3600000000ULL);
    assert(ParseMeshType("SPHERE") == MeshType::Sphere);
    assert(std::string(GetMeshTypeName(MeshType::Cylinder)) == "Cylinder");
    for (int t = 0; t < 5; ++t) {
        std::vector<float> p;
        std::vector<uint16_t> ix;
        GeneratePrimitiveMeshData(static_cast<MeshType>(t), p, ix);
        assert(!p.empty() && p.size() % 3 == 0 && ix.size() % 3 == 0);
        for (auto i : ix) assert(i < p.size() / 3);
        for (size_t i = 0; i < ix.size(); i += 3) {
            double a[3], b[3], c[3], u[3], v[3];
            for (int k = 0; k < 3; ++k) {
                a[k] = p[ix[i]*3+k]; b[k] = p[ix[i+1]*3+k]; c[k] = p[ix[i+2]*3+k];
                u[k] = b[k]-a[k]; v[k] = c[k]-a[k];
            }
            double n[3] = {u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0]};
            double dot = 0;
            for (int k = 0; k < 3; ++k) dot += n[k]*(a[k]+b[k]+c[k])/3;
            // Plane has both sides; sphere poles include zero-area triangles.
            if (t != 4) assert(dot >= -1e-7);
        }
    }
    auto q = EulerDegreesToQuaternion(90, 0, 0);
    assert(std::abs(q[0]-std::sqrt(0.5)) < 1e-6);
    assert(std::abs(q[3]-std::sqrt(0.5)) < 1e-6);
    q = EulerDegreesToQuaternion(30, 60, 90);
    double norm = 0; for (double x : q) norm += x*x;
    assert(std::abs(norm-1) < 1e-6);
    std::vector<GameObject> objects;
    objects.emplace_back("source", MeshType::Sphere);
    objects[0].Position[0] = 12; objects[0].Metallic = 0.8f;
    assert(DuplicateObject(objects, -1, "bad") == -1);
    assert(DuplicateObject(objects, 1, "bad") == -1);
    assert(DuplicateObject(objects, 0, "copy") == 1);
    assert(objects[1].Name == "copy" && objects[1].Position[0] == 12);
    assert(objects[1].Type == MeshType::Sphere && objects[1].Metallic == 0.8f);
    objects[1].Position[0] = 99; assert(objects[0].Position[0] == 12);
    GameObject snapped("snap");snapped.Position[0]=1.26f;assert(!SnapObject(snapped,0));assert(SnapObject(snapped,.5f)&&snapped.Position[0]==1.5f);
    assert(WrapYaw(361)==1 && WrapYaw(-721)==-1);
    assert(WrapYaw(std::numeric_limits<float>::infinity())==0);
    float yaw=0;for(int i=0;i<100000;++i) {yaw=WrapYaw(yaw+.4f);assert(yaw>=-180&&yaw<=180);}
    GameObject editable("clay",MeshType::Plane);
    assert(!MakeEditable(editable,-1) && editable.Vertices.empty());
    assert(MakeEditable(editable,2));
    auto before=editable.Vertices;
    assert(!SculptBrush(editable,{0,0,0},0,1,0) && editable.Vertices==before);
    assert(SculptBrush(editable,{0,0,0},2,1,0));
    bool changed=false;for(size_t i=1;i<before.size();i+=3)changed|=editable.Vertices[i]>before[i];assert(changed);
    assert(SculptBrush(editable,{0,0,0},2,.5f,1));
    std::vector<GameObject> joined;joined.emplace_back("red");joined.emplace_back("blue");
    joined[0].Position[0]=2;joined[1].Position[0]=-2;
    joined[0].Rotation[2]=90;
    assert(MergeObjects(joined,{0,99},"invalid")==-1&&joined.size()==2);
    assert(MergeObjects(joined,{0,0},"invalid")==-1);
    assert(MergeObjects(joined,{0,1},"joined")==0&&joined.size()==1);
    assert(joined[0].Vertices.size()==48&&joined[0].Indices.size()==72);
    Bounds bounds;assert(WorldBounds(joined[0],bounds));
    assert(std::abs(bounds.Min.x+2.5f)<1e-5&&std::abs(bounds.Max.x-2.5f)<1e-5);
    Vec3 hit;float hitDistance=100;
    assert(RaycastObject(joined[0],{2,0,4},{0,0,-1},hit,hitDistance));
    assert(hitDistance>3&&hitDistance<4);
    GameObject transformed("transform");transformed.Position[0]=5;transformed.Rotation[0]=25;transformed.Rotation[1]=60;transformed.Rotation[2]=15;transformed.Scale[1]=2;
    Vec3 world,local;assert(TransformPoint(transformed,{.2f,.3f,.4f},world)&&InverseTransformPoint(transformed,world,local));
    assert(std::abs(local.x-.2f)<1e-5&&std::abs(local.y-.3f)<1e-5&&std::abs(local.z-.4f)<1e-5);
    ObjectProperty property;property.NumberValue=100;
    assert(!SetProperty(joined[0],"",property));assert(SetProperty(joined[0],"Health",property));
    property.NumberValue=std::numeric_limits<double>::quiet_NaN();
    assert(!SetProperty(joined[0],"Health",property)&&joined[0].Properties["Health"].NumberValue==100);
    std::vector<GameObject> bodies;bodies.emplace_back("falling");bodies[0].Dynamic=true;bodies[0].Position[1]=3;
    PhysicsState physics;for(int i=0;i<600;++i)AdvancePhysics(bodies,physics,1.0f/120);
    assert(bodies[0].Position[1]>=.499f&&bodies[0].Position[1]<.51f);
    assert(bodies[0].Grounded);
    std::vector<GameObject> platform;platform.emplace_back("body");platform[0].Dynamic=true;platform[0].Restitution=0;platform[0].Position[1]=4;
    platform.emplace_back("platform");platform[1].Position[1]=1;platform[1].Scale[0]=platform[1].Scale[2]=3;
    PhysicsState platformPhysics;for(int i=0;i<600;++i)AdvancePhysics(platform,platformPhysics,1.0f/120);
    assert(std::abs(platform[0].Position[1]-2)<.01f);
    CharacterConfig character;assert(CreateCharacter(bodies,"Player",character)==1);
    assert(bodies[1].Character&&bodies[1].Dynamic&&bodies[1].Vertices.size()==144);
    character.Height=0;assert(CreateCharacter(bodies,"bad",character)==-1&&bodies.size()==2);
    std::cout << "PASS: five mesh types, bounds, winding, quaternion, duplication\n";
}
