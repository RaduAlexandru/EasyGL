#include <GL/glad.h>

#include <iostream>
#include <fstream>
#include <unordered_map>

#pragma once
#include <GL/glad.h>
#include <UtilsGL.h>

#include <iostream>

namespace gl{
    class Shader{
    public:
        Shader():
            m_prog_id(-1),
            m_is_compiled(false),
            m_nr_texture_units_used(0),
            m_nr_image_units_used(0),
            m_is_compute_shader(false)
            {
                //when we bind a texture we use up a texture unit. We check that we don't go above this value
                glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &m_max_allowed_texture_units);

                //when we bind a image we use up a image unit. We check that we don't go above this value
                glGetIntegerv(GL_MAX_IMAGE_UNITS, &m_max_allowed_image_units);
        }
        ~Shader(){
            glUseProgram(0);
            glDeleteProgram(m_prog_id);
        }

        //compiles a program from various shaders
        void compile(const std::string &compute_shader_filename){
            std::cout << "compiling compute shader " << '\n';
            m_prog_id = program_init(file_to_string(compute_shader_filename));
            m_is_compiled=true;
            m_is_compute_shader=true;
        }
        void compile(const std::string &vertex_shader_filename,
                     const std::string &fragment_shader_filename){
            m_prog_id = program_init(file_to_string(vertex_shader_filename),
                                     file_to_string(fragment_shader_filename)  );
            m_is_compiled=true;
        }
        void compile(const std::string &vertex_shader_filename,
                     const std::string &fragment_shader_filename,
                     const std::string &geom_shader_filename){
            m_prog_id = program_init(file_to_string(vertex_shader_filename),
                                     file_to_string(fragment_shader_filename),
                                     file_to_string(geom_shader_filename));
            m_is_compiled=true;
        }


        void use() const{
            CHECK(m_is_compiled) << "Program is not compiled! Use prog.compile() first";
            std::cout << "using program with id " << m_prog_id << '\n';
            glUseProgram(m_prog_id);
        }

        template <class T>
        void bind_texture(const T& tex, const std::string& uniform_name){
            CHECK(m_is_compiled) << "Program is not compiled! Use prog.compile() first";

            //get the location in shader for the sampler
            GLint shader_location=glGetUniformLocation(m_prog_id,uniform_name.c_str());
            LOG_IF(WARNING,shader_location==-1) << "Uniform location for name " << uniform_name << " is invalid. Are you sure you are using the uniform in the shader? Maybe you are also binding too many stuff";

            //each texture must be bound to a texture unit. We store the mapping from the textures to their corresponding texture unit in a map for later usage
            int cur_texture_unit;
            if(tex_sampler2texture_units.find (uniform_name) == tex_sampler2texture_units.end()){
                //the sampler was never used before so we bind the texture to a certain texture unit and we remember the one we bound it to
                cur_texture_unit=m_nr_texture_units_used;
                tex_sampler2texture_units[uniform_name]=cur_texture_unit;
                m_nr_texture_units_used++;
            }else{
                //the sampler was used before and the texture was already assigned to a certain texture unit, We bind the texture to the same one
                cur_texture_unit=tex_sampler2texture_units[uniform_name];
            }
            CHECK(m_nr_texture_units_used<m_max_allowed_texture_units) << "We used too many texture units! Try to bind less textures to the shader";

            glActiveTexture(GL_TEXTURE0 + cur_texture_unit);
            tex.bind(); //bind the texure to a certain texture unit
            glUniform1i(shader_location, cur_texture_unit); //the sampler will sample from that texture unit
        }

        //bind with a certain access mode a 2D image
        void bind_image(const gl::Texture2D& tex, const GLenum access, const std::string& uniform_name){

            check_format_is_valid_for_image_bind(tex);


            int cur_image_unit;
            if(image2image_units.find (uniform_name) == image2image_units.end()){
                //the image was never used before so we bind it
                cur_image_unit=m_nr_image_units_used;
                image2image_units[uniform_name]=cur_image_unit;
                m_nr_image_units_used++;
            }else{
                //the image was used before and the texture was already assigned to a certain image unit, We bind the image to the same one
                cur_image_unit=image2image_units[uniform_name];
            }
            uniform_int(cur_image_unit, uniform_name); //we cna either use binding=x in the shader or we can set it programatically like this
            CHECK(m_nr_image_units_used<m_max_allowed_image_units) << "We used too many image units! Try to bind less images to the shader";



            GL_C(glBindImageTexture(cur_image_unit, tex.get_tex_id(), 0, GL_FALSE, 0, access, tex.get_internal_format()));
        }

        // //bind all layers of the Texture Array
        // void bind_image(const gl::Texture2DArray& tex,  const GLenum access, const std::string& uniform_name){
        //     TODO get the internal of texture object from itself
        //     TODO get the image unit
        //     glBindImageTexture(1, tex.get_tex_id(), 0, GL_TRUE, 0, access, GL_R8UI);
        //     glBindImageTexture(2, tex.get_tex_id(), 0, GL_FALSE, 0, GL_WRITE_ONLY, tex.get_format());
        // }
        // //binding a Texture array but binds a specific layer
        // void bind_image(const gl::Texture2DArray& tex, const GLint layer, const GLenum access, const std::string& uniform_name){
        //     TODO get the type of texture from itself
        //     TODO get the image unit
        //     glBindImageTexture(1, tex.get_tex_id(), 0, GL_FALSE, layer, access, Format);
        // }

        void uniform_int(const int val, const std::string uniform_name){
            GLint uniform_location=get_uniform_location(uniform_name);
            std::cout << "uniform int locationn is " << uniform_location << " val is " << val << '\n';
            glUniform1i(uniform_location, val);
        }

        void uniform_float(const float val, const std::string uniform_name){
            GLint uniform_location=get_uniform_location(uniform_name);
            glUniform1f(uniform_location, 1);
        }

        void uniform_3x3(const GLfloat* val, const std::string uniform_name){
            GLint uniform_location=get_uniform_location(uniform_name);
            glUniformMatrix3fv(uniform_location, 1, GL_FALSE, val);
        }

        void uniform_4x4(const GLfloat* val, const std::string uniform_name){
            GLint uniform_location=get_uniform_location(uniform_name);
            glUniformMatrix4fv(uniform_location, 1, GL_FALSE, val);
        }

        void dispatch(const int total_x, const int total_y, const int local_size_x, const int local_size_y){
            CHECK(m_is_compute_shader) << "Program is not a compute shader so we cannot dispatch it";
            glDispatchCompute(round_up_to_nearest_multiple(total_x,local_size_x)/local_size_x,
                              round_up_to_nearest_multiple(total_y,local_size_y)/local_size_y, 1 );
            glMemoryBarrier(GL_ALL_BARRIER_BITS);
        }

        int get_prog_id() const{
            return m_prog_id;
        }

    private:
        GLuint m_prog_id;
        bool m_is_compiled;
        bool m_is_compute_shader;
        int m_nr_texture_units_used;
        int m_nr_image_units_used;
        int m_max_allowed_texture_units;
        int m_max_allowed_image_units;

        std::unordered_map<std::string, int > tex_sampler2texture_units;
        std::unordered_map<std::string, int > image2image_units;



        GLint get_uniform_location(std::string uniform_name){
            GLint uniform_location=glGetUniformLocation(m_prog_id,uniform_name.c_str());
            LOG_IF(WARNING,uniform_location==-1) << "Uniform location for name " << uniform_name << " is invalid. Are you sure you are using the uniform in the shader? Maybe you are also binding too many stuff.";
            return uniform_location;
        }

        //for compute shaders
        GLuint program_init( const std::string &compute_shader_string){
            GLuint compute_shader = load_shader(compute_shader_string,GL_COMPUTE_SHADER);
            GLuint program_shader = glCreateProgram();
            glAttachShader(program_shader, compute_shader);
            link_program_and_check(program_shader);
            std::cout << "linked a program with id " << program_shader << '\n';
            return program_shader;
        }

        //for program which have vertex and fragment shaders
        GLuint program_init( const std::string &vertex_shader_string, const std::string &fragment_shader_string){
            GLuint vertex_shader = load_shader(vertex_shader_string,GL_VERTEX_SHADER);
            GLuint fragment_shader = load_shader(fragment_shader_string, GL_FRAGMENT_SHADER);
            GLuint program_shader = glCreateProgram();
            glAttachShader(program_shader, vertex_shader);
            glAttachShader(program_shader, fragment_shader);
            link_program_and_check(program_shader);
            return program_shader;
        }

        //for program which have vertex, fragment and geometry shaders
        GLuint program_init( const std::string &vertex_shader_string, const std::string &fragment_shader_string, const std::string &geom_shader_string){
            GLuint vertex_shader = load_shader(vertex_shader_string,GL_VERTEX_SHADER);
            GLuint fragment_shader = load_shader(fragment_shader_string, GL_FRAGMENT_SHADER);
            GLuint geom_shader = load_shader(geom_shader_string, GL_GEOMETRY_SHADER);
            GLuint program_shader = glCreateProgram();
            glAttachShader(program_shader, vertex_shader);
            glAttachShader(program_shader, fragment_shader);
            glAttachShader(program_shader, geom_shader);
            link_program_and_check(program_shader);
            return program_shader;
        }

        void link_program_and_check(const GLuint& program_shader){
            glLinkProgram(program_shader);

            GLint status;
            glGetProgramiv(program_shader, GL_LINK_STATUS, &status);
            if (status != GL_TRUE){
                char buffer[512];
                glGetProgramInfoLog(program_shader, 512, NULL, buffer);
                LOG(ERROR) << "Linker error:" << std::endl << buffer;
                LOG(FATAL) << "Program linker error";
            }

        }

        GLuint load_shader( const std::string & src,const GLenum type) {
            if(src.empty()){
                return (GLuint) 0;
            }

            GLuint s = glCreateShader(type);
            if(s == 0){
                fprintf(stderr,"Error: load_shader() failed to create shader.\n");
                LOG(FATAL) << "Shader failed to be created. This should not happen. Maybe something is wrong with the GL context or your graphics card driver?";
                return 0;
            }
            // Pass shader source string
            const char *c = src.c_str();
            glShaderSource(s, 1, &c, NULL);
            glCompileShader(s);

            //check if it was compiled correctly
            GLint isCompiled = 0;
            glGetShaderiv(s, GL_COMPILE_STATUS, &isCompiled);
            if(isCompiled == GL_FALSE){
                GLint maxLength = 5000;
                glGetShaderiv(s, GL_INFO_LOG_LENGTH, &maxLength);

                // The maxLength includes the NULL character
                std::vector<GLchar> errorLog(maxLength);
                glGetShaderInfoLog(s, maxLength, &maxLength, &errorLog[0]);

                LOG(ERROR) << "Error: compiling shader" << std::endl << &errorLog[0];

                // Provide the infolog in whatever manor you deem best.
                // Exit with failure.
                glDeleteShader(s); // Don't leak the shader.
                LOG(FATAL) << "Shader failed to compile";
                return -1;
            }


            return s;
        }


        //only formats of 1,2 and 4 channels are allowed for ImageLoadStore. List is here https://www.khronos.org/opengl/wiki/Image_Load_Store
        template <class T>
        void check_format_is_valid_for_image_bind(const T& tex){
            GLint internal_format;
            internal_format=tex.get_internal_format();

            std::vector<GLint> allowed_internal_formats;
            allowed_internal_formats.push_back(GL_RGBA32F);
            allowed_internal_formats.push_back(GL_RGBA16F);
            allowed_internal_formats.push_back(GL_RG32F);
            allowed_internal_formats.push_back(GL_RG16F);
            allowed_internal_formats.push_back(GL_R11F_G11F_B10F);
            allowed_internal_formats.push_back(GL_R32F);
            allowed_internal_formats.push_back(GL_R16F);
            allowed_internal_formats.push_back(GL_RGBA16);
            allowed_internal_formats.push_back(GL_RGB10_A2);
            allowed_internal_formats.push_back(GL_RGBA8);
            allowed_internal_formats.push_back(GL_RG16);
            allowed_internal_formats.push_back(GL_RG8);
            allowed_internal_formats.push_back(GL_R16);
            allowed_internal_formats.push_back(GL_R8);
            allowed_internal_formats.push_back(GL_RGBA16_SNORM);
            allowed_internal_formats.push_back(GL_RGBA8_SNORM);
            allowed_internal_formats.push_back(GL_RG16_SNORM);
            allowed_internal_formats.push_back(GL_RG8_SNORM);
            allowed_internal_formats.push_back(GL_R16_SNORM);

            //check that we are a valid one
            for (size_t i = 0; i < allowed_internal_formats.size(); i++) {
                if(internal_format==allowed_internal_formats[i]){
                    return; //This is a valid format
                }
            }

            //we went through all of them and didn't find a valid one
            LOG(FATAL) << "Texture is not valid for ImageLoadStore operations. Check the valid list of internal formats at https://www.khronos.org/opengl/wiki/Image_Load_Store";


        }

    };

}
