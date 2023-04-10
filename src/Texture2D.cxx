#include "easy_gl/Texture2D.h"


#include <glad/glad.h>


// DO NOT USE A IFDEF because other C++ libs may include this Viewer.h without the compile definitions and therefore the Viewer.h that was used to compile easypbr and the one included will be different leading to issues
#ifdef EASYPBR_WITH_TORCH
    #include "torch/torch.h"
    #include <cuda_gl_interop.h>
#endif

#include <iostream>

#include "opencv2/opencv.hpp"

#include "easy_gl/UtilsGL.h"
#include "easy_gl/Buf.h"



//use the maximum value of an int as invalid . We don't use negative because we sometimes compare with unsigned int
#define EGL_INVALID 2147483647

namespace gl{

Texture2D::Texture2D():
    m_width(0),
    m_height(0),
    m_tex_id(EGL_INVALID),
    m_tex_storage_initialized(false),
    m_tex_storage_inmutable(false),
    m_internal_format(EGL_INVALID),
    m_format(EGL_INVALID),
    m_type(EGL_INVALID),
    m_idx_mipmap_allocated(0),
    m_nr_pbos_upload(2),
    m_cur_pbo_upload_idx(0),
    m_nr_pbos_download(3),
    m_cur_pbo_download_idx(0),
    m_fbos_for_mips(16, EGL_INVALID),
    m_cuda_transfer_enabled(false){
    glGenTextures(1,&m_tex_id);

    //create some pbos used for uploading to the texture
    m_pbos_upload.resize(m_nr_pbos_upload);
    for(int i=0; i<m_nr_pbos_upload; i++){
        m_pbos_upload[i].set_target(GL_PIXEL_UNPACK_BUFFER);
    }
    //create some pbos used to download this texture to cpu
    m_pbos_download.resize(m_nr_pbos_download);
    for(int i=0; i<m_nr_pbos_download; i++){
        m_pbos_download[i].set_target(GL_PIXEL_PACK_BUFFER);
    }

    //initializing a texture requires setting the mip map levels  https://www.khronos.org/opengl/wiki/Common_Mistakes
    bind();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    //start with some sensible parameter initialziations
    set_wrap_mode(GL_CLAMP_TO_EDGE);
    set_filter_mode_min_mag(GL_LINEAR);
    // set_filter_mode(GL_NEAREST);

    //framebuffers that points towards all the mip maps. We create only the FBO pointing at mipmap 0 and the other ones will be created dinamically when calling fbo_id( mip )
    fbo_id(0);

    // bind();
    // cudaGraphicsGLRegisterImage(&m_cuda_resource, m_tex_id, GL_TEXTURE_2D, cudaGraphicsRegisterFlagsNone);


    // glGenFramebuffers(1,&m_fbo_for_clearing_id);
    // glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo_for_clearing_id);
    // glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_tex_id, 0);
    // glDrawBuffer(GL_COLOR_ATTACHMENT0); //Only need to do this once.
    // glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

Texture2D::Texture2D(std::string name):
    Texture2D(){
    m_name=name; //we delegate the constructor to the main one but we cannot have in this intializer list more than that call.
}

Texture2D::~Texture2D(){
    // LOG(WARNING) << named("Destroying texture");
    // cudaGraphicsUnregisterResource(m_cuda_resource);
    #ifdef EASYPBR_WITH_TORCH
        disable_cuda_transfer();
    #endif

    glDeleteTextures(1, &m_tex_id);

    for(size_t i=0; i<m_fbos_for_mips.size(); i++){
        if (m_fbos_for_mips[i]!=EGL_INVALID){
            glDeleteFramebuffers(1, &m_fbos_for_mips[i]);
            m_fbos_for_mips[i]=EGL_INVALID;
        }
    }


}


void Texture2D::set_name(const std::string name){
    m_name=name;
}

std::string Texture2D::name() const{
    return m_name;
}

void Texture2D::set_wrap_mode(const GLenum wrap_mode){
    glBindTexture(GL_TEXTURE_2D, m_tex_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_mode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_mode);
}

void Texture2D::set_filter_mode_min_mag(const GLenum filter_mode){
    glBindTexture(GL_TEXTURE_2D, m_tex_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter_mode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter_mode);
}

void Texture2D::set_filter_mode_min(const GLenum filter_mode){
    glBindTexture(GL_TEXTURE_2D, m_tex_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter_mode);
}

void Texture2D::set_filter_mode_mag(const GLenum filter_mode){
    glBindTexture(GL_TEXTURE_2D, m_tex_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter_mode);
}

#ifdef EASYPBR_WITH_TORCH
    void Texture2D::enable_cuda_transfer(){ //enabling cuda transfer has a performance cost for allocating memory of the texture so we leave this as optional
        if(!m_cuda_transfer_enabled){
            m_cuda_transfer_enabled=true;
            register_for_cuda();
        }
    }

    void Texture2D::disable_cuda_transfer(){
        m_cuda_transfer_enabled=false;
        unregister_cuda();
    }

    void Texture2D::register_for_cuda(){
        unregister_cuda(); 
        if (m_tex_storage_initialized){
            bind();
            cudaGraphicsGLRegisterImage(&m_cuda_resource, m_tex_id, GL_TEXTURE_2D, cudaGraphicsRegisterFlagsNone);
        }
    }

    void Texture2D::unregister_cuda(){
        if(m_cuda_resource){
            cudaGraphicsUnregisterResource(m_cuda_resource);
            m_cuda_resource=nullptr;
        }
    }
#endif


void Texture2D::upload_data(GLint internal_format, GLenum format, GLenum type, GLsizei width, GLsizei height,  const void* data_ptr, int size_bytes){
    CHECK(is_internal_format_valid(internal_format)) << named("Internal format not valid");
    CHECK(is_format_valid(format)) << named("Format not valid");
    CHECK(is_type_valid(type)) << named("Type not valid");

    //if the width is not divisible by 4 we need to change the packing alignment https://www.khronos.org/opengl/wiki/Common_Mistakes#Texture_upload_and_pixel_reads
    if( (format==GL_RGB || format==GL_BGR || format==GL_RED) && width%4!=0){
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    }


    // bind the texture and PBO
    GL_C( glBindTexture(GL_TEXTURE_2D, m_tex_id) );
    Buf& pbo_upload=m_pbos_upload[m_cur_pbo_upload_idx];
    pbo_upload.bind();


    if(!pbo_upload.storage_initialized() || pbo_upload.width()!=width || pbo_upload.height()!=height){
        pbo_upload.allocate_storage(size_bytes, GL_STREAM_DRAW);
        pbo_upload.set_width(width);
        pbo_upload.set_height(height);
    }
    // if(!m_tex_storage_initialized || m_width!=width || m_height!=height){
        // GL_C( glTexImage2D(GL_TEXTURE_2D, 0, internal_format,width,height,0,format,type,0) ); //allocate storage texture
        // m_tex_storage_initialized=true;
    // }
    allocate_or_resize(internal_format, format, type, width, height);

    //this assignments need to be here because only here we actually create the storage and we check that m_width!=width
    m_width=width;
    m_height=height;
    m_internal_format=internal_format;
    m_format=format;
    m_type=type;



    //update the pbo fast, mapping and doing a memcpy is slower, it is faster to do a upload_subdata
    pbo_upload.upload_sub_data(size_bytes, data_ptr);


    // copy pixels from PBO to texture object (this returns inmediatelly and lets the GPU perform DMA at a later time)
    GL_C( glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, format, type, 0) );


    // it is good idea to release PBOs with ID 0 after use. Once bound with 0, all pixel operations behave normal ways.
    pbo_upload.unbind();

    m_cur_pbo_upload_idx=(m_cur_pbo_upload_idx+1)%m_nr_pbos_upload;

    //change back to unpack alignment of 4 which would be the default in case we changed it before
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
}


//easy way to just get a cv mat there, does internally an upload to pbo and schedules a dma
//by default the values will get transfered to the gpu and get normalized to [0,1] therefore an rgb texture of unsigned bytes will be read as floats from the shader with sampler2D. However sometimes we might want to use directly the integers stored there, for example when we have a semantic texture and the nr range from [0,nr_classes]. Then we set normalize to false and in the shader we acces the texture with usampler2D
void Texture2D::upload_from_cv_mat(const cv::Mat& cv_mat, const bool flip_red_blue, const bool store_as_normalized_vals){
    //TODO needs to be rechecked as we now store as class member the internal format, format and type

    CHECK(cv_mat.data) << "cv_mat is empty";

    //from the cv format get the corresponding gl internal_format, format and type
    GLint internal_format=EGL_INVALID;
    GLenum format=EGL_INVALID;
    GLenum type=EGL_INVALID;
    cv_type2gl_formats(internal_format, format, type ,cv_mat.type(), flip_red_blue, store_as_normalized_vals);

    CHECK(is_internal_format_valid(internal_format)) << named("Internal format not valid");
    CHECK(is_format_valid(format)) << named("Format not valid");
    CHECK(is_type_valid(type)) << named("Type not valid");
    if (m_internal_format!=EGL_INVALID){ //if we already have a format, it should be compatible with the one we are using for uploding
        CHECK(m_internal_format==internal_format) << "Previously defined internal format is not the same as the one which will be used for the opencv image upload";
    }

    //do the upload to the pbo
    int size_bytes=cv_mat.step[0] * cv_mat.rows;
    upload_data(internal_format, format, type, cv_mat.cols, cv_mat.rows, cv_mat.ptr(), size_bytes);
}


// void upload_without_pbo(GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* data_ptr){
    // TODO this needs to be redone because I dont like that it takes the xoffset and y offset and so on. We should have an overloaded function that just is upload_without_pbo(data_ptr) which assumes that the texture already has a storage and it can do TexSubimage and another more complete that take also the internal_format, format and type and does teximage2d
//     CHECK(m_tex_storage_initialized) << named("Texture storage was not initialized. Uploading without pbo assumes we have already a storage allocated");

//     m_internal_format=internal_format;
//     m_format=format;

//     glBindTexture(GL_TEXTURE_2D, m_tex_id);
//     glTexSubImage2D(GL_TEXTURE_2D,level,xoffset,yoffset,width,height,format,type,data_ptr);
// }

void Texture2D::resize(const int w, const int h){
    LOG_IF(FATAL, w==0 && h==0) << named("Resizing texture with 0 size width and height is invalid.");
    CHECK(m_internal_format!=EGL_INVALID) << named("Cannot resize without knowing the internal format. You should previously allocate storage for the texture using allocate_texture_storage or upload_data if you have any");
    CHECK(m_format!=EGL_INVALID) << named("Cannot resize without knowing the format. You should previously allocate storage for the texture using allocate_texture_storage or upload_data if you have any");
    CHECK(m_type!=EGL_INVALID) << named("Cannot resize without knowing the texture type. You should previously allocate storage for the texture using allocate_texture_storage or upload_data if you have any");

    m_width=w;
    m_height=h;


    glBindTexture(GL_TEXTURE_2D, m_tex_id);

    #ifdef EASYPBR_WITH_TORCH
        if (m_cuda_transfer_enabled){
            unregister_cuda();
        }
    #endif


    glTexImage2D(GL_TEXTURE_2D, 0, m_internal_format, w, h, 0, m_format, m_type, 0); //allocate storage texture

    //if we have mip map levels we have to regenerate the memory for them too
    if(m_idx_mipmap_allocated!=0){
        generate_mipmap(m_idx_mipmap_allocated);
    }

    //update the cuda resource since we have changed the memory of the texture
    #ifdef EASYPBR_WITH_TORCH
        if (m_cuda_transfer_enabled){
            register_for_cuda();
        }
    #endif
}


//allocate mutable texture storage and leave it uninitialized
void Texture2D::allocate_storage(GLenum internal_format, GLenum format, GLenum type, GLsizei width, GLsizei height){
    CHECK(is_internal_format_valid(internal_format)) << named("Internal format not valid");
    CHECK(is_format_valid(format)) << named("Format not valid");
    CHECK(is_type_valid(type)) << named("Type not valid");

    m_width=width;
    m_height=height;
    m_internal_format=internal_format;
    m_format=format;
    m_type=type;

    // bind the texture
    glBindTexture(GL_TEXTURE_2D, m_tex_id);

    #ifdef EASYPBR_WITH_TORCH
        if (m_cuda_transfer_enabled){
            unregister_cuda();
        }
    #endif

    glTexImage2D(GL_TEXTURE_2D, 0, internal_format,width,height,0,format,type,0); //allocate storage texture
    m_tex_storage_initialized=true;

    //if we have mip map levels we have to regenerate the memory for them too
    if(m_idx_mipmap_allocated!=0){
        generate_mipmap(m_idx_mipmap_allocated);
    }

    //update the cuda resource since we have changed the memory of the texture
    #ifdef EASYPBR_WITH_TORCH
        if (m_cuda_transfer_enabled){
            register_for_cuda();
        }
    #endif
}


//allocate inmutable texture storage
void Texture2D::allocate_storage_inmutable(GLenum internal_format, GLenum format, GLenum type, GLsizei width, GLsizei height){
    CHECK(is_internal_format_valid(internal_format)) << named("Internal format not valid");
    CHECK(is_format_valid(format)) << named("Format not valid");
    CHECK(is_type_valid(type)) << named("Type not valid");
    CHECK(!m_tex_storage_inmutable) << named("You already allocated texture as inmutable. To resize you can delete and recreate the texture or use mutable storage with allocate_tex_storage()");
    m_width=width;
    m_height=height;
    m_internal_format=internal_format;
    m_format=format; //these are not really needed for allocating an inmutable texture but it's nice to have
    m_type=type; //also not really needed but its nice to have

    glBindTexture(GL_TEXTURE_2D, m_tex_id);
    glTexStorage2D(GL_TEXTURE_2D, 1, internal_format, width, height);
    m_tex_storage_initialized=true;
    m_tex_storage_inmutable=true;
}

void Texture2D::allocate_or_resize(GLenum internal_format, GLenum format, GLenum type, GLsizei width, GLsizei height){
    CHECK(is_internal_format_valid(internal_format)) << named("Internal format not valid");
    CHECK(is_format_valid(format)) << named("Format not valid");
    CHECK(is_type_valid(type)) << named("Type not valid");

    if(!m_tex_storage_initialized){
        allocate_storage(internal_format, format, type, width, height ); //never initialized so we allocate some storage for it
    }else if(m_tex_storage_initialized && (m_width!=width || m_height!=height ) ){
        resize( width, height ); // initialized but the texture changed size and we have to resize the buffer
    }
}

// //uploads data from cpu to pbo (on gpu) will take some cpu time to perform the memcpy
// void upload_to_pbo(const void* data_ptr, int size_bytes){
//     // bind the PBO
//     glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pbo_id);
//
//
//     //allocate storage for pbo
//     if(!m_pbo_storage_initialized){
//         glBufferData(GL_PIXEL_UNPACK_BUFFER, size_bytes, NULL, GL_STREAM_DRAW); //allocate storage for pbo
//         m_pbo_storage_initialized=true;
//     }
//
//
//     //update pbo
//     glBufferData(GL_PIXEL_UNPACK_BUFFER, size_bytes, 0, GL_STREAM_DRAW); //orphan the pbo storage
//     GLubyte* ptr = (GLubyte*)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
//     if(ptr){
//         memcpy(ptr, data_ptr, size_bytes);
//         glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER); // release pointer to mapping buffer
//     }
//
//     glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
// }

// //uploads the data of the pbo (on the gpu already) to the texture (also on the gpu). Should return inmediatelly
// void upload_pbo_to_tex( GLsizei width, GLsizei height,
//                         GLenum format, GLenum type){
//     // bind the PBO and texture
//     glBindTexture(GL_TEXTURE_2D, m_tex_id);
//     glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pbo_id);
//
//     // copy pixels from PBO to texture object (this returns inmediatelly and lets the GPU perform DMA at a later time)
//     glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, format, type, 0);
//
//
//     // it is good idea to release PBOs with ID 0 after use.
//     // Once bound with 0, all pixel operations behave normal ways.
//     glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
// }

// //uploads the data of the pbo (on the gpu already) to the texture (also on the gpu). Should return inmediatelly
// void upload_pbo_to_tex_no_binds( GLsizei width, GLsizei height,
//                                  GLenum format, GLenum type){

//     //TODO needs some refactoring to not take as parameters the width and the height and the format type and so one, we should have a function that just takes the width height and format type from the class member is they are available, if they are not they should be set

//     // copy pixels from PBO to texture object (this returns inmediatelly and lets the GPU perform DMA at a later time)
//     glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, format, type, 0);

// }


// //opengl stores it as floats which are in range [0,1]. By default we return them as such, othewise we denormalize them to the range [0,255]
// cv::Mat download_to_cv_mat(const bool denormalize=false){
//     //TODO now that the class stores internally the format type and eveything, we should need to use this ffunction that translates from internal_format to format and type
//     CHECK(m_tex_storage_initialized) << named("Texture storage was not initialized. Cannot download to an opencv mat");
//     CHECK(m_internal_format!=EGL_INVALID) << named("Internal format was not initialized");

//     bind();
//     //get the width and height of the gl_texture
//     int w, h;
//     int miplevel = 0;
//     glGetTexLevelParameteriv(GL_TEXTURE_2D, miplevel, GL_TEXTURE_WIDTH, &w);
//     glGetTexLevelParameteriv(GL_TEXTURE_2D, miplevel, GL_TEXTURE_HEIGHT, &h);

//     //create the cv_mat and
//     std::cout << "m_internal_format is " << m_internal_format << '\n';
//     int cv_type=gl_internal_format2cv_type(m_internal_format);
//     GLenum format;
//     GLenum type;
//     gl_internal_format2format_and_type(format,type, m_internal_format, denormalize);
//     cv::Mat cv_mat(h, w, cv_type);
//     glGetTexImage(GL_TEXTURE_2D,0, format, type, cv_mat.data);
//     cv_mat*=255; //go from range [0,1] to [0,255];

//     return cv_mat;
// }

//transfers data from the texture into a pbo. Useful for later reading the pbo with download_from_oldest_pbo() and transfering to cpu without stalling the pipeline
void Texture2D::download_to_pbo(){
    CHECK(storage_initialized()) << named("Texture storage not initialized");

    //if the width is not divisible by 4 we need to change the packing alignment https://www.khronos.org/opengl/wiki/Common_Mistakes#Texture_upload_and_pixel_reads
    if( (m_format==GL_RGB || m_format==GL_BGR || m_format==GL_RED) && width()%4!=0){
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    }


    // bind the texture and PBO
    GL_C( bind() );
    Buf& pbo_download=m_pbos_download[m_cur_pbo_download_idx];
    pbo_download.bind();


    if(!pbo_download.storage_initialized() || pbo_download.width()!=width() || pbo_download.height()!=height()){
        int size_bytes=width()*height()*channels()*bytes_per_element();
        pbo_download.allocate_storage(size_bytes, GL_STREAM_DRAW);
        pbo_download.set_width(width());
        pbo_download.set_height(height());
    }


    //transfer from texture to pbo
    glGetTexImage(GL_TEXTURE_2D, 0, m_format, m_type, 0);


    // it is good idea to release PBOs with ID 0 after use. Once bound with 0, all pixel operations behave normal ways.
    pbo_download.unbind();
    m_cur_pbo_download_idx=(m_cur_pbo_download_idx+1)%m_nr_pbos_download;
    //change back to unpack alignment of 4 which would be the default in case we changed it before
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    unbind(); //unbind also the texture
}

void Texture2D::download_from_oldest_pbo(void* data_out){
    //if the width is not divisible by 4 we need to change the packing alignment https://www.khronos.org/opengl/wiki/Common_Mistakes#Texture_upload_and_pixel_reads
    if( (m_format==GL_RGB || m_format==GL_BGR || m_format==GL_RED) && width()%4!=0){
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    }

    // bind the PBO and copy the dtaa from it
    Buf& pbo_download=m_pbos_download[m_cur_pbo_download_idx];
    if(pbo_download.storage_initialized()){
        pbo_download.bind();

        void* data = glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
        memcpy(data_out, data, pbo_download.size_bytes());
        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
        pbo_download.unbind();

    }

    //change back to unpack alignment of 4 which would be the default in case we changed it before
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
}

Buf& Texture2D::cur_pbo_download(){
    return m_pbos_download[m_cur_pbo_download_idx];
}



//clears the texture to zero
void Texture2D::clear(){
    CHECK(m_format!=EGL_INVALID) << named("Format was not initialized");
    CHECK(m_type!=EGL_INVALID) << named("Type was not initialized");

    //it is for some reason super slow (5 to 6 ms)
    // glClearTexImage(m_tex_id, 0, m_format, m_type, nullptr);

    //a ton faster(less than 1 ms)
    set_constant(0.0);

}

void Texture2D::set_constant(float val){
    CHECK(m_format!=EGL_INVALID) << named("Format was not initialized");
    CHECK(m_type!=EGL_INVALID) << named("Type was not initialized");



    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_id(0) );

    //glClear only clears the active draw color buffers specified by glDrawBuffers https://stackoverflow.com/a/18029493
    GLenum draw_buffers[1];
    draw_buffers[0]=GL_COLOR_ATTACHMENT0+0;
    glDrawBuffers(1, draw_buffers);


    glClearColor(val, val, val, val);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    //if we have mip map levels we have to clear also the lower levels
    if(m_idx_mipmap_allocated!=0){
        generate_mipmap(m_idx_mipmap_allocated);
    }


}

void Texture2D::set_constant(float val, float val_alpha){
    CHECK(m_format!=EGL_INVALID) << named("Format was not initialized");
    CHECK(m_type!=EGL_INVALID) << named("Type was not initialized");

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_id(0) );

    //glClear only clears the active draw color buffers specified by glDrawBuffers https://stackoverflow.com/a/18029493
    GLenum draw_buffers[1];
    draw_buffers[0]=GL_COLOR_ATTACHMENT0+0;
    glDrawBuffers(1, draw_buffers);

    glClearColor(val, val, val, val_alpha);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    //if we have mip map levels we have to clear also the lower levels
    if(m_idx_mipmap_allocated!=0){
        generate_mipmap(m_idx_mipmap_allocated);
    }

}

void Texture2D::set_val(const float r, const float g, const float b, const float alpha){
    CHECK(m_format!=EGL_INVALID) << named("Format was not initialized");
    CHECK(m_type!=EGL_INVALID) << named("Type was not initialized");

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_id(0) );

    //glClear only clears the active draw color buffers specified by glDrawBuffers https://stackoverflow.com/a/18029493
    GLenum draw_buffers[1];
    draw_buffers[0]=GL_COLOR_ATTACHMENT0+0;
    glDrawBuffers(1, draw_buffers);

    glClearColor(r, g, b, alpha);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    //if we have mip map levels we have to clear also the lower levels
    if(m_idx_mipmap_allocated!=0){
        generate_mipmap(m_idx_mipmap_allocated);
    }

}


//opengl stores it as floats which are in range [0,1]. By default we return them as such, othewise we denormalize them to the range [0,255]
cv::Mat Texture2D::download_to_cv_mat(const int lvl, const bool denormalize){
    //TODO now that the class stores internally the format type and eveything, we should need to use this ffunction that translates from internal_format to format and type
    CHECK(m_tex_storage_initialized) << named("Texture storage was not initialized. Cannot download to an opencv mat");
    CHECK(m_internal_format!=EGL_INVALID) << named("Internal format was not initialized");
    CHECK(m_format!=EGL_INVALID) << named("Format was not initialized");
    CHECK(m_type!=EGL_INVALID) << named("Type was not initialized");
    //check that the lvl is in a correct range
    CHECK(lvl>=0 && lvl<=mipmap_highest_idx()) << "Mip map must be in range [0,mipmap_highest_idx()]. So the max lvl idx is: " << mipmap_highest_idx() << " but the input lvl is" << lvl;

    //if the width is not divisible by 4 we need to change the packing alignment https://www.khronos.org/opengl/wiki/Common_Mistakes#Texture_upload_and_pixel_reads
    if( (m_format==GL_RGB || m_format==GL_BGR || m_format==GL_RED) && m_width%4!=0){
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
    }


    bind();

    //create the cv_mat and
    int cv_type=gl_internal_format2cv_type(m_internal_format);
    //calculate the width and height of the texture at this lvl
    cv::Mat cv_mat( height_for_lvl(lvl) , width_for_lvl(lvl) , cv_type);

    //download from gpu into the cv memory
    glGetTexImage(GL_TEXTURE_2D,lvl, m_format, m_type, cv_mat.data);
    if(denormalize){
        cv_mat*=255; //go from range [0,1] to [0,255];
    }

    //restore the packing alignment back to 4 as per default
    glPixelStorei(GL_PACK_ALIGNMENT, 4);

    unbind();


    return cv_mat;
}

#ifdef EASYPBR_WITH_TORCH
    void Texture2D::from_tensor(const torch::Tensor& tensor, const bool flip_red_blue, const bool store_as_normalized_vals){
        CHECK(m_cuda_transfer_enabled) << "You must enable first the cuda transfer with tex.enable_cuda_transfer(). This incurrs a performance cost for memory realocations so try to keep the texture in memory mostly unchanged.";

        //check that the tensor is in format nchw
        CHECK(tensor.dim()==4 ) << "tensor should have 4 dimensions correponding to NCHW but it has " << tensor.dim();
        CHECK(tensor.size(0)==1 ) << "tensor first dimension should be 1. So it should be only 1 batch but it is " << tensor.size(0);
        CHECK(tensor.size(1)==1 || tensor.size(1)==2 || tensor.size(1)==4 ) << "tensor second dimension should be 1,2,or 4 but it is " << tensor.size(1); //cuda doesnt support 3 channels as explained in the function cudaGraphicsGLRegisterImage here https://docs.nvidia.com/cuda/cuda-runtime-api/group__CUDART__OPENGL.html#group__CUDART__OPENGL_1g80d12187ae7590807c7676697d9fe03d
        CHECK(tensor.scalar_type()==torch::kFloat32 || tensor.scalar_type()==torch::kUInt8 || tensor.scalar_type()==torch::kInt32 ) << "Tensor should be of type float uint8 or int32. However it is " << tensor.scalar_type();
        CHECK(tensor.device().is_cuda() ) << "tensor should be on the GPU but it is on device " << tensor.device();

        int c=tensor.size(1);
        int h=tensor.size(2);
        int w=tensor.size(3);


        //from the cv format get the corresponding gl internal_format, format and type
        GLint internal_format=EGL_INVALID;
        GLenum format=EGL_INVALID;
        GLenum type=EGL_INVALID;
        tensor_type2gl_formats(internal_format, format, type, c, tensor.scalar_type(),  flip_red_blue, store_as_normalized_vals);

        CHECK(is_internal_format_valid(internal_format)) << named("Internal format not valid");
        CHECK(is_format_valid(format)) << named("Format not valid");
        CHECK(is_type_valid(type)) << named("Type not valid");


        //if the texture is already initialized, check if the types and sizes match
        if(m_tex_storage_initialized){
            if (m_internal_format!=EGL_INVALID){ //if we already have a format, it should be compatible with the one we are using for uploding
                CHECK(m_internal_format==internal_format) << "Previously defined internal format is not the same as the one which will be used for the tensor upload";
            }
            if (m_format!=EGL_INVALID){ //if we already have a format, it should be compatible with the one we are using for uploding
                CHECK(m_format==format) << "Previously defined format is not the same as the one which will be used for the tensor upload";
            }
            if (m_type!=EGL_INVALID){ //if we already have a format, it should be compatible with the one we are using for uploding
                CHECK(m_type==type) << "Previously defined type is not the same as the one which will be used for the tensor upload";
            }


            //we might only need to resize so it's faster
            allocate_or_resize(internal_format, format, type, w, h);


        }else{
            //allocate storage for the texture and let it uninitialized
            allocate_storage(internal_format, format, type, w, h);


        }

        CHECK(m_cuda_resource!=nullptr) << "The cuda resource should be something different than null. Something went wrong";



        //bind the texture and map it to cuda https://developer.download.nvidia.com/GTC/PDF/GTC2012/PresentationPDF/S0267A-GTC2012-Mixing-Graphics-Compute.pdf
        bind();
        cudaGraphicsMapResources(1, &m_cuda_resource, 0);

        //get cuda ptr
        cudaArray *tex_data_ptr;
        cudaGraphicsSubResourceGetMappedArray(&tex_data_ptr, m_cuda_resource, 0, 0);

        //copy from tensor to tex_data_ptr http://leadsense.ilooktech.com/sdk/docs/page_samplewithopengl.html
        torch::Tensor tensor_hwc=tensor.permute({0,2,3,1}).squeeze().contiguous();
        cudaMemcpy2DToArray(tex_data_ptr,0,0, tensor_hwc.data_ptr(),  c*w *bytes_per_element(),  c*w *bytes_per_element(), h, cudaMemcpyDeviceToDevice );


        //unmap
        cudaGraphicsUnmapResources(1, &m_cuda_resource, 0);

        // generate_mipmap_full(); //cannot generate mip map for some reason, probably because the memory layour changes and therefore the cuda resource also changes

        unbind();


    }


    torch::Tensor Texture2D::to_tensor(){
        CHECK(m_cuda_transfer_enabled) << "You must enable first the cuda transfer with tex.enable_cuda_transfer(). This incurrs a performance cost for memory realocations so try to keep the texture in memory mostly unchanged.";
        CHECK(m_tex_storage_initialized) << named("Texture storage was not initialized. Cannot copy to tensor");
        CHECK(m_internal_format!=EGL_INVALID) << named("Internal format was not initialized");
        CHECK(m_format!=EGL_INVALID) << named("Format was not initialized");
        CHECK(m_type!=EGL_INVALID) << named("Type was not initialized");

        int nr_channels_tensor=0;
        torch::ScalarType scalar_type_tensor=torch::kFloat32;
        gl_internal_format2tensor_type(nr_channels_tensor, scalar_type_tensor, m_internal_format);

        torch::Tensor tensor = torch::zeros({ m_height, m_width, nr_channels_tensor }, torch::dtype(scalar_type_tensor).device(torch::kCUDA, 0) );

        int c=tensor.size(2);
        int h=tensor.size(0);
        int w=tensor.size(1);

        bind();
        cudaGraphicsMapResources(1, &m_cuda_resource, 0);

        //get cuda ptr
        cudaArray *tex_data_ptr;
        cudaGraphicsSubResourceGetMappedArray(&tex_data_ptr, m_cuda_resource, 0, 0);

        //copy from tensor to tex_data_ptr http://leadsense.ilooktech.com/sdk/docs/page_samplewithopengl.html
        // torch::Tensor tensor_hwc=tensor.permute({0,2,3,1}).squeeze().contiguous();
        // cudaMemcpy2DFromArray(tex_data_ptr,0,0, tensor_hwc.data_ptr(),  c*w *bytes_per_element(),  c*w *bytes_per_element(), h, cudaMemcpyDeviceToDevice );
        cudaMemcpy2DFromArray(tensor.data_ptr(),   c*w *bytes_per_element(), tex_data_ptr, 0,0, c*w *bytes_per_element(), h, cudaMemcpyDeviceToDevice );

        //unmap
        cudaGraphicsUnmapResources(1, &m_cuda_resource, 0);

        unbind();

        //from h,w,c to nchw
        tensor=tensor.unsqueeze(0).permute({0,3,1,2});

        return tensor;


    }
#endif

void Texture2D::copy_from_tex(Texture2D& other_tex, const int level){
    //following https://stackoverflow.com/a/23994979 seems that glCopyTexSubImage2D is one of the fastest ways to copy
    //more example on the usage of of glCopyTexSubImage2D https://stackoverflow.com/a/55294964

    //check this texture
    CHECK(m_tex_storage_initialized) << named("Texture storage was not initialized. Cannot copy to tensor");
    CHECK(m_internal_format!=EGL_INVALID) << named("Internal format was not initialized");
    CHECK(m_format!=EGL_INVALID) << named("Format was not initialized");
    CHECK(m_type!=EGL_INVALID) << named("Type was not initialized");
    //check the other texture
    CHECK(other_tex.m_tex_storage_initialized) << named("other_tex: Texture storage was not initialized. Cannot copy to tensor");
    CHECK(other_tex.m_internal_format!=EGL_INVALID) << named("other_tex: Internal format was not initialized");
    CHECK(other_tex.m_format!=EGL_INVALID) << named("other_tex: Format was not initialized");
    CHECK(other_tex.m_type!=EGL_INVALID) << named("other_tex: Type was not initialized");
    //check that things are equal between the two textures
    CHECK(other_tex.width()==this->width()) << named("Width is not the same between the two textures");
    CHECK(other_tex.height()==this->height()) << named("Height is not the same between the two textures");
    CHECK(other_tex.m_internal_format==this->m_internal_format) << named("m_internal_format is not the same between the two textures");
    CHECK(other_tex.m_format==this->m_format) << named("m_format is not the same between the two textures");
    CHECK(other_tex.m_type==this->m_type) << named("m_type is not the same between the two textures");



    glBindFramebuffer(GL_FRAMEBUFFER, other_tex.fbo_id(level));
    glReadBuffer(GL_COLOR_ATTACHMENT0);

    glActiveTexture(GL_TEXTURE0);
    this->bind();

    //copy
    glCopyTexSubImage2D(GL_TEXTURE_2D, level, 0, 0, 0, 0, this->width(), this->height());


}

void Texture2D::generate_mipmap(const int idx_max_lvl){
    if(idx_max_lvl!=0){
        // glActiveTexture(GL_TEXTURE0);
        bind();
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,  GL_TEXTURE_MAX_LEVEL, idx_max_lvl);
        glGenerateMipmap(GL_TEXTURE_2D);
        m_idx_mipmap_allocated=idx_max_lvl;
    }

    // // generate mip map allocates memory so therefore the framebuffer needs to be updated
    // glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo_for_clearing_id);
    // glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_tex_id, 0);
    // glDrawBuffer(GL_COLOR_ATTACHMENT0); //Only need to do this once.
    // glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    // clear();
}

//creates the full chain of mip map, up until the smallest possible texture
void Texture2D::generate_mipmap_full(){
    int idx_max_lvl=mipmap_highest_idx();
    generate_mipmap(idx_max_lvl);
}


void Texture2D::bind() const{
    glBindTexture(GL_TEXTURE_2D, m_tex_id);
}

void Texture2D::unbind() const{
    glBindTexture(GL_TEXTURE_2D, 0);
}

int Texture2D::tex_id() const{
    return m_tex_id;
}

bool Texture2D::storage_initialized () const{
    return m_tex_storage_initialized;
}

GLint Texture2D::internal_format() const{
    CHECK(m_internal_format!=EGL_INVALID) << named("The texture has not been initialzied and doesn't yet have a internal format");
    return m_internal_format;
}

GLenum Texture2D::format() const{
    CHECK(m_format!=EGL_INVALID) << named("The texture has not been initialzied and doesn't yet have a format");
    return m_format;
}

GLenum Texture2D::type() const{
    CHECK(m_type!=EGL_INVALID) << named("The texture has not been initialzied and doesn't yet have a type");
    return m_type;
}

GLuint Texture2D::fbo_id(const int mip){

    //check if the mip we are accesing is withing the range of mip maps we have allocated
    CHECK(mip<mipmap_nr_levels_allocated()) << "mipmap idx " << mip << " is smaller than the nr of mips we have allocated which is " << mipmap_nr_levels_allocated();

    //check if the fbo for this mip is created
    if(m_fbos_for_mips[mip]==EGL_INVALID){
        //the fbo is not created so we create it
        glGenFramebuffers(1, &m_fbos_for_mips[mip] );
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbos_for_mips[mip]);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_tex_id, mip);
        glDrawBuffer(GL_COLOR_ATTACHMENT0); //Only need to do this once.
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    }

    return m_fbos_for_mips[mip];
}


int Texture2D::width() const{ return m_width; }
int Texture2D::height() const{ return m_height; }
int Texture2D::width_for_lvl(const int lvl) const{ return std::max(1, (int)floor(m_width /  (int)pow(2,lvl)  )); }
int Texture2D::height_for_lvl(const int lvl) const{ return std::max(1,  (int)floor(m_height /  (int)pow(2,lvl)  ));  }
int Texture2D::channels() const{
    CHECK(m_format!=EGL_INVALID) << named("Format was not initialized");
    switch(m_format) {
        case GL_RED : return 1; break;
        case GL_RG : return 2; break;
        case GL_RGB : return 3; break;
        case GL_RGBA : return 4; break;
        default : LOG(FATAL) << "We don't know how many channels does this format have."; return 0; break;
    }
}
int Texture2D::bytes_per_element() const{
    CHECK(m_type!=EGL_INVALID) << named("Type was not initialized");
    switch(m_type) {
        case GL_UNSIGNED_BYTE : return 1; break;
        case GL_BYTE : return 1; break;
        case GL_UNSIGNED_SHORT : return 2; break;
        case GL_SHORT : return 2; break;
        case GL_UNSIGNED_INT : return 4; break;
        case GL_INT : return 4; break;
        case GL_HALF_FLOAT : return 2; break;
        case GL_FLOAT : return 4; break;
        default : LOG(FATAL) << "We don't know this type."; return 0; break;
    }
}

int Texture2D::num_bytes_texture(){
    CHECK(m_type!=EGL_INVALID) << named("Type was not initialized");
    CHECK(m_tex_storage_initialized) << named("Texture storage was not initialized. Cannot download to an opencv mat");
    return m_width*m_height*channels()*bytes_per_element();
}

//returns the index of the highest mip map lvl (this is used to plug into generate_mip_map)
int Texture2D::mipmap_highest_idx() const { return floor(log2(  std::max(m_width, m_height)  ));  }
//return maximum number of mip map lvls, effectivelly it is mipmap_highest_idx+1
int Texture2D::mipmap_nr_lvls() const{ return mipmap_highest_idx()+1; }
int Texture2D::mipmap_nr_levels_allocated() const{ return m_idx_mipmap_allocated+1;}




std::string Texture2D::named(const std::string msg) const{
    return m_name.empty()? msg : m_name + ": " + msg;
}


} //namespace gl
