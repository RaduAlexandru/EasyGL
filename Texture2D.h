#pragma once
#include <GL/glad.h>

#include <iostream>

#include "opencv2/opencv.hpp"

#include "UtilsGL.h"
#include "stereo_cost_vol_dense/MiscUtils.h"

namespace gl{
    class Texture2D{
    public:
        Texture2D():
            m_tex_id(-1),
            m_pbo_id(-1),
            m_internal_format(-1),
            m_tex_storage_initialized(false),
            m_pbo_storage_initialized(false){
            glGenTextures(1,&m_tex_id);
            glGenBuffers(1, &m_pbo_id);

            //start with some sensible parameter initialziations
            set_wrap_mode(GL_CLAMP_TO_BORDER);
            set_filter_mode(GL_LINEAR);
        }

        ~Texture2D(){
            glDeleteTextures(1, &m_tex_id);
            glDeleteBuffers(1, &m_pbo_id);
        }

        void set_wrap_mode(const GLenum wrap_mode){
            glBindTexture(GL_TEXTURE_2D, m_tex_id);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_mode);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_mode);
        }

        void set_filter_mode(const GLenum filter_mode){
            glBindTexture(GL_TEXTURE_2D, m_tex_id);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter_mode);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter_mode);
        }
        void upload_data(GLint internal_format, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* data_ptr, int size_bytes){
            m_internal_format=internal_format;
            // bind the texture and PBO
            glBindTexture(GL_TEXTURE_2D, m_tex_id);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pbo_id);

            if(!m_pbo_storage_initialized){
                glBufferData(GL_PIXEL_UNPACK_BUFFER, size_bytes, NULL, GL_STREAM_DRAW); //allocate storage for pbo
                m_pbo_storage_initialized=true;
            }
            if(!m_tex_storage_initialized){
                glTexImage2D(GL_TEXTURE_2D, 0, internal_format,width,height,0,format,type,0); //allocate storage texture
                m_tex_storage_initialized=true;
            }


            //update pbo
            glBufferData(GL_PIXEL_UNPACK_BUFFER, size_bytes, 0, GL_STREAM_DRAW); //orphan the pbo storage
            GLubyte* ptr = (GLubyte*)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
            if(ptr){
                memcpy(ptr, data_ptr, size_bytes);
                glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER); // release pointer to mapping buffer
            }

            // copy pixels from PBO to texture object (this returns inmediatelly and lets the GPU perform DMA at a later time)
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, format, type, 0);

            // it is good idea to release PBOs with ID 0 after use.
            // Once bound with 0, all pixel operations behave normal ways.
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        }


        //easy way to just get a cv mat there, does internally an upload to pbo and schedules a dma
        //by default the values will get transfered to the gpu and get normalized to [0,1] therefore an rgb texture of unsigned bytes will be read as floats from the shader with sampler2D. However sometimes we might want to use directly the integers stored there, for example when we have a semantic texture and the nr range from [0,nr_classes]. Then we set normalize to false and in the shader we acces the texture with usampler2D
        void upload_from_cv_mat(const cv::Mat& cv_mat, const bool store_as_normalized_vals=true){

            //from the cv format get the corresponding gl internal_format, format and type
            GLint internal_format;
            GLenum format;
            GLenum type;
            cv_type2gl_formats(internal_format, format, type ,cv_mat.type(), store_as_normalized_vals);
            std::cout << "upload from cv_mat internal format is " << std::hex << internal_format << std::dec << '\n';

            //do the upload to the pbo
            int size_bytes=cv_mat.step[0] * cv_mat.rows;
            upload_data(internal_format, cv_mat.cols, cv_mat.rows, format, type, cv_mat.ptr(), size_bytes);
        }


        void upload_without_pbo(GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* data_ptr){
            glBindTexture(GL_TEXTURE_2D, m_tex_id);
            glTexSubImage2D(GL_TEXTURE_2D,level,xoffset,yoffset,width,height,format,type,data_ptr);
        }


        //allocate inmutable texture storage
        void allocate_tex_storage_inmutable(GLenum internal_format,  GLsizei width, GLsizei height){
            m_internal_format=internal_format;
            glBindTexture(GL_TEXTURE_2D, m_tex_id);
            glTexStorage2D(GL_TEXTURE_2D, 1, internal_format, width, height);
            m_tex_storage_initialized=true;
        }

        //uploads data from cpu to pbo (on gpu) will take some cpu time to perform the memcpy
        void upload_to_pbo(const void* data_ptr, int size_bytes){
            // bind the PBO
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pbo_id);


            //allocate storage for pbo
            if(!m_pbo_storage_initialized){
                glBufferData(GL_PIXEL_UNPACK_BUFFER, size_bytes, NULL, GL_STREAM_DRAW); //allocate storage for pbo
                m_pbo_storage_initialized=true;
            }


            //update pbo
            glBufferData(GL_PIXEL_UNPACK_BUFFER, size_bytes, 0, GL_STREAM_DRAW); //orphan the pbo storage
            GLubyte* ptr = (GLubyte*)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
            if(ptr){
                memcpy(ptr, data_ptr, size_bytes);
                glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER); // release pointer to mapping buffer
            }

            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        }

        //uploads the data of the pbo (on the gpu already) to the texture (also on the gpu). Should return inmediatelly
        void upload_pbo_to_tex( GLsizei width, GLsizei height,
                                GLenum format, GLenum type){
            // bind the PBO and texture
            glBindTexture(GL_TEXTURE_2D, m_tex_id);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pbo_id);

            // copy pixels from PBO to texture object (this returns inmediatelly and lets the GPU perform DMA at a later time)
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, format, type, 0);


            // it is good idea to release PBOs with ID 0 after use.
            // Once bound with 0, all pixel operations behave normal ways.
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        }

        //uploads the data of the pbo (on the gpu already) to the texture (also on the gpu). Should return inmediatelly
        void upload_pbo_to_tex_no_binds( GLsizei width, GLsizei height,
                                         GLenum format, GLenum type){

            // copy pixels from PBO to texture object (this returns inmediatelly and lets the GPU perform DMA at a later time)
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, format, type, 0);

        }


        //opengl stores it as floats which are in range [0,1]. By default we return them as such, othewise we denormalize them to the range [0,255]
        cv::Mat download_to_cv_mat(const bool denormalize=false){
            bind();
            //get the width and height of the gl_texture
            int w, h;
            int miplevel = 0;
            glGetTexLevelParameteriv(GL_TEXTURE_2D, miplevel, GL_TEXTURE_WIDTH, &w);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, miplevel, GL_TEXTURE_HEIGHT, &h);

            //create the cv_mat and
            std::cout << "m_internal_format is " << m_internal_format << '\n';
            int cv_type=gl_internal_format2cv_type(m_internal_format);
            GLenum format;
            GLenum type;
            gl_internal_format2format_and_type(format,type, m_internal_format, denormalize);
            cv::Mat cv_mat(h, w, cv_type);
            glGetTexImage(GL_TEXTURE_2D,0, format, type, cv_mat.data);
            cv_mat*=255; //go from range [0,1] to [0,255];

            return cv_mat;
        }


        void bind() const{
            glBindTexture(GL_TEXTURE_2D, m_tex_id);
        }

        int get_tex_id() const{
            return m_tex_id;
        }

        bool get_tex_storage_initialized (){
            return m_tex_storage_initialized;
        }

        GLint get_internal_format() const{
            CHECK(m_internal_format!=-1) << "The texture has not been initialzied and doesn't yet have a format";
            return m_internal_format;
        }


    private:
        GLuint m_tex_id;
        GLuint m_pbo_id;
        bool m_tex_storage_initialized;
        bool m_pbo_storage_initialized;
        GLint m_internal_format;


    };
}
