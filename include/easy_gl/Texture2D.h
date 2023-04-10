#pragma once
#include <glad/glad.h>


// DO NOT USE A IFDEF because other C++ libs may include this Viewer.h without the compile definitions and therefore the Viewer.h that was used to compile easypbr and the one included will be different leading to issues
// #ifdef EASYPBR_WITH_TORCH
    // #include "torch/torch.h"
    // #include <cuda_gl_interop.h>
// #endif

#include <iostream>

#include "opencv2/opencv.hpp"

// #include "easy_gl/UtilsGL.h"
#include "easy_gl/Buf.h"

//forward declare
struct cudaGraphicsResource;
// namespace torch{
    // class Tensor;
// };
// class Tensor;
namespace at{
    class Tensor;
};




//use the maximum value of an int as invalid . We don't use negative because we sometimes compare with unsigned int
#define EGL_INVALID 2147483647

namespace gl{
    class Texture2D{
    public:
        Texture2D();
        Texture2D(std::string name);
        ~Texture2D();

        //rule of five (make the class non copyable)
        Texture2D(const Texture2D& other) = delete; // copy ctor
        Texture2D& operator=(const Texture2D& other) = delete; // assignment op
        // Use default move ctors.  You have to declare these, otherwise the class will not have automatically generated move ctors.
        Texture2D (Texture2D && other) = default; //move ctor
        Texture2D & operator=(Texture2D &&) = default; //move assignment



        void set_name(const std::string name);
        std::string name() const;
        void set_wrap_mode(const GLenum wrap_mode);
        void set_filter_mode_min_mag(const GLenum filter_mode);
        void set_filter_mode_min(const GLenum filter_mode);
        void set_filter_mode_mag(const GLenum filter_mode);
        // #ifdef EASYPBR_WITH_TORCH
        void enable_cuda_transfer();
        void disable_cuda_transfer();
        void register_for_cuda();
        void unregister_cuda();
        // #endif


        void upload_data(GLint internal_format, GLenum format, GLenum type, GLsizei width, GLsizei height,  const void* data_ptr, int size_bytes);


        //easy way to just get a cv mat there, does internally an upload to pbo and schedules a dma
        //by default the values will get transfered to the gpu and get normalized to [0,1] therefore an rgb texture of unsigned bytes will be read as floats from the shader with sampler2D. However sometimes we might want to use directly the integers stored there, for example when we have a semantic texture and the nr range from [0,nr_classes]. Then we set normalize to false and in the shader we acces the texture with usampler2D
        void upload_from_cv_mat(const cv::Mat& cv_mat, const bool flip_red_blue=true, const bool store_as_normalized_vals=true);


        // void upload_without_pbo(GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* data_ptr){
            // TODO this needs to be redone because I dont like that it takes the xoffset and y offset and so on. We should have an overloaded function that just is upload_without_pbo(data_ptr) which assumes that the texture already has a storage and it can do TexSubimage and another more complete that take also the internal_format, format and type and does teximage2d
        //     CHECK(m_tex_storage_initialized) << named("Texture storage was not initialized. Uploading without pbo assumes we have already a storage allocated");

        //     m_internal_format=internal_format;
        //     m_format=format;

        //     glBindTexture(GL_TEXTURE_2D, m_tex_id);
        //     glTexSubImage2D(GL_TEXTURE_2D,level,xoffset,yoffset,width,height,format,type,data_ptr);
        // }

        void resize(const int w, const int h);


        //allocate mutable texture storage and leave it uninitialized
        void allocate_storage(GLenum internal_format, GLenum format, GLenum type, GLsizei width, GLsizei height);


        //allocate inmutable texture storage
        void allocate_storage_inmutable(GLenum internal_format, GLenum format, GLenum type, GLsizei width, GLsizei height);

        void allocate_or_resize(GLenum internal_format, GLenum format, GLenum type, GLsizei width, GLsizei height);

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
        void download_to_pbo();

        void download_from_oldest_pbo(void* data_out);

        Buf& cur_pbo_download();



        //clears the texture to zero
        void clear();

        void set_constant(float val);

        void set_constant(float val, float val_alpha);

        void set_val(const float r, const float g, const float b, const float alpha);


        //opengl stores it as floats which are in range [0,1]. By default we return them as such, othewise we denormalize them to the range [0,255]
        cv::Mat download_to_cv_mat(const int lvl=0, const bool denormalize=false);

        // #ifdef EASYPBR_WITH_TORCH
        // void from_tensor(const torch::Tensor& tensor, const bool flip_red_blue=false, const bool store_as_normalized_vals=true);
        void from_tensor(const at::Tensor& tensor, const bool flip_red_blue=false, const bool store_as_normalized_vals=true);
        at::Tensor to_tensor();
        // #endif

        void copy_from_tex(Texture2D& other_tex, const int level=0); //following https://stackoverflow.com/a/23994979 seems that glCopyTexSubImage2D is one of the fastest ways to copy

        void generate_mipmap(const int idx_max_lvl);

        //creates the full chain of mip map, up until the smallest possible texture
        void generate_mipmap_full();


        void bind() const;

        void unbind() const;

        int tex_id() const;

        bool storage_initialized () const;

        GLint internal_format() const;

        GLenum format() const;

        GLenum type() const;

        GLuint fbo_id(const int mip=0);


        int width() const;
        int height() const;
        int width_for_lvl(const int lvl) const;
        int height_for_lvl(const int lvl) const;
        int channels() const;
        int bytes_per_element() const;

        int num_bytes_texture();

        //returns the index of the highest mip map lvl (this is used to plug into generate_mip_map)
        int mipmap_highest_idx() const;
        //return maximum number of mip map lvls, effectivelly it is mipmap_highest_idx+1
        int mipmap_nr_lvls() const;
        int mipmap_nr_levels_allocated() const;


    private:
        int m_width;
        int m_height;

        std::string named(const std::string msg) const;
        std::string m_name;

        GLuint m_tex_id;
        bool m_tex_storage_initialized;
        bool m_tex_storage_inmutable;
        GLint m_internal_format;
        GLenum m_format;
        GLenum m_type;
        int m_idx_mipmap_allocated; //the index of the maximum mip_map level allocated. It starts at 0 for the case when we have only the base level texture

        //pbos for uploading data into the texture
        int m_nr_pbos_upload;
        int m_cur_pbo_upload_idx; //index into the pbo that we will use for uploading
        std::vector<gl::Buf> m_pbos_upload;

        //pbos used for downloading the texture
        int m_nr_pbos_download;
        int m_cur_pbo_download_idx; //index into the pbo that we will use for downloading to cpu. It point at the pbo we will use for writing next time we call download_to_pbo. Also it is the one we use for reading when calling download_from_oldest_pbo() because this is the oldest one and the current one that will get overwritten if we were to write into it
        std::vector<gl::Buf> m_pbos_download;

        std::vector<GLuint> m_fbos_for_mips; //each fbo point to a mip map of this texture
        // GLuint m_fbo_for_clearing_id; //for clearing we attach the texture to a fbo and clear that. It's a lot faster than glcleartexImage


        //if we allocate a new storage for the texture, we need to update the cuda_resource
        bool m_cuda_transfer_enabled;
        struct cudaGraphicsResource *m_cuda_resource=nullptr;

    };
}
