#pragma once
struct StudioMesh {
    std::vector<float> Positions,Colors,UV,Normals;
    std::vector<uint32_t> Indices,FaceMaterials;
    std::vector<std::vector<uint32_t>> Faces;
    std::vector<uint64_t> Seams;
};
struct StudioMaterial {
    std::string Name="Material",Texture;
    float Color[3]={1,1,1},Metallic=0,Roughness=.5f;
};
struct StudioModifier {
    enum Kind {Mirror,Array,Subdivision,Bevel} Type=Mirror;
    bool Enabled=true;int Axis=0,Count=2,Levels=1;
    float Amount=.1f,Offset[3]={1,0,0};
};
