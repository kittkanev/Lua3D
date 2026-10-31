#pragma once
// OpenGL 2.1 + EXT_framebuffer_object, with core-entry-point fallbacks.
struct ViewportTarget {
    GLuint Framebuffer=0,Color=0,Depth=0;
    int Width=0,Height=0;
    using GenFn=void (APIENTRY*)(GLsizei,GLuint*);
    using DeleteFn=void (APIENTRY*)(GLsizei,const GLuint*);
    using BindFn=void (APIENTRY*)(GLenum,GLuint);
    using TextureFn=void (APIENTRY*)(GLenum,GLenum,GLenum,GLuint,GLint);
    using StorageFn=void (APIENTRY*)(GLenum,GLenum,GLsizei,GLsizei);
    using AttachFn=void (APIENTRY*)(GLenum,GLenum,GLenum,GLuint);
    using CheckFn=GLenum (APIENTRY*)(GLenum);
    GenFn GenFramebuffer=nullptr,GenRenderbuffer=nullptr;
    DeleteFn DeleteFramebuffer=nullptr,DeleteRenderbuffer=nullptr;
    BindFn BindFramebuffer=nullptr,BindRenderbuffer=nullptr;
    TextureFn AttachTexture=nullptr;
    StorageFn Storage=nullptr;
    AttachFn AttachRenderbuffer=nullptr;
    CheckFn Check=nullptr;
    template<class Function>static Function Load(const char* core,const char* extension) {
        void* pointer=SDL_GL_GetProcAddress(core);if(!pointer)pointer=SDL_GL_GetProcAddress(extension);return reinterpret_cast<Function>(pointer);
    }
    bool Initialize() {
        GenFramebuffer=Load<GenFn>("glGenFramebuffers","glGenFramebuffersEXT");GenRenderbuffer=Load<GenFn>("glGenRenderbuffers","glGenRenderbuffersEXT");
        DeleteFramebuffer=Load<DeleteFn>("glDeleteFramebuffers","glDeleteFramebuffersEXT");DeleteRenderbuffer=Load<DeleteFn>("glDeleteRenderbuffers","glDeleteRenderbuffersEXT");
        BindFramebuffer=Load<BindFn>("glBindFramebuffer","glBindFramebufferEXT");BindRenderbuffer=Load<BindFn>("glBindRenderbuffer","glBindRenderbufferEXT");
        AttachTexture=Load<TextureFn>("glFramebufferTexture2D","glFramebufferTexture2DEXT");Storage=Load<StorageFn>("glRenderbufferStorage","glRenderbufferStorageEXT");
        AttachRenderbuffer=Load<AttachFn>("glFramebufferRenderbuffer","glFramebufferRenderbufferEXT");Check=Load<CheckFn>("glCheckFramebufferStatus","glCheckFramebufferStatusEXT");
        if(!GenFramebuffer||!GenRenderbuffer||!DeleteFramebuffer||!DeleteRenderbuffer||!BindFramebuffer||!BindRenderbuffer||!AttachTexture||!Storage||!AttachRenderbuffer||!Check)return false;
        GenFramebuffer(1,&Framebuffer);GenRenderbuffer(1,&Depth);glGenTextures(1,&Color);return Resize(1,1);
    }
    bool Resize(int width,int height) {
        if(width==Width&&height==Height)return true;
        if(width<1||height<1||width>8192||height>8192)return false;
        glBindTexture(GL_TEXTURE_2D,Color);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        BindRenderbuffer(GL_RENDERBUFFER,Depth);Storage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,width,height);
        BindFramebuffer(GL_FRAMEBUFFER,Framebuffer);AttachTexture(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,Color,0);AttachRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,Depth);
        bool valid=Check(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
        BindFramebuffer(GL_FRAMEBUFFER,0);BindRenderbuffer(GL_RENDERBUFFER,0);glBindTexture(GL_TEXTURE_2D,0);
        if(valid){Width=width;Height=height;}return valid;
    }
    void Destroy() {
        if(Framebuffer&&DeleteFramebuffer)DeleteFramebuffer(1,&Framebuffer);
        if(Depth&&DeleteRenderbuffer)DeleteRenderbuffer(1,&Depth);
        if(Color)glDeleteTextures(1,&Color);
        Framebuffer=Color=Depth=0;Width=Height=0;
    }
};
static ViewportTarget g_ViewportTarget;
