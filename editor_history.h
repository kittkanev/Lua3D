#pragma once
// Operation snapshots are captured when a gesture finishes, never during Play.
struct StudioSnapshot {
    std::vector<GameObject> Objects;
    std::vector<StudioSound> Sounds;
    float Top[3],Bottom[3];int Sky;PhysicsState Physics;
    uint64_t Hash=0;
};
inline uint64_t StudioDocumentHash() {
    uint64_t h=StudioSceneFingerprint(g_Objects);
    auto bytes=[&](const void* p,size_t n){const unsigned char* b=static_cast<const unsigned char*>(p);while(n--)h=(h^*b++)*1099511628211ULL;};
    for(const auto& s:g_Sounds){bytes(&s.Id,sizeof(s.Id));bytes(s.Name.data(),s.Name.size());bytes(s.Path.data(),s.Path.size());bytes(s.Position,sizeof(s.Position));bytes(&s.Volume,sizeof(s.Volume));bytes(&s.Pitch,sizeof(s.Pitch));bytes(&s.Loop,sizeof(s.Loop));bytes(&s.Spatial,sizeof(s.Spatial));bytes(&s.Autoplay,sizeof(s.Autoplay));}
    bytes(g_SkyTop,sizeof(g_SkyTop));bytes(g_SkyBottom,sizeof(g_SkyBottom));bytes(&g_SkyPreset,sizeof(g_SkyPreset));bytes(&g_Physics.Gravity,sizeof(float));bytes(&g_Physics.Ground,sizeof(bool));return h;
}
struct StudioHistory {
    std::vector<StudioSnapshot> States;size_t Cursor=0,Bytes=0;
    static size_t Size(const StudioSnapshot& s){size_t n=StudioObjectHistory::Size(s.Objects)+s.Sounds.size()*sizeof(StudioSound);for(const auto& o:s.Objects){n+=o.Name.size()+o.UV.size()*4+o.Normals.size()*4+o.BaseMesh.Positions.size()*4+o.BaseMesh.Indices.size()*4+o.BaseMesh.UV.size()*4;for(const auto& f:o.Faces)n+=f.size()*4;for(const auto& f:o.BaseMesh.Faces)n+=f.size()*4;for(const auto& p:o.Properties)n+=p.first.size()+sizeof(ObjectProperty)+p.second.TextValue.size();}for(const auto& a:s.Sounds)n+=a.Name.size()+a.Path.size();return n;}
    void Capture(const std::vector<GameObject>&) {
        if(g_PhysicsRunning)return;
        uint64_t hash=StudioDocumentHash();if(!States.empty()&&States[Cursor].Hash==hash)return;
        StudioSnapshot s;s.Objects=g_Objects;s.Sounds=g_Sounds;std::copy(g_SkyTop,g_SkyTop+3,s.Top);std::copy(g_SkyBottom,g_SkyBottom+3,s.Bottom);s.Sky=g_SkyPreset;s.Physics=g_Physics;s.Hash=hash;
        size_t n=Size(s);if(n>128*1024*1024)return;
        while(States.size()>Cursor+1){Bytes-=Size(States.back());States.pop_back();}
        while(!States.empty()&&(States.size()>=64||Bytes+n>128*1024*1024)){Bytes-=Size(States.front());States.erase(States.begin());if(Cursor)--Cursor;}
        States.push_back(std::move(s));Cursor=States.size()-1;Bytes+=n;
    }
    void Restore() {const auto& s=States[Cursor];for(const auto& a:g_Sounds)studio_audio_unload(a.Id);g_Objects=s.Objects;g_Sounds=s.Sounds;std::copy(s.Top,s.Top+3,g_SkyTop);std::copy(s.Bottom,s.Bottom+3,g_SkyBottom);g_SkyPreset=s.Sky;g_Physics=s.Physics;g_SelectedSound=-1;g_SelectedObjectIndex=-1;g_SimulationRunning=false;}
    bool Undo(std::vector<GameObject>& objects){if(g_PhysicsRunning)return false;Capture(objects);if(States.empty()||!Cursor)return false;--Cursor;Restore();return true;}
    bool Redo(std::vector<GameObject>&){if(g_PhysicsRunning||States.empty()||Cursor+1>=States.size())return false;++Cursor;Restore();return true;}
    void Clear(){States.clear();Cursor=Bytes=0;}
};
