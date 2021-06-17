#pragma once
#include <glad/glad.h>

#include <iostream>

//use the maximum value of an int as invalid . We don't use negative because we sometimes compare with unsigned int
#define EGL_INVALID 2147483647

namespace gl{
    class Texture3D{
    public:
        Texture3D():
            m_width(0),
            m_height(0),
            m_depth(0),
            m_tex_id(EGL_INVALID),
            m_tex_storage_initialized(false),
            m_internal_format(EGL_INVALID),
            m_nr_pbos(1),
            m_cur_pbo_idx(0){
            glGenTextures(1,&m_tex_id);

            //create some pbos
            m_pbo_ids.resize(m_nr_pbos,EGL_INVALID);
            m_pbo_storages_initialized.resize(m_nr_pbos,false);
            glGenBuffers(m_nr_pbos, m_pbo_ids.data());


            //start with some sensible parameter initialziations
            set_wrap_mode(GL_CLAMP_TO_EDGE);
            set_filter_mode_min_mag(GL_LINEAR);
        }

        Texture3D(std::string name):
            Texture3D(){
            m_name=name; //we delegate the constructor to the main one but we cannot have in this intializer list more than that call.
        }

        ~Texture3D(){
            // LOG(WARNING) << named("Destroying texture");
            glDeleteTextures(1, &m_tex_id);
            glDeleteBuffers(m_nr_pbos, m_pbo_ids.data());
        }

        //rule of five (make the class non copyable)
        Texture3D(const Texture3D& other) = delete; // copy ctor
        Texture3D& operator=(const Texture3D& other) = delete; // assignment op
        // Use default move ctors.  You have to declare these, otherwise the class will not have automatically generated move ctors.
        Texture3D (Texture3D && other) = default; //move ctor
        Texture3D & operator=(Texture3D &&) = default; //move assignment

        void set_name(const std::string name){
            m_name=name;
        }

        std::string name() const{
            return m_name;
        }


        void set_wrap_mode(const GLenum wrap_mode){
            glBindTexture(GL_TEXTURE_3D, m_tex_id);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, wrap_mode);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, wrap_mode);
        }

        void set_filter_mode_min_mag(const GLenum filter_mode){
            glBindTexture(GL_TEXTURE_3D, m_tex_id);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, filter_mode);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, filter_mode);
        }

        void upload_data(GLint internal_format, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* data_ptr, int size_bytes){

            m_width=width;
            m_height=height;
            m_depth=depth;
            m_internal_format=internal_format;


            // bind the texture and PBO
            glBindTexture(GL_TEXTURE_3D, m_tex_id);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER,  m_pbo_ids[m_cur_pbo_idx]);

            if(!m_pbo_storages_initialized[m_cur_pbo_idx]){
                glBufferData(GL_PIXEL_UNPACK_BUFFER, size_bytes, NULL, GL_STREAM_DRAW); //allocate storage for pbo
                m_pbo_storages_initialized[m_cur_pbo_idx]=true;
            }
            if(!m_tex_storage_initialized){
                glTexImage3D(GL_TEXTURE_3D, 0, internal_format,width,height,depth,0,format,type,0); //allocate storage texture
                m_tex_storage_initialized=true;
            }

            //attempt 2 to update the pbo fast, mapping and doing a memcpy is slower
            glBufferSubData(GL_PIXEL_UNPACK_BUFFER, 0, size_bytes, data_ptr);

            // copy pixels from PBO to texture object (this returns inmediatelly and lets the GPU perform DMA at a later time)
            glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, width, height, depth, format, type, 0);

            // it is good idea to release PBOs with ID 0 after use.
            // Once bound with 0, all pixel operations behave normal ways.
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

            m_cur_pbo_idx=(m_cur_pbo_idx+1)%m_nr_pbos;
        }

        void set_sparse(GLint val){
            glBindTexture(GL_TEXTURE_3D, m_tex_id);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_SPARSE_ARB, val);
        }

        //allocate inmutable texture storage
        void allocate_tex_storage_inmutable(GLenum internal_format,  GLsizei width, GLsizei height, GLsizei depth){
            m_width=width;
            m_height=height;
            m_depth=depth;
            m_internal_format=internal_format;
            glTexStorage3D(GL_TEXTURE_3D, 1, internal_format, width, height, depth);
            m_tex_storage_initialized=true;
        }

        // template <class T>
        // void clear(T val){
        //     CHECK(m_tex_storage_initialized) << named("Texture storage not initialized. Use allocate_tex_storage_inmutable or upload data first");

        //     GLenum format;
        //     GLenum type;
        //     gl_internal_format2format_and_type(format,type, m_internal_format, false);

        //     std::vector<T> clear_color(4, val);
        //     glClearTexSubImage( m_tex_id , 0, 0,0,0,  m_width, m_height, m_depth, format, type, clear_color.data());
        // }

        void upload_without_pbo(GLint level, GLint xoffset, GLint yoffset,GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* data_ptr){
            glBindTexture(GL_TEXTURE_3D, m_tex_id);
            glTexSubImage3D(GL_TEXTURE_3D,level,xoffset,yoffset,zoffset,width,height,depth,format,type,data_ptr);
        }



        //uploads the data of the pbo (on the gpu already) to the texture (also on the gpu). Should return inmediatelly
        void upload_pbo_to_tex_no_binds( GLint xoffset, GLint yoffset, GLint zoffset,
                                GLsizei width, GLsizei height, GLsizei depth,
                                GLenum format, GLenum type){

            // copy pixels from PBO to texture object (this returns inmediatelly and lets the GPU perform DMA at a later time)
            glTexSubImage3D(GL_TEXTURE_3D, 0,
                    xoffset, yoffset, zoffset,
                    width, height, depth,
                    format, type,
                    0);

        }

        void bind() const{
            glBindTexture(GL_TEXTURE_3D, m_tex_id);
        }

        int tex_id() const{
            return m_tex_id;
        }

        bool storage_initialized () const{
            return m_tex_storage_initialized;
        }

        GLint internal_format() const{
            CHECK(m_internal_format!=EGL_INVALID) << named("The texture has not been initialzied and doesn't yet have a format");
            return m_internal_format;
        }

        int width() const{ LOG_IF(WARNING,m_width==0) << named("Width of the texture is 0"); return m_width; };
        int height() const{ LOG_IF(WARNING,m_height==0) << named("Height of the texture is 0");return m_height; };
        int depth() const{ LOG_IF(WARNING,m_depth==0) << named("Depth of the texture is 0");return m_depth; };


    private:
        int m_width;
        int m_height;
        int m_depth;

        std::string named(const std::string msg) const{
            return m_name.empty()? msg : m_name + ": " + msg;
        }
        std::string m_name;

        GLuint m_tex_id;
        // GLuint m_pbo_id;
        bool m_tex_storage_initialized;
        // bool m_pbo_storage_initialized;
        GLint m_internal_format;

        int m_nr_pbos;
        std::vector<GLuint> m_pbo_ids;
        std::vector<bool> m_pbo_storages_initialized;
        int m_cur_pbo_idx; //index into the pbo that we will use for uploading




    };
}
