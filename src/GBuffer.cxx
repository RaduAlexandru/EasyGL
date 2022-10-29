#include "easy_gl/GBuffer.h"

#include <glad/glad.h>

#include <easy_gl/Texture2D.h>

//loguru
#define LOGURU_REPLACE_GLOG 1
#include <loguru.hpp>


#define MAX_TEXTURES 8


//use the maximum value of an int as invalid . We don't use negative because we sometimes compare with unsigned int
#define EGL_INVALID 2147483647

namespace gl{
GBuffer::GBuffer():
    m_width(0),
    m_height(0),
    m_fbo_id(EGL_INVALID),
    m_depth_tex("depth_gbuffer"), // add a dummy name just to have in case we throw any error
    m_has_depth_tex(false)
    {
    glGenFramebuffers(1,&m_fbo_id);


    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS , &m_max_color_attachments);
    m_textures.reserve(m_max_color_attachments); // we define a maximum amount of textures and preallocate the vector of textures because I do not want it to dinammy resize as it will cause textures to be deconstructed and constructed again which will cause the gl resources to be deleted

}

GBuffer::GBuffer(std::string name):
    GBuffer(){
    m_name=name; //we delegate the constructor to the main one but we cannot have in this intializer list more than that call.
}

GBuffer::~GBuffer(){
    // LOG(WARNING) << named("Destroying gbuffer");
    glDeleteFramebuffers(1, &m_fbo_id);
}



void GBuffer::add_texture(const std::string name, GLint internal_format, GLenum format, GLenum type){
    LOG_IF(FATAL, (int)m_textures.size()>= m_max_color_attachments-1  ) << named( name + " could not be added. Added to many textures. This will cause the vector to be dinamically resized and therefore move and destruct some of the textures. Please increase the MAX_TEXTURES #define inside the GBuffer class");
    CHECK(is_initialized()) <<"The gbuffer has to be initialized first by calling set_size()";

    m_textures.emplace_back(name);

    if(m_width!=0 && m_height!=0) { //we already used set_size() so we have some size for the whole gbuffer so we can already allocate using a certain size
        m_textures.back().allocate_storage(internal_format, format, type, m_width, m_height );
    }else{
        m_textures.back().allocate_storage(internal_format, format, type, 0, 0);
    }

    //add this new texture as an attachment to the framebuffer
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo_id);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + m_textures.size()-1, GL_TEXTURE_2D, m_textures.back().tex_id(), 0);
    m_texname2attachment[name]=m_textures.size()-1;

    // restore default FBO
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

}

void GBuffer::add_depth(const std::string name){
    CHECK(is_initialized()) <<"The gbuffer has to be initialized first by calling set_size()";

    m_has_depth_tex=true;
    m_depth_tex.set_name(name);
    if(m_width!=0 && m_height!=0){
            m_depth_tex.allocate_storage(GL_DEPTH_COMPONENT32, GL_DEPTH_COMPONENT, GL_FLOAT, m_width, m_height); //for more precise depth which translates in more precise shading because we reconstruct the position based on the depth
        // m_depth_tex.allocate_tex_storage(GL_DEPTH24_STENCIL8, GL_DEPTH_COMPONENT, GL_FLOAT, m_width, m_height); //we do't really need the stencil but it's nice to hace a fully 4 byte aligned texture
    }else{
            m_depth_tex.allocate_storage(GL_DEPTH_COMPONENT32, GL_DEPTH_COMPONENT, GL_FLOAT, 0, 0);
        // m_depth_tex.allocate_tex_storage(GL_DEPTH24_STENCIL8, GL_DEPTH_COMPONENT, GL_FLOAT,0,0);
    }

    //add this new texture as an attachment to the framebuffer
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo_id);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depth_tex.tex_id(), 0);

    // restore default FBO
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

}

void GBuffer::make_empty(const int width, const int height){
    CHECK(width!=0 && height!=0) << named("Initializing Gbuffer with 0 width and height is invalid.");

    m_width=width;
    m_height=height;
    //empty framebuffer with no attachments using GL_ARB_framebuffer_no_attachments
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_id);
    glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH, width);
    glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_HEIGHT, height);
    sanity_check();
        // restore default FBO
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GBuffer::sanity_check(){
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo_id);
    GLuint status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE){
        LOG(FATAL) << named("Framebufer is not complete.");
        return;
    }
}

void GBuffer::set_size(const int w, const int h){
    CHECK(w!=0 && h!=0) << named("Setting size of Gbuffer to 0 width and height is invalid.");

    m_width=w;
    m_height=h;

    //resize the sizes of all texures that don't have the same size
    for(size_t i=0; i<m_textures.size(); i++){
        if(m_textures[i].width()!=w || m_textures[i].height()!=h ){
            m_textures[i].resize(w,h);
        }
    }

    //resize also the depth
    if(  m_has_depth_tex && (m_depth_tex.width()!=w || m_depth_tex.height()!=h) && m_depth_tex.storage_initialized() ){
        m_depth_tex.resize(w,h);
    }

}

void GBuffer::bind() const{
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_id);
}

void GBuffer::bind_for_draw() const{
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo_id);
}

void GBuffer::bind_for_read() const{
    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo_id);
}

void GBuffer::clear(){
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo_id);

    //glClear only clears the active draw color buffers specified by glDrawBuffers https://stackoverflow.com/a/18029493
    GLenum draw_buffers[m_textures.size()];
    for(size_t i=0; i<m_textures.size(); i++){
        int attachment_nr=this->attachment_nr(m_textures[i].name());
        draw_buffers[i]=GL_COLOR_ATTACHMENT0+attachment_nr;
    }
    glDrawBuffers(m_textures.size(), draw_buffers);

    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    // clear_depth();

    // for(size_t i=0; i<m_textures.size(); i++){
    //     m_textures[i].clear();
    // }

}

void GBuffer::clear_depth(){
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo_id);
    glClearDepth(1.0);
    glClear(GL_DEPTH_BUFFER_BIT);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

void GBuffer::set_constant(float val){
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo_id);

    //glClear only clears the active draw color buffers specified by glDrawBuffers https://stackoverflow.com/a/18029493
    GLenum draw_buffers[m_textures.size()];
    for(size_t i=0; i<m_textures.size(); i++){
        int attachment_nr=this->attachment_nr(m_textures[i].name());
        draw_buffers[i]=GL_COLOR_ATTACHMENT0+attachment_nr;
    }
    glDrawBuffers(m_textures.size(), draw_buffers);

    glClearColor(val, val, val, val);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

}


int GBuffer::get_fbo_id() const{
    return m_fbo_id;
}

int GBuffer::width() const{ LOG_IF(WARNING,m_width==0) << named("Width of the gbuffer is 0"); return m_width; };
int GBuffer::height() const{ LOG_IF(WARNING,m_height==0) << named("Height of the gbuffer is 0");return m_height; };

bool GBuffer::is_initialized(){
    if(m_width==0 || m_height==0){
        return false;
    }else{
        return true;
    }
}

int GBuffer::attachment_nr(const std::string tex_name) const{
    auto it = m_texname2attachment.find(tex_name);
    if(it != std::end(m_texname2attachment)){
        //found
        return it->second;
    }else{
        LOG(FATAL) << named("Texture with name: "+tex_name + " is not added to this gbuffer");
        return 0; //HACK because this line will never occur because the previous line will kill it but we just put it to shut up the compiler warning
    }
}

Texture2D& GBuffer::tex_with_name(const std::string name){
    for(size_t i=0; i<m_textures.size(); i++){
        if(m_textures[i].name()==name){
            return m_textures[i];
        }
    }
    if(m_depth_tex.name()==name){
        return m_depth_tex;
    }
    LOG(FATAL) << named("No texture with name: " + name);
    return m_textures[0]; //HACK because this line will never occur because the previous line will kill it but we just put it to shut up the compiler warning
}

bool GBuffer::has_tex_with_name(const std::string name){
    for(size_t i=0; i<m_textures.size(); i++){
        if(m_textures[i].name()==name){
            return true;
        }
    }
    if(m_depth_tex.name()==name){
        return true;
    }
    return false;
}

std::string GBuffer::named(const std::string msg) const{
    return m_name.empty()? msg : m_name + ": " + msg;
}



}//namespace gl
