#pragma once
#include <glad/glad.h>

#include <iostream>

#include "opencv2/opencv.hpp"

// #include "easy_gl/UtilsGL.h"

//use the maximum value of an int as invalid . We don't use negative because we sometimes compare with unsigned int
#define EGL_INVALID 2147483647

namespace gl{
    class CubeMap{
    public:
        CubeMap();
        CubeMap(std::string name);
        ~CubeMap();

        //rule of five (make the class non copyable)
        CubeMap(const CubeMap& other) = delete; // copy ctor
        CubeMap& operator=(const CubeMap& other) = delete; // assignment op
        // Use default move ctors.  You have to declare these, otherwise the class will not have automatically generated move ctors.
        CubeMap (CubeMap && other) = default; //move ctor
        CubeMap & operator=(CubeMap &&) = default; //move assignment

        void set_name(const std::string name);

        std::string name() const;

        void set_wrap_mode(const GLenum wrap_mode);
        void set_filter_mode_min_mag(const GLenum filter_mode);
        void set_filter_mode_min(const GLenum filter_mode);
        void set_filter_mode_mag(const GLenum filter_mode);


        void resize(const int w, const int h);


        //allocate mutable texture storage and leave it uninitialized
        void allocate_tex_storage(GLenum internal_format, GLenum format, GLenum type, GLsizei width, GLsizei height);
        //allocate inmutable texture storage
        void allocate_tex_storage_inmutable(GLenum internal_format, GLenum format, GLenum type, GLsizei width, GLsizei height);
        void allocate_or_resize(GLenum internal_format, GLenum format, GLenum type, GLsizei width, GLsizei height);

        //clears the texture to zero
        void clear();

        void set_constant(float val);

        void set_constant(float val, float val_alpha);

        void generate_mipmap(const int idx_max_lvl);
        //creates the full chain of mip map, up until the smallest possible texture
        void generate_mipmap_full();


        void bind() const;

        int tex_id() const;

        bool storage_initialized () const;

        GLint internal_format() const;

        GLuint fbo_id(const int mip=0);


        int width() const;
        int height() const;
        int width_for_lvl(const int lvl) const;
        int height_for_lvl(const int lvl) const;
        int channels() const;

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
        // GLuint m_pbo_id;
        bool m_tex_storage_initialized;
        bool m_tex_storage_inmutable;
        // bool m_pbo_storage_initialized;
        GLint m_internal_format;
        GLenum m_format;
        GLenum m_type;
        int m_idx_mipmap_allocated; //the index of the maximum mip_map level allocated. It starts at 0 for the case when we have only the base level texture

        std::vector<GLuint> m_fbos_for_mips; //each fbo point to a mip map of this texture
        // GLuint m_fbo_for_clearing_id; //for clearing we attach the texture to a fbo and clear that. It's a lot faster than glcleartexImage

    };
}
