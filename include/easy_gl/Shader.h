#pragma once

#include <glad/glad.h>

#include <iostream>
#include <fstream>
#include <unordered_map>

// #include <easy_gl/UtilsGL.h>
#include "easy_gl/Texture2D.h"
// #include "Texture2DArray.h"
// #include "Texture3D.h"
#include "easy_gl/Buf.h"
#include "easy_gl/GBuffer.h"
#include "easy_gl/CubeMap.h"

#include <iostream>

#include <Eigen/Core>

//use the maximum value of an int as invalid . We don't use negative because we sometimes compare with unsigned int
#define EGL_INVALID 2147483647

namespace gl{
    class Shader{
    public:
        Shader();
        Shader(const std::string name);
        ~Shader();

        //rule of five (make the class non copyable)
        Shader(const Shader& other) = delete; // copy ctor
        Shader& operator=(const Shader& other) = delete; // assignment op
        // Use default move ctors.  You have to declare these, otherwise the class will not have automatically generated move ctors.
        Shader (Shader && other) = default; //move ctor
        Shader & operator=(Shader &&) = default; //move assignment


        //compiles a program from various shaders
        void compile(const std::string &compute_shader_filename);
        void compile(const std::string &vertex_shader_filename, const std::string &fragment_shader_filename);
        void compile(const std::string &vertex_shader_filename, const std::string &fragment_shader_filename, const std::string &geom_shader_filename);


        void use() const;

        template <class T>
        void bind_texture(const T& tex, const std::string& uniform_name);
        //bind with a certain access mode a 2D image
        void bind_image(const gl::Texture2D& tex, const GLenum access, const std::string& uniform_name);
        //bind all layers of the Texture Array
        // void bind_image(const gl::Texture2DArray& tex,  const GLenum access, const std::string& uniform_name);
        // //binding a Texture array but binds a specific layer
        // void bind_image(const gl::Texture2DArray& tex, const GLint layer, const GLenum access, const std::string& uniform_name);
        //bind all layers of Texture 3D
        // void bind_image(const gl::Texture3D& tex,  const GLenum access, const std::string& uniform_name);
        //bind a buffer
        void bind_buffer(const gl::Buf& buf, const std::string& uniform_name);

        GLint get_attrib_location(const std::string attrib_name) const;

        void uniform_bool(const bool val, const std::string uniform_name);
        void uniform_int(const int val, const std::string uniform_name);
        void uniform_float(const float val, const std::string uniform_name);
        void uniform_v2_float(const Eigen::Vector2f vec, const std::string uniform_name);
        void uniform_v3_float(const Eigen::Vector3f vec, const std::string uniform_name);
        void uniform_v4_float(const Eigen::Vector4f vec, const std::string uniform_name);
        //sends an array of vec3 to the shader. Inside the shader we declare it as vec3 array[SIZE] where size must correspond to the one being sent
        void uniform_array_v3_float(const Eigen::MatrixXf mat, const std::string uniform_name);
        //sends an array of vec2 to the shader. Inside the shader we declare it as vec2 array[SIZE] where size must correspond to the one being sent
        void uniform_array_v2_float(const Eigen::MatrixXf mat, const std::string uniform_name);
        void uniform_3x3(const Eigen::Matrix3f mat, const std::string uniform_name);
        void uniform_4x4(const Eigen::Matrix4f mat, const std::string uniform_name);
        void dispatch(const int total_x, const int total_y, const int local_size_x, const int local_size_y);

        //output2tex_list is a list of pair which map from the output of a shader to the corresponding name of the texture that we want to write into.
        // void draw_into(const GBuffer& gbuffer, std::initializer_list<  std::pair<std::string, std::string> > output2tex_list){
        void draw_into(const GBuffer& gbuffer, std::vector<  std::pair<std::string, std::string> > output2tex_list);
        //draw into just one texture, which is not assigned to a gbuffer
        void draw_into(Texture2D& tex, const std::string frag_out_name, const int mip=0);
        //draw into one of the faces of a cubemap
        void draw_into(CubeMap& tex, const std::string frag_out_name, const int cube_face_idx, const int mip=0);


        int get_prog_id() const;

        GLint get_uniform_location(std::string uniform_name);
        bool is_compiled();


    private:
        std::string m_name;
        GLuint m_prog_id;
        bool m_is_compiled;
        bool m_is_compute_shader;
        int m_nr_texture_units_used;
        int m_nr_image_units_used;
        int m_max_allowed_texture_units;
        int m_max_allowed_image_units;

        std::unordered_map<std::string, int > tex_sampler2texture_units;
        std::unordered_map<std::string, int > image2image_units;

        std::string named(const std::string msg) const;


        //for compute shaders
        GLuint program_init( const std::string &compute_shader_string);
        //for program which have vertex and fragment shaders
        GLuint program_init( const std::string &vertex_shader_string, const std::string &fragment_shader_string);
        //for program which have vertex, fragment and geometry shaders
        GLuint program_init( const std::string &vertex_shader_string, const std::string &fragment_shader_string, const std::string &geom_shader_string);
        void link_program_and_check(const GLuint& program_shader);
        GLuint load_shader( const std::string & src,const GLenum type);
        inline std::string file_to_string (const std::string &filename);

    };

}
