#pragma once
#include <glad/glad.h>

#include <easy_gl/Texture2D.h>


#define MAX_TEXTURES 8


//use the maximum value of an int as invalid . We don't use negative because we sometimes compare with unsigned int
#define EGL_INVALID 2147483647

namespace gl{
    class GBuffer{
    public:
        GBuffer();

        GBuffer(std::string name);

        ~GBuffer();

        // rule of five (make the class non copyable)
        GBuffer(const GBuffer& other) = delete; // copy ctor
        GBuffer& operator=(const GBuffer& other) = delete; // assignment op
        // Use default move ctors.  You have to declare these, otherwise the class will not have automatically generated move ctors.
        GBuffer (GBuffer && other) = default; //move ctor
        GBuffer & operator=(GBuffer &&) = default; //move assignment


        void add_texture(const std::string name, GLint internal_format, GLenum format, GLenum type);
        void add_depth(const std::string name);
        void make_empty(const int width, const int height);
        void sanity_check();
        void set_size(const int w, const int h);
        void bind() const;
        void bind_for_draw() const;
        void bind_for_read() const;
        void clear();
        void clear_depth();
        void set_constant(float val);


        int get_fbo_id() const;

        int width() const;
        int height() const;

        bool is_initialized();
        int attachment_nr(const std::string tex_name) const;
        Texture2D& tex_with_name(const std::string name);

        bool has_tex_with_name(const std::string name);

    private:
        int m_width;
        int m_height;

        std::string named(const std::string msg) const;
        std::string m_name;

        GLuint m_fbo_id;
        std::vector<gl::Texture2D> m_textures; //we don't use a vector beceuase as its size increases it might need to copy elements around and I dont want to implement a move constructor in the Texture. Also the copy constructor is deleted
        gl::Texture2D m_depth_tex;
        bool m_has_depth_tex;

        std::unordered_map<std::string, int > m_texname2attachment; //maps from each texture name to the corresponding GL_COLOR_ATTACHMENT in the framebuffer. Useful when binding the framebuffer to be written to

        int m_max_color_attachments;

    };
}
