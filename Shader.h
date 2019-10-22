#pragma once

#include <glad/glad.h>

#include <iostream>
#include <fstream>
#include <unordered_map>

#include <UtilsGL.h>
#include "Texture2D.h"
#include "Texture2DArray.h"
#include "Texture3D.h"
#include "Buf.h"
#include "GBuffer.h"
#include "CubeMap.h"

#include <iostream>

#include <Eigen/Core>

//use the maximum value of an int as invalid . We don't use negative because we sometimes compare with unsigned int
#define EGL_INVALID 2147483647 

namespace gl{
    class Shader{
    public:
        Shader():
            m_prog_id(EGL_INVALID),
            m_is_compiled(false),
            m_is_compute_shader(false),
            m_nr_texture_units_used(0),
            m_nr_image_units_used(0)
            {
                //when we bind a texture we use up a texture unit. We check that we don't go above this value
                glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &m_max_allowed_texture_units);

                //when we bind a image we use up a image unit. We check that we don't go above this value
                glGetIntegerv(GL_MAX_IMAGE_UNITS, &m_max_allowed_image_units);
        }

        Shader(const std::string name):
            Shader(){
            m_name=name;
        }

        ~Shader(){
            LOG(WARNING) << named("Destroying shader program");
            glUseProgram(0);
            glDeleteProgram(m_prog_id);
        }

        //rule of five (make the class non copyable)   
        Shader(const Shader& other) = delete; // copy ctor
        Shader& operator=(const Shader& other) = delete; // assignment op
        // Use default move ctors.  You have to declare these, otherwise the class will not have automatically generated move ctors.
        Shader (Shader && other) = default; //move ctor
        Shader & operator=(Shader &&) = default; //move assignment

        //compiles a program from various shaders
        void compile(const std::string &compute_shader_filename){
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
            CHECK(m_is_compiled) << named("Program is not compiled! Use prog.compile() first");
            glUseProgram(m_prog_id);
        }

        template <class T>
        void bind_texture(const T& tex, const std::string& uniform_name){
            CHECK(m_is_compiled) << named("Program is not compiled! Use prog.compile() first");
            CHECK(tex.get_tex_storage_initialized()) << named("Texture " + tex.name() + " has not storage initialized");

            //get the location in shader for the sampler
            GLint shader_location=glGetUniformLocation(m_prog_id,uniform_name.c_str());
            LOG_IF(WARNING,shader_location==-1) << named("Uniform location for name ") << uniform_name << " is invalid. Are you sure you are using the uniform in the shader? Maybe you are also binding too many stuff";

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
            CHECK(m_nr_texture_units_used<m_max_allowed_texture_units) << named("You used too many texture units! Try to bind less textures to the shader");

            glActiveTexture(GL_TEXTURE0 + cur_texture_unit);
            tex.bind(); //bind the texure to a certain texture unit
            glUniform1i(shader_location, cur_texture_unit); //the sampler will sample from that texture unit
        }

        //bind with a certain access mode a 2D image
        void bind_image(const gl::Texture2D& tex, const GLenum access, const std::string& uniform_name){
            CHECK(m_is_compiled) << named("Program is not compiled! Use prog.compile() first");
            CHECK(tex.get_tex_storage_initialized()) << named("Texture " + tex.name() + " has no storage initialized");
            CHECK(is_internal_format_valid_for_image_bind(tex.get_internal_format())) << named("Texture " ) << tex.name() << "is internal format invalid for image bind. Check the list of valid formats at https://www.khronos.org/opengl/wiki/Image_Load_Store";

            // check_format_is_valid_for_image_bind(tex);


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
            CHECK(m_nr_image_units_used<m_max_allowed_image_units) << named("You used too many image units! Try to bind less images to the shader");



            GL_C(glBindImageTexture(cur_image_unit, tex.get_tex_id(), 0, GL_FALSE, 0, access, tex.get_internal_format()));
        }

        //bind all layers of the Texture Array
        void bind_image(const gl::Texture2DArray& tex,  const GLenum access, const std::string& uniform_name){
            CHECK(m_is_compiled) << named("Program is not compiled! Use prog.compile() first");
            CHECK(tex.get_tex_storage_initialized()) << named("Texture " + tex.name() + " has no storage initialized");
            CHECK(is_internal_format_valid_for_image_bind(tex.get_internal_format())) << named("Texture " ) << tex.name() << "is internal format invalid for image bind. Check the list of valid formats at https://www.khronos.org/opengl/wiki/Image_Load_Store";

            // check_format_is_valid_for_image_bind(tex);

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
            CHECK(m_nr_image_units_used<m_max_allowed_image_units) << named("You used too many image units! Try to bind less images to the shader");

            GL_C(glBindImageTexture(cur_image_unit, tex.get_tex_id(), 0, GL_TRUE, 0, access, tex.get_internal_format()));
        }

        // //binding a Texture array but binds a specific layer
        // void bind_image(const gl::Texture2DArray& tex, const GLint layer, const GLenum access, const std::string& uniform_name){
        //     TODO get the type of texture from itself
        //     TODO get the image unit
        //     glBindImageTexture(1, tex.get_tex_id(), 0, GL_FALSE, layer, access, Format);
        // }


        //bind all layers of Texture 3D
        void bind_image(const gl::Texture3D& tex,  const GLenum access, const std::string& uniform_name){
            CHECK(m_is_compiled) << named("Program is not compiled! Use prog.compile() first");
            CHECK(tex.get_tex_storage_initialized()) << named("Texture " + tex.name() + " has no storage initialized");
            CHECK(is_internal_format_valid_for_image_bind(tex.get_internal_format())) << named("Texture " ) << tex.name() << "is internal format invalid for image bind. Check the list of valid formats at https://www.khronos.org/opengl/wiki/Image_Load_Store";
            // check_format_is_valid_for_image_bind(tex);

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
            CHECK(m_nr_image_units_used<m_max_allowed_image_units) << named("You used too many image units! Try to bind less images to the shader");

            GL_C(glBindImageTexture(cur_image_unit, tex.get_tex_id(), 0, GL_TRUE, 0, access, tex.get_internal_format()));
        }


        //bind a buffer
        void bind_buffer(const gl::Buf& buf, const std::string& uniform_name){

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
            CHECK(m_nr_image_units_used<m_max_allowed_image_units) << named("You used too many image units! Try to bind less images to the shader");

            glBindBufferBase(buf.get_target(), cur_image_unit, buf.get_buf_id());
        }

        GLint get_attrib_location(const std::string attrib_name) const{
            this->use();
            GLint attribute_location = glGetAttribLocation(m_prog_id, attrib_name.c_str());
            LOG_IF(WARNING,attribute_location==-1) << named("Attribute location for name ") << attrib_name << " is invalid. Are you sure you are using the attribute in the shader? Maybe you are also binding too many stuff.";
            return attribute_location;
        }

         void uniform_bool(const bool val, const std::string uniform_name){
            GLint uniform_location=get_uniform_location(uniform_name);
            glUniform1i(uniform_location, val);
        }

        void uniform_int(const int val, const std::string uniform_name){
            GLint uniform_location=get_uniform_location(uniform_name);
            glUniform1i(uniform_location, val);
        }

        void uniform_float(const float val, const std::string uniform_name){
            GLint uniform_location=get_uniform_location(uniform_name);
            glUniform1f(uniform_location, val);
        }

        void uniform_v2_float(const Eigen::Vector2f vec, const std::string uniform_name){
            GLint uniform_location=get_uniform_location(uniform_name);
            glUniform2fv(uniform_location, 1, vec.data());
        }

        void uniform_v3_float(const Eigen::Vector3f vec, const std::string uniform_name){
            GLint uniform_location=get_uniform_location(uniform_name);
            glUniform3fv(uniform_location, 1, vec.data());
        }

        void uniform_v4_float(const Eigen::Vector4f vec, const std::string uniform_name){
            GLint uniform_location=get_uniform_location(uniform_name);
            glUniform4fv(uniform_location, 1, vec.data());
        }

        //sends an array of vec3 to the shader. Inside the shader we declare it as vec3 array[SIZE] where size must correspond to the one being sent 
        void uniform_array_v3_float(const Eigen::MatrixXf mat, const std::string uniform_name){
            CHECK(mat.cols()==3) << named("The matrix should have 3 columns because we expect a matrix with N rows and 3 columns for the vec3 array.");
            for(int i=0; i<mat.rows(); i++){ 
                std::string uniform_array_name;
                uniform_array_name=uniform_name+"["+std::to_string(i)+"]";
                GLint uniform_location=get_uniform_location(uniform_array_name);
                Eigen::Vector3f row=mat.row(i); //cannot use directly mat.row(i).data() because that seems to give us eroneous data, maybe because of column major storing
                glUniform3fv(uniform_location, 1, row.data()); 
            }
        }

        //sends an array of vec2 to the shader. Inside the shader we declare it as vec2 array[SIZE] where size must correspond to the one being sent 
        void uniform_array_v2_float(const Eigen::MatrixXf mat, const std::string uniform_name){
            CHECK(mat.cols()==2) << named("The matrix should have 2 columns because we expect a matrix with N rows and 2 columns for the vec3 array.");
            for(int i=0; i<mat.rows(); i++){ 
                std::string uniform_array_name;
                uniform_array_name=uniform_name+"["+std::to_string(i)+"]";
                GLint uniform_location=get_uniform_location(uniform_array_name);
                Eigen::Vector2f row=mat.row(i); //cannot use directly mat.row(i).data() because that seems to give us eroneous data, maybe because of column major storing
                glUniform3fv(uniform_location, 1, row.data()); 
            }
        }

        void uniform_3x3(const Eigen::Matrix3f mat, const std::string uniform_name){
            GLint uniform_location=get_uniform_location(uniform_name);
            glUniformMatrix3fv(uniform_location, 1, GL_FALSE, mat.data());
        }

        void uniform_4x4(const Eigen::Matrix4f mat, const std::string uniform_name){
            GLint uniform_location=get_uniform_location(uniform_name);
            glUniformMatrix4fv(uniform_location, 1, GL_FALSE, mat.data());
        }

        void dispatch(const int total_x, const int total_y, const int local_size_x, const int local_size_y){
            CHECK(m_is_compute_shader) << named("Program is not a compute shader so we cannot dispatch it");
            glDispatchCompute(round_up_to_nearest_multiple(total_x,local_size_x)/local_size_x,
                              round_up_to_nearest_multiple(total_y,local_size_y)/local_size_y, 1 );
            glMemoryBarrier(GL_ALL_BARRIER_BITS);
        }

        //output2tex_list is a list of pair which map from the output of a shader to the corresponding name of the texture that we want to write into. 
        void draw_into(const GBuffer& gbuffer, std::initializer_list<  std::pair<std::string, std::string> > output2tex_list){
            CHECK(!m_is_compute_shader) << named("Program is a compute shader so we use to draw into gbuffer. Please use a fragment shader.");


            int max_location=-1; //will be used to determine how many drawbuffers should be used by seeing how many outputs does the fragment shader actually use
            for(auto output2tex : output2tex_list){
                std::string frag_out_name=output2tex.first; 
                int frag_out_location=glGetFragDataLocation(m_prog_id, frag_out_name.data());
                std::string tex_name=output2tex.second; 
                // int attachment_nr=gbuffer.attachment_nr(tex_name);
                LOG_IF(WARNING, frag_out_location==-1) << named("Fragment output location for name " + frag_out_name + " is either not declared in the shader or not being used for outputting anything.");
                // std::cout << frag_out_name << " with location " << frag_out_location <<  " will write into " << tex_name << " with attachment nr" << attachment_nr << '\n';

                if(frag_out_location>max_location){
                    max_location=frag_out_location;
                }
            }
            int nr_draw_buffers=max_location+1; //if max location is 1 it means we use locations 0 and 1 so therefore we need 2 drawbuffers
            GLenum draw_buffers[nr_draw_buffers];
            for(int i=0; i<nr_draw_buffers; i++){
                draw_buffers[i]=GL_NONE; //initialize to gl_none
            }

            //fill the draw buffers 
             for(auto output2tex : output2tex_list){
                std::string frag_out_name=output2tex.first; 
                int frag_out_location=glGetFragDataLocation(m_prog_id, frag_out_name.data());
                std::string tex_name=output2tex.second; 
                int attachment_nr=gbuffer.attachment_nr(tex_name);

                draw_buffers[frag_out_location]=GL_COLOR_ATTACHMENT0+attachment_nr;

                // std::cout << '\n';
                // std::cout << "frag_out_location" << frag_out_location << "set to attachment" << attachment_nr << '\n';
            }

            // tthe layout qualifers inside the shader index into this gluenum array and say for each output where do we write into 
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, gbuffer.get_fbo_id());
            // glClearColor(0.0, 0.0, 0.0, 0.0);
            // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glDrawBuffers(nr_draw_buffers, draw_buffers);

        }


        //draw into just one texture, which is not assigned to a gbuffer 
        void draw_into(const Texture2D& tex, const std::string frag_out_name){
            CHECK(!m_is_compute_shader) << named("Program is a compute shader so we use to draw into gbuffer. Please use a fragment shader.");


            int frag_out_location=glGetFragDataLocation(m_prog_id, frag_out_name.data());
            LOG_IF(WARNING, frag_out_location==-1) << named("Fragment output location for name " + frag_out_name + " is either not declared in the shader or not being used for outputting anything.");

            int max_location=frag_out_location; //we only suppose we have one location in which we draw
            int nr_draw_buffers=max_location+1; //if max location is 1 it means we use locations 0 and 1 so therefore we need 2 drawbuffers
            GLenum draw_buffers[nr_draw_buffers];
            for(int i=0; i<nr_draw_buffers; i++){
                draw_buffers[i]=GL_NONE; //initialize to gl_none
            }
            draw_buffers[frag_out_location]=GL_COLOR_ATTACHMENT0; //the texture is only assigned as color0 for its fbo
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, tex.fbo_id());
            glDrawBuffers(nr_draw_buffers, draw_buffers);

        }

        //draw into one of the faces of a cubemap
        void draw_into(const CubeMap& tex, const std::string frag_out_name, const int cube_face_idx, const int mip=0){
            CHECK(!m_is_compute_shader) << named("Program is a compute shader so we use to draw into gbuffer. Please use a fragment shader.");


            int frag_out_location=glGetFragDataLocation(m_prog_id, frag_out_name.data());
            LOG_IF(WARNING, frag_out_location==-1) << named("Fragment output location for name " + frag_out_name + " is either not declared in the shader or not being used for outputting anything.");

            int max_location=frag_out_location; //we only suppose we have one location in which we draw
            int nr_draw_buffers=max_location+1; //if max location is 1 it means we use locations 0 and 1 so therefore we need 2 drawbuffers
            GLenum draw_buffers[nr_draw_buffers];
            for(int i=0; i<nr_draw_buffers; i++){
                draw_buffers[i]=GL_NONE; //initialize to gl_none
            }
            draw_buffers[frag_out_location]=GL_COLOR_ATTACHMENT0; //the texture is only assigned as color0 for its fbo
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, tex.fbo_id()); //bindframbuffer
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X+cube_face_idx, tex.get_tex_id(), mip); //attach to the framebuffer the correct face
            glDrawBuffers(nr_draw_buffers, draw_buffers);
        }


        int get_prog_id() const{
            return m_prog_id;
        }
        
        GLint get_uniform_location(std::string uniform_name){
            this->use();
            GLint uniform_location=glGetUniformLocation(m_prog_id,uniform_name.c_str());
            LOG_IF(WARNING,uniform_location==-1) << named("Uniform location for name ") << uniform_name << " is invalid. Are you sure you are using the uniform in the shader? Maybe you are also binding too many stuff.";
            return uniform_location;
        }
        

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

        std::string named(const std::string msg) const{
            return m_name.empty()? msg : m_name + ": " + msg; 
        } 


        //for compute shaders
        GLuint program_init( const std::string &compute_shader_string){
            GLuint compute_shader = load_shader(compute_shader_string,GL_COMPUTE_SHADER);
            GLuint program_shader = glCreateProgram();
            glAttachShader(program_shader, compute_shader);
            link_program_and_check(program_shader);
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
                LOG(ERROR) << named("Linker error: ") << std::endl << buffer;
                LOG(FATAL) << named("Program linker error");
            }

        }

        GLuint load_shader( const std::string & src,const GLenum type) {
            if(src.empty()){
                return (GLuint) 0;
            }

            GLuint s = glCreateShader(type);
            if(s == 0){
                fprintf(stderr,"Error: load_shader() failed to create shader.\n");
                LOG(FATAL) << named("Shader failed to be created. This should not happen. Maybe something is wrong with the GL context or your graphics card driver?");
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

                LOG(ERROR) << named("Error: compiling shader") << std::endl << &errorLog[0];

                // Provide the infolog in whatever manor you deem best.
                // Exit with failure.
                glDeleteShader(s); // Don't leak the shader.
                LOG(FATAL) << named("Shader failed to compile");
                return -1;
            }


            return s;
        }


        // //only formats of 1,2 and 4 channels are allowed for ImageLoadStore. List is here https://www.khronos.org/opengl/wiki/Image_Load_Store
        // template <class T>
        // void check_format_is_valid_for_image_bind(const T& tex){
        //     GLint internal_format;
        //     internal_format=tex.get_internal_format();

        //     std::vector<GLint> allowed_internal_formats;
        //     allowed_internal_formats.push_back(GL_RGBA32F);
        //     allowed_internal_formats.push_back(GL_RGBA16F);
        //     allowed_internal_formats.push_back(GL_RG32F);
        //     allowed_internal_formats.push_back(GL_RG16F);
        //     // allowed_internal_formats.push_back(GL_R11F_G11F_B10F); no channel 3 allowed because it will get translated to 4 channels either way
        //     allowed_internal_formats.push_back(GL_R32F);
        //     allowed_internal_formats.push_back(GL_R16F);
        //     // allowed_internal_formats.push_back(GL_RGBA16); //ambigous, I prefer the version with RGBA16UI or RGBA16I
        //     // allowed_internal_formats.push_back(GL_RGB10_A2);
        //     // allowed_internal_formats.push_back(GL_RGBA8);
        //     // allowed_internal_formats.push_back(GL_RG16);
        //     // allowed_internal_formats.push_back(GL_RG8);
        //     // allowed_internal_formats.push_back(GL_R16);
        //     // allowed_internal_formats.push_back(GL_R8);
        //     allowed_internal_formats.push_back(GL_RGBA16_SNORM);
        //     allowed_internal_formats.push_back(GL_RGBA8_SNORM);
        //     allowed_internal_formats.push_back(GL_RG16_SNORM);
        //     allowed_internal_formats.push_back(GL_RG8_SNORM);
        //     allowed_internal_formats.push_back(GL_R16_SNORM);
        //     allowed_internal_formats.push_back(GL_RGBA32UI);	
        //     allowed_internal_formats.push_back(GL_RGBA16UI);	
        //     allowed_internal_formats.push_back(GL_RGB10_A2UI); 	
        //     allowed_internal_formats.push_back(GL_RGBA8UI); 	
        //     allowed_internal_formats.push_back(GL_RG32UI); 	
        //     allowed_internal_formats.push_back(GL_RG16UI); 	
        //     allowed_internal_formats.push_back(GL_RG8UI); 	
        //     allowed_internal_formats.push_back(GL_R32UI); 
        //     allowed_internal_formats.push_back(GL_R16UI); 	
        //     allowed_internal_formats.push_back(GL_R8UI); 	
        //     allowed_internal_formats.push_back(GL_RGBA32I); 
        //     allowed_internal_formats.push_back(GL_RGBA16I); 
        //     allowed_internal_formats.push_back(GL_RGBA8I); 	
        //     allowed_internal_formats.push_back(GL_RG32I); 	
        //     allowed_internal_formats.push_back(GL_RG16I); 	
        //     allowed_internal_formats.push_back(GL_RG8I); 	
        //     allowed_internal_formats.push_back(GL_R32I); 	
        //     allowed_internal_formats.push_back(GL_R16I); 	
        //     allowed_internal_formats.push_back(GL_R8I); 	

        //     //check that we are a valid one
        //     for (size_t i = 0; i < allowed_internal_formats.size(); i++) {
        //         if(internal_format==allowed_internal_formats[i]){
        //             return; //This is a valid format
        //         }
        //     }

        //     //we went through all of them and didn't find a valid one
        //     LOG(FATAL) << named("Texture is not valid for ImageLoadStore operations. Check the valid list of internal formats at https://www.khronos.org/opengl/wiki/Image_Load_Store");


        // }


        inline std::string file_to_string (const std::string &filename){
            std::ifstream t(filename);
            if (!t.is_open()){
                LOG(FATAL) << named("Cannot open file ") << filename;
            }
            return std::string((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
        }

    };

}
