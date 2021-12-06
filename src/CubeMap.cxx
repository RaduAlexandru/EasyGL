#include "easy_gl/CubeMap.h"

#include <glad/glad.h>

#include <iostream>

#include "opencv2/opencv.hpp"

#include "easy_gl/UtilsGL.h"

//use the maximum value of an int as invalid . We don't use negative because we sometimes compare with unsigned int
#define EGL_INVALID 2147483647

namespace gl{

CubeMap::CubeMap():
    m_width(0),
    m_height(0),
    m_tex_id(EGL_INVALID),
    m_tex_storage_initialized(false),
    m_tex_storage_inmutable(false),
    m_internal_format(EGL_INVALID),
    m_format(EGL_INVALID),
    m_type(EGL_INVALID),
    m_idx_mipmap_allocated(0),
    m_fbos_for_mips(16, EGL_INVALID){
    glGenTextures(1,&m_tex_id);

    //start with some sensible parameter initialziations
    set_wrap_mode(GL_CLAMP_TO_EDGE);
    set_filter_mode_min_mag(GL_LINEAR);
    // set_filter_mode(GL_NEAREST);

    //framebuffers that points towards all the mip maps. We create only the FBO pointing at mipmap 0 and the other ones will be created dinamically when calling fbo_id( mip )
    fbo_id(0);

    //for clearing we use a framebuffer that will look through all of the 6 faces and clear them all https://github.com/JoeyDeVries/LearnOpenGL/blob/master/src/6.pbr/2.1.1.ibl_irradiance_conversion/ibl_irradiance_conversion.cpp
    // glGenFramebuffers(1,&m_fbo_for_clearing_id);
    // glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo_for_clearing_id);
    // glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X+0, m_tex_id, 0);
    // glDrawBuffer(GL_COLOR_ATTACHMENT0); //Only need to do this once.
    // glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

CubeMap::CubeMap(std::string name):
    CubeMap(){
    m_name=name; //we delegate the constructor to the main one but we cannot have in this intializer list more than that call.
}

CubeMap::~CubeMap(){
    // LOG(WARNING) << named("Destroying texture");
    glDeleteTextures(1, &m_tex_id);
}


void CubeMap::set_name(const std::string name){
    m_name=name;
}

std::string CubeMap::name() const{
    return m_name;
}

void CubeMap::set_wrap_mode(const GLenum wrap_mode){
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_tex_id);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, wrap_mode);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, wrap_mode);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, wrap_mode);
}

void CubeMap::set_filter_mode_min_mag(const GLenum filter_mode){
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_tex_id);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, filter_mode);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, filter_mode);
}

void CubeMap::set_filter_mode_min(const GLenum filter_mode){
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_tex_id);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, filter_mode);
}

void CubeMap::set_filter_mode_mag(const GLenum filter_mode){
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_tex_id);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, filter_mode);
}


void CubeMap::resize(const int w, const int h){
    LOG_IF(FATAL, w==0 && h==0) << named("Resizing texture with 0 size width and height is invalid.");
    CHECK(m_internal_format!=EGL_INVALID) << named("Cannot resize without knowing the internal format. You should previously allocate storage for the texture using allocate_texture_storage or upload_data if you have any");
    CHECK(m_format!=EGL_INVALID) << named("Cannot resize without knowing the format. You should previously allocate storage for the texture using allocate_texture_storage or upload_data if you have any");
    CHECK(m_type!=EGL_INVALID) << named("Cannot resize without knowing the texture type. You should previously allocate storage for the texture using allocate_texture_storage or upload_data if you have any");

    m_width=w;
    m_height=h;


    glBindTexture(GL_TEXTURE_CUBE_MAP, m_tex_id);
    for(int i=0; i<6; i++){
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, m_internal_format, w, h, 0, m_format, m_type, 0);
    }
}


//allocate mutable texture storage and leave it uninitialized
void CubeMap::allocate_tex_storage(GLenum internal_format, GLenum format, GLenum type, GLsizei width, GLsizei height){
    CHECK(is_internal_format_valid(internal_format)) << named("Internal format not valid");
    CHECK(is_format_valid(format)) << named("Format not valid");
    CHECK(is_type_valid(type)) << named("Type not valid");

    m_width=width;
    m_height=height;
    m_internal_format=internal_format;
    m_format=format;
    m_type=type;

    // bind the texture
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_tex_id);

    for(int i=0; i<6; i++){
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, m_internal_format, width, height, 0, m_format, m_type, 0);
    }
    m_tex_storage_initialized=true;
}


//allocate inmutable texture storage
void CubeMap::allocate_tex_storage_inmutable(GLenum internal_format, GLenum format, GLenum type, GLsizei width, GLsizei height){
    CHECK(is_internal_format_valid(internal_format)) << named("Internal format not valid");
    CHECK(is_format_valid(format)) << named("Format not valid");
    CHECK(is_type_valid(type)) << named("Type not valid");
    CHECK(!m_tex_storage_inmutable) << named("You already allocated texture as inmutable. To resize you can delete and recreate the texture or use mutable storage with allocate_tex_storage()");
    m_width=width;
    m_height=height;
    m_internal_format=internal_format;
    m_format=format; //these are not really needed for allocating an inmutable texture but it's nice to have
    m_type=type; //also not really needed but its nice to have

    glBindTexture(GL_TEXTURE_CUBE_MAP, m_tex_id);
    for(int i=0; i<6; i++){
        glTexStorage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 1, internal_format, width, height);
    }
    m_tex_storage_initialized=true;
    m_tex_storage_inmutable=true;
}

void CubeMap::allocate_or_resize(GLenum internal_format, GLenum format, GLenum type, GLsizei width, GLsizei height){
    CHECK(is_internal_format_valid(internal_format)) << named("Internal format not valid");
    CHECK(is_format_valid(format)) << named("Format not valid");
    CHECK(is_type_valid(type)) << named("Type not valid");

    if(!m_tex_storage_initialized){
        allocate_tex_storage(internal_format, format, type, width, height ); //never initialized so we allocate some storage for it
    }else if(m_tex_storage_initialized && (m_width!=width || m_height!=height ) ){
        resize( width, height ); // initialized but the texture changed size and we have to resize the buffer
    }
}

//clears the texture to zero
void CubeMap::clear(){
    CHECK(m_format!=EGL_INVALID) << named("Format was not initialized");
    CHECK(m_type!=EGL_INVALID) << named("Type was not initialized");

    //it is for some reason super slow (5 to 6 ms)
    // glClearTexImage(m_tex_id, 0, m_format, m_type, nullptr);

    //a ton faster(less than 1 ms)
    set_constant(0.0);

}

void CubeMap::set_constant(float val){
    CHECK(m_format!=EGL_INVALID) << named("Format was not initialized");
    CHECK(m_type!=EGL_INVALID) << named("Type was not initialized");

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_id(0) );
    glClearColor(val, val, val, val);
    for(int i=0; i<6; i++){
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_tex_id, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    //if we have mip map levels we have to clear also the lower levels
    if(m_idx_mipmap_allocated!=0){
        generate_mipmap(m_idx_mipmap_allocated);
    }

}

void CubeMap::set_constant(float val, float val_alpha){
    CHECK(m_format!=EGL_INVALID) << named("Format was not initialized");
    CHECK(m_type!=EGL_INVALID) << named("Type was not initialized");

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_id(0) );
    glClearColor(val, val, val, val_alpha);
    for(int i=0; i<6; i++){
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_tex_id, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    //if we have mip map levels we have to clear also the lower levels
    if(m_idx_mipmap_allocated!=0){
        generate_mipmap(m_idx_mipmap_allocated);
    }

}

void CubeMap::generate_mipmap(const int idx_max_lvl){
    glActiveTexture(GL_TEXTURE0);
    bind();
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP,  GL_TEXTURE_MAX_LEVEL, idx_max_lvl);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    m_idx_mipmap_allocated=idx_max_lvl;
}

//creates the full chain of mip map, up until the smallest possible texture
void CubeMap::generate_mipmap_full(){
    int idx_max_lvl=mipmap_highest_idx();
    generate_mipmap(idx_max_lvl);
}


void CubeMap::bind() const{
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_tex_id);
}

int CubeMap::tex_id() const{
    return m_tex_id;
}

bool CubeMap::storage_initialized () const{
    return m_tex_storage_initialized;
}

GLint CubeMap::internal_format() const{
    CHECK(m_internal_format!=EGL_INVALID) << named("The texture has not been initialzied and doesn't yet have a format");
    return m_internal_format;
}

GLuint CubeMap::fbo_id(const int mip) {
    // return m_fbo_for_clearing_id;
        //check if the mip we are accesing is withing the range of mip maps we have allocated
    CHECK(mip<mipmap_nr_levels_allocated()) << "mipmap idx " << mip << " is smaller than the nr of mips we have allocated which is " << mipmap_nr_levels_allocated();

    //check if the fbo for this mip is created
    if(m_fbos_for_mips[mip]==EGL_INVALID){
        //the fbo is not created so we create it
        glGenFramebuffers(1,&m_fbos_for_mips[mip]);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbos_for_mips[mip]);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X+0, m_tex_id, mip);
        glDrawBuffer(GL_COLOR_ATTACHMENT0); //Only need to do this once.
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    }

    return m_fbos_for_mips[mip];
}


int CubeMap::width() const{ return m_width; }
int CubeMap::height() const{ return m_height; }
int CubeMap::width_for_lvl(const int lvl) const{ return std::max(1, (int)floor(m_width /  (int)pow(2,lvl)  )); }
int CubeMap::height_for_lvl(const int lvl) const{ return std::max(1,  (int)floor(m_height /  (int)pow(2,lvl)  ));  }
int CubeMap::channels() const{
    CHECK(m_format!=EGL_INVALID) << named("Format was not initialized");
    switch(m_format) {
        case GL_RED : return 1; break;
        case GL_RG : return 2; break;
        case GL_RGB : return 3; break;
        case GL_RGBA : return 4; break;
        default : LOG(FATAL) << "We don't know how many channels does this format have."; return 0; break;
    }
}

//returns the index of the highest mip map lvl (this is used to plug into generate_mip_map)
int CubeMap::mipmap_highest_idx() const { return floor(log2(  std::max(m_width, m_height)  ));  }
//return maximum number of mip map lvls, effectivelly it is mipmap_highest_idx+1
int CubeMap::mipmap_nr_lvls() const{ return mipmap_highest_idx()+1; }
int CubeMap::mipmap_nr_levels_allocated() const{ return m_idx_mipmap_allocated+1;}


std::string CubeMap::named(const std::string msg) const{
    return m_name.empty()? msg : m_name + ": " + msg;
}


}//namespace gl