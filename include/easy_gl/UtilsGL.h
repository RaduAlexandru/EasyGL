#pragma once

#ifdef EASYPBR_WITH_TORCH
    #include "torch/torch.h"
    #include "c10/core/ScalarType.h" //for the function elementSize which return the number of bytes for a certain scalartype of torch
#endif


//eigen
#include <Eigen/Core>

//GL
#include <glad/glad.h>

//c++
#include <iostream>

//opencv
#include "opencv2/opencv.hpp"

//loguru
#define LOGURU_REPLACE_GLOG 1
#include <loguru.hpp>

//https://blog.noctua-software.com/opencv-opengl-projection-matrix.html
inline Eigen::Matrix4f intrinsics_to_opengl_proj(const Eigen::Matrix3f& K, const int width, const int height, float znear=0.1f, float zfar=100.0f){
    //apllying glscale like here solves the flipping issue https://www.opengl.org/discussion_boards/showthread.php/144492-Flip-entire-framebuffer-upside-down
    //effectivelly we flip m(1,1) and m(2,1)

    Eigen::Matrix4f m;
    float fx=K(0,0);
    float fy=K(1,1);
    float cx=K(0,2);
    float cy=K(1,2);


    //attempt 4 (http://ksimek.github.io/2013/06/03/calibrated_cameras_in_opengl)
    Eigen::Matrix4f persp = Eigen::Matrix4f::Zero();
    persp(0,0) = fx;                      persp(0,2) = -cx;
                        persp(1,1) = fy;  persp(1,2) = -height+cy; //we flip the cy because we assume that the opencv 
                                          persp(2,2) = (znear+zfar); persp(2,3) = znear*zfar;
                                          persp(3,2) = -1.0;

     Eigen::Matrix4f ortho = Eigen::Matrix4f::Zero();
     ortho(0,0) =  2.0/width; ortho(0,3) = -1;
     ortho(1,1) =  2.0/height; ortho(1,3) = -1;
     ortho(2,2) = -2.0/(zfar-znear); ortho(2,3) = -(zfar+znear)/(zfar-znear);
     ortho(3,3) =  1.0;


    m = ortho*persp;
    //need t flip the z axis for some reason
     m(0,2)=-m(0,2);
     m(1,2)=-m(1,2);
     m(2,2)=-m(2,2);
     m(3,2)=-m(3,2);

    return m;
}

inline Eigen::Matrix3f opengl_proj_to_intrinsics(const Eigen::Matrix4f& P, const int width, const int height){
    //from https://fruty.io/2019/08/29/augmented-reality-with-opencv-and-opengl-the-tricky-projection-matrix/

    float fx,fy,cx,cy;

    //the element (0,0) of the projection matrix is -2.0*fx/w, therefore fx=-(0,0)*w/2.0
    fx=P(0,0)*width/2.0;
    fy=P(1,1)*height/2.0;

    // the (0,2) element is X= 2*cx/w +1  SO 2*cx=(X-1)*w
    cx=-(-P(0,2)-1)*width/2.0;
    cy=-(-P(1,2)-1)*height/2.0;

    Eigen::Matrix3f K;
    K.setIdentity();
    K(0,0)=fx;
    K(1,1)=fy;
    K(0,2)=cx;
    K(1,2)=cy;


    return K;
}



// OpenGL-error callback function
// Used when GL_ARB_debug_output is supported
inline void APIENTRY debug_func(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
	const GLchar* message, GLvoid const* userParam)
{
	std::string srcName;
	switch(source)
	{
	case GL_DEBUG_SOURCE_API_ARB: srcName = "API"; break;
	case GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB: srcName = "Window System"; break;
	case GL_DEBUG_SOURCE_SHADER_COMPILER_ARB: srcName = "ShaderProgram Compiler"; break;
	case GL_DEBUG_SOURCE_THIRD_PARTY_ARB: srcName = "Third Party"; break;
	case GL_DEBUG_SOURCE_APPLICATION_ARB: srcName = "Application"; break;
	case GL_DEBUG_SOURCE_OTHER_ARB: srcName = "Other"; break;
	}

	std::string errorType;
	switch(type)
	{
	case GL_DEBUG_TYPE_ERROR_ARB: errorType = "Error"; break;
	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB: errorType = "Deprecated Functionality"; break;
	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB: errorType = "Undefined Behavior"; break;
	case GL_DEBUG_TYPE_PORTABILITY_ARB: errorType = "Portability"; break;
	case GL_DEBUG_TYPE_PERFORMANCE_ARB: errorType = "Performance"; break;
	case GL_DEBUG_TYPE_OTHER_ARB: errorType = "Other"; break;
	}

	std::string typeSeverity;
	switch(severity)
	{
	case GL_DEBUG_SEVERITY_HIGH_ARB: typeSeverity = "High"; break;
	case GL_DEBUG_SEVERITY_MEDIUM_ARB: typeSeverity = "Medium"; break;
	case GL_DEBUG_SEVERITY_LOW_ARB: typeSeverity = "Low"; break;
	}

    int loguru_severity=1;
    if(typeSeverity == "High"){
        loguru_severity=loguru::Verbosity_ERROR;
    }else if (typeSeverity == "Medium"){
        loguru_severity=loguru::Verbosity_WARNING;
    }else if (typeSeverity == "Low"){
        loguru_severity=loguru::Verbosity_4;
    }else{
        loguru_severity=loguru::Verbosity_4;
    }

    VLOG(loguru_severity) << message;

    // if(typeSeverity == "High"){
    //     auto st = loguru::stacktrace(1);
    //     if (!st.empty()) {
    //         RAW_LOG_F(ERROR, "Stack trace:\n%s", st.c_str());
    //     }
    // }

	// printf("%s from %s,\t%s priority\nMessage: %s\n",
	// 	errorType.c_str(), srcName.c_str(), typeSeverity.c_str(), message);
}

inline void print_supported_extensions(){
    GLint ExtensionCount = 0;
	glGetIntegerv(GL_NUM_EXTENSIONS, &ExtensionCount);
	for(GLint i = 0; i < ExtensionCount; ++i){
        std::cout << std::string((char const*)glGetStringi(GL_EXTENSIONS, i)) << '\n';
    }
}
// inline void bind_for_sampling(const GLenum texture_target, const GLuint texture_id, const GLint texture_unit, const GLint shader_location){
//     //https://www.opengl.org/discussion_boards/showthread.php/174926-when-to-use-glActiveTexture
//     GLenum cur_texture_unit = GL_TEXTURE0 + texture_unit;
//     glActiveTexture(cur_texture_unit);
//     glBindTexture(texture_target, texture_id);
//     glUniform1i(shader_location, texture_unit);
// }
//
// inline void bind_for_sampling(const gl::Texture2D& texture, const GLint texture_unit, const GLint shader_location){
//     //https://www.opengl.org/discussion_boards/showthread.php/174926-when-to-use-glActiveTexture
//     GLenum cur_texture_unit = GL_TEXTURE0 + texture_unit;
//     glActiveTexture(cur_texture_unit);
//     texture.bind();
//     glUniform1i(shader_location, texture_unit);
// }
//
// inline void bind_for_sampling(const gl::Texture2DArray& texture, const GLint texture_unit, const GLint shader_location){
//     //https://www.opengl.org/discussion_boards/showthread.php/174926-when-to-use-glActiveTexture
//     GLenum cur_texture_unit = GL_TEXTURE0 + texture_unit;
//     glActiveTexture(cur_texture_unit);
//     texture.bind();
//     glUniform1i(shader_location, texture_unit);
// }

inline void CheckOpenGLError(const char* stmt, const char* fname, int line)
{
    GLenum err = glGetError();
    //  const GLubyte* sError = gluErrorString(err);

    if (err != GL_NO_ERROR){
        printf("OpenGL error %08x, at %s:%i - for %s.\n", err, fname, line, stmt);
        exit(1);
    }
}

// GL Check Macro. Will terminate the program if a GL error is detected.
#define GL_C(stmt) do {					\
	stmt;						\
	CheckOpenGLError(#stmt, __FILE__, __LINE__);	\
} while (0)

// #ifdef WITH_GLM
// //https://stackoverflow.com/a/20545775
// #include <glm/gtx/string_cast.hpp>
// #include <type_traits>
// #include <utility>
// template <typename GLMType, typename = decltype(glm::to_string(std::declval<GLMType>()))>
// inline std::ostream& operator<<(std::ostream& out, const GLMType& g)
// {
//     return out << glm::to_string(g);
// }
// #endif

inline int gl_internal_format2cv_type(const GLint internal_format){

    int cv_type=0;
    switch ( internal_format ) {
        //the ones with uchar (They don't work for some reason... I think it has to be Just G_RGB and not GL_RGB8UI)
       case GL_R8UI: cv_type=CV_8UC1;  break;
       case GL_RG8UI: cv_type=CV_8UC2; break;
       case GL_RGB8UI: cv_type=CV_8UC3; break;
       case GL_RGBA8UI: cv_type=CV_8UC4; break;

       case GL_R8: cv_type=CV_8UC1;  break;
       case GL_RG8: cv_type=CV_8UC2; break;
       case GL_RGB8: cv_type=CV_8UC3; break;
       case GL_RGBA8: cv_type=CV_8UC4; break;

       case GL_R32I: cv_type=CV_32SC1;  break;
       case GL_RG32I: cv_type=CV_32SC2;  break;
       case GL_RGB32I: cv_type=CV_32SC3;  break;
       case GL_RGBA32I: cv_type=CV_32SC4;  break;

       //the ones with float
       case GL_R32F: cv_type=CV_32FC1;  break;
       case GL_RG32F: cv_type=CV_32FC2;  break;
       case GL_RGB32F: cv_type=CV_32FC3;  break;
       case GL_RGBA32F: cv_type=CV_32FC4;  break;
       //print the internal forma tin hex because the glad.h header stores them like that so it'seasy to look up
       default:  LOG(FATAL) << "Internal format "<< std::hex << internal_format << std::dec <<  " unkown. We support only 8, 8UI and 32F and 1, 2, 3 and 4 channels"; break;
    }

    return cv_type;
}


inline void cv_type2gl_formats(GLint& internal_format, GLenum& format, GLenum& type, const int cv_type, const bool flip_red_blue, const bool store_as_normalized_vals){


    //from the cv format get the corresponding gl internal_format, format and type
    unsigned char depth = cv_type & CV_MAT_DEPTH_MASK;
    unsigned char channels = 1 + (cv_type >> CV_CN_SHIFT);

    if(depth==CV_8U && store_as_normalized_vals){
        type=GL_UNSIGNED_BYTE;
        switch ( channels ) {
           case 1: internal_format=GL_R8; format=GL_RED;  break;
           case 2: internal_format=GL_RG8; format=GL_RG;  break;
           case 3:
                internal_format=GL_RGB8;
                   if(flip_red_blue){
                       format=GL_BGR;
                   }else{
                       format=GL_RGB;
                   }
                   break;
           case 4:
                   internal_format=GL_RGBA8;
                   if(flip_red_blue){
                       format=GL_BGRA;
                   }else{
                       format=GL_RGBA;
                   }
                   break;
           default:  LOG(FATAL) << "Nr of channels not supported. We only support 1, 2, 3 and 4."; break;
        }
    }else if(depth==CV_8U && !store_as_normalized_vals){
        type=GL_UNSIGNED_BYTE;
        switch ( channels ) {
           case 1: internal_format=GL_R8UI; format=GL_RED_INTEGER;  break;
           case 2: internal_format=GL_RG8UI; format=GL_RG_INTEGER;  break;
           case 3:
                    internal_format=GL_RGB8UI;
                    if(flip_red_blue){
                        format=GL_BGR_INTEGER;
                    }else{
                        format=GL_RGB_INTEGER;
                    }
                    break;
           case 4:
                    internal_format=GL_RGBA8UI;
                    if(flip_red_blue){
                        format=GL_BGRA_INTEGER;
                    }else{
                        format=GL_RGBA_INTEGER;
                    }
                    break;
           default: LOG(FATAL) << "Nr of channels not supported. We only support 1, 2, 3 and 4."; break;
        }
    }
    else if(depth==CV_32F){
        type=GL_FLOAT;
        switch ( channels ) {
           case 1: internal_format=GL_R32F; format=GL_RED;  break;
           case 2: internal_format=GL_RG32F; format=GL_RG;  break;
           case 3:
                    internal_format=GL_RGB32F;
                    if(flip_red_blue){
                        format=GL_BGR;
                    }else{
                        format=GL_RGB;
                    }
                    break;
           case 4:
                    internal_format=GL_RGBA32F;
                    if(flip_red_blue){
                        format=GL_BGRA;
                    }else{
                        format=GL_RGBA;
                    }
                    break;
           default: LOG(FATAL) << "Nr of channels not supported. We only support 1, 2, 3 and 4."; break;
        }
    }else{
        LOG(FATAL) << "CV mat is only supported for types of unsigned byte and float. Check the depth of your cv mat";
    }


}

#ifdef EASYPBR_WITH_TORCH

    //given an internal format of the texture, return the appropriate channels and scalar type for the tensor
    inline void gl_internal_format2tensor_type(int& nr_channels_tensor, torch::ScalarType& scalar_type_tensor, const GLint internal_format){

        //cuda doesnt support 3 channels as explained in the function cudaGraphicsGLRegisterImage here https://docs.nvidia.com/cuda/cuda-runtime-api/group__CUDART__OPENGL.html#group__CUDART__OPENGL_1g80d12187ae7590807c7676697d9fe03d

        switch ( internal_format ) {
            //the ones with uchar (They don't work for some reason... I think it has to be Just G_RGB and not GL_RGB8UI)
        case GL_R8UI: nr_channels_tensor=1; scalar_type_tensor=torch::kUInt8;  break;
        case GL_RG8UI: nr_channels_tensor=2; scalar_type_tensor=torch::kUInt8;  break;
        // case GL_RGB8UI: nr_channels_tensor=3; scalar_type_tensor=torch::kUInt8;  break;
        case GL_RGBA8UI: nr_channels_tensor=4; scalar_type_tensor=torch::kUInt8;  break;

        case GL_R8: nr_channels_tensor=1; scalar_type_tensor=torch::kUInt8;  break;
        case GL_RG8: nr_channels_tensor=2; scalar_type_tensor=torch::kUInt8;  break;
        // case GL_RGB8: nr_channels_tensor=3; scalar_type_tensor=torch::kUInt8;  break;
        case GL_RGBA8: nr_channels_tensor=4; scalar_type_tensor=torch::kUInt8;  break;

        case GL_R32I: nr_channels_tensor=1; scalar_type_tensor=torch::kInt32;  break;
        case GL_RG32I: nr_channels_tensor=2; scalar_type_tensor=torch::kInt32;  break;
        // case GL_RGB32I: nr_channels_tensor=3; scalar_type_tensor=torch::kInt32;  break;
        case GL_RGBA32I: nr_channels_tensor=4; scalar_type_tensor=torch::kInt32;  break;

        //the ones with float
        case GL_R32F: nr_channels_tensor=1; scalar_type_tensor=torch::kFloat32;  break;
        case GL_RG32F: nr_channels_tensor=2; scalar_type_tensor=torch::kFloat32;  break;
        // case GL_RGB32F: nr_channels_tensor=3; scalar_type_tensor=torch::kFloat32;  break;
        case GL_RGBA32F: nr_channels_tensor=4; scalar_type_tensor=torch::kFloat32;  break;
        //print the internal forma tin hex because the glad.h header stores them like that so it'seasy to look up
        default:  LOG(FATAL) << "Internal format "<< std::hex << internal_format << std::dec <<  " unkown. We support only 8, 8UI and 32F and 1, 2, and 4 channels. Cuda doesnt support 3 channels"; break;
        }

    }

    //from the tensor type get the approapraite gl internal format, format and type
    inline void tensor_type2gl_formats(GLint& internal_format, GLenum& format, GLenum& type, const int tensor_channels, const torch::ScalarType tensor_scalar_type, const bool flip_red_blue, const bool store_as_normalized_vals){


        //from the tensor format get the corresponding gl internal_format, format and type
        unsigned char channels = tensor_channels;

        if(tensor_scalar_type==torch::kUInt8 && store_as_normalized_vals){
            type=GL_UNSIGNED_BYTE;
            switch ( channels ) {
            case 1: internal_format=GL_R8; format=GL_RED;  break;
            case 2: internal_format=GL_RG8; format=GL_RG;  break;
            case 3:
                    internal_format=GL_RGB8;
                    if(flip_red_blue){
                        format=GL_BGR;
                    }else{
                        format=GL_RGB;
                    }
                    break;
            case 4:
                    internal_format=GL_RGBA8;
                    if(flip_red_blue){
                        format=GL_BGRA;
                    }else{
                        format=GL_RGBA;
                    }
                    break;
            default:  LOG(FATAL) << "Nr of channels not supported. We only support 1, 2, 3 and 4."; break;
            }
        }else if(tensor_scalar_type==torch::kUInt8 && !store_as_normalized_vals){
            type=GL_UNSIGNED_BYTE;
            switch ( channels ) {
            case 1: internal_format=GL_R8UI; format=GL_RED_INTEGER;  break;
            case 2: internal_format=GL_RG8UI; format=GL_RG_INTEGER;  break;
            case 3:
                        internal_format=GL_RGB8UI;
                        if(flip_red_blue){
                            format=GL_BGR_INTEGER;
                        }else{
                            format=GL_RGB_INTEGER;
                        }
                        break;
            case 4:
                        internal_format=GL_RGBA8UI;
                        if(flip_red_blue){
                            format=GL_BGRA_INTEGER;
                        }else{
                            format=GL_RGBA_INTEGER;
                        }
                        break;
            default: LOG(FATAL) << "Nr of channels not supported. We only support 1, 2, 3 and 4."; break;
            }
        }
        else if(tensor_scalar_type==torch::kFloat32){
            type=GL_FLOAT;
            switch ( channels ) {
            case 1: internal_format=GL_R32F; format=GL_RED;  break;
            case 2: internal_format=GL_RG32F; format=GL_RG;  break;
            case 3:
                        internal_format=GL_RGB32F;
                        if(flip_red_blue){
                            format=GL_BGR;
                        }else{
                            format=GL_RGB;
                        }
                        break;
            case 4:
                        internal_format=GL_RGBA32F;
                        if(flip_red_blue){
                            format=GL_BGRA;
                        }else{
                            format=GL_RGBA;
                        }
                        break;
            default: LOG(FATAL) << "Nr of channels not supported. We only support 1, 2, 3 and 4."; break;
            }
        }else{
            LOG(FATAL) << "CV mat is only supported for types of unsigned byte and float. Check the depth of your cv mat";
        }


    }
#endif




///from the internal format of the GL representation get the possible type and format that was used for inputing data into that texture
inline void gl_internal_format2format_and_type(GLenum& format, GLenum& type, const GLint& internal_format, const bool flip_red_blue, const bool denormalize){
    //a way of doing it without replicating code is by using the above two functions

    int cv_type=gl_internal_format2cv_type(internal_format);

    //from the cv format get the corresponding gl internal_format, format and type
    GLint returned_internal_format;
    cv_type2gl_formats(returned_internal_format, format, type, cv_type, flip_red_blue, denormalize);

    //santy check. The internal format that was passed in and the one after the roundabout should be the same
    // CHECK(returned_internal_format==internal_format) << "Something went wrong when we went from gl to cv and back to gl type. Original internal format was " << std::hex << internal_format << " but we got back from the roundabut " <<returned_internal_format << std::dec ;
}

//sometimes you want to allocate memory that is multiple of 64 bytes, so therefore you want to allocate more memory but you need a nr that is divisible by 64
inline int round_up_to_nearest_multiple(const int number, const int divisor){
 return number - number % divisor + divisor * !!(number % divisor);
}

//calculates for a certain full sized image what would be size at a certain mip map level
inline Eigen::Vector2i calculate_mipmap_size(const int full_w, const int full_h, const int level){
    int new_w=std::max<int>(1, floor(full_w / pow(2,level) )  );
    int new_h=std::max<int>(1, floor(full_h / pow(2,level) )  );
    Eigen::Vector2i new_size;
    new_size<< new_w, new_h;
    return new_size;
}

inline bool is_internal_format_valid(const GLenum internal_format){

    //taken from https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glTexImage2D.xhtml
    //not all of them are here but rather the ones that are specific about the format so we don't consider RGBA a valid one and prefer, RGBA8 RGBA8UI or RGBA8I,d
    std::vector<GLenum> allowed_internal_formats;


    allowed_internal_formats.push_back(GL_R8);
    allowed_internal_formats.push_back(GL_R8_SNORM);
    allowed_internal_formats.push_back(GL_R16);
    allowed_internal_formats.push_back(GL_R16_SNORM);
    allowed_internal_formats.push_back(GL_RG8);
    allowed_internal_formats.push_back(GL_RG8_SNORM);
    allowed_internal_formats.push_back(GL_RG16);
    allowed_internal_formats.push_back(GL_RG16_SNORM);
    allowed_internal_formats.push_back(GL_R3_G3_B2);
    allowed_internal_formats.push_back(GL_RGB4);
    allowed_internal_formats.push_back(GL_RGB5);
    allowed_internal_formats.push_back(GL_RGB8);
    allowed_internal_formats.push_back(GL_RGB8_SNORM);
    allowed_internal_formats.push_back(GL_RGB10);
    allowed_internal_formats.push_back(GL_RGB12);
    allowed_internal_formats.push_back(GL_RGB16_SNORM);
    allowed_internal_formats.push_back(GL_RGBA2);
    allowed_internal_formats.push_back(GL_RGBA4);
    allowed_internal_formats.push_back(GL_RGB5_A1);
    allowed_internal_formats.push_back(GL_RGBA8);
    allowed_internal_formats.push_back(GL_RGBA8_SNORM);
    allowed_internal_formats.push_back(GL_RGB10_A2);
    allowed_internal_formats.push_back(GL_RGB10_A2UI);
    allowed_internal_formats.push_back(GL_RGBA12);
    allowed_internal_formats.push_back(GL_RGBA16);
    allowed_internal_formats.push_back(GL_SRGB8);
    allowed_internal_formats.push_back(GL_SRGB8_ALPHA8);
    allowed_internal_formats.push_back(GL_R16F);
    allowed_internal_formats.push_back(GL_RG16F);
    allowed_internal_formats.push_back(GL_RGB16F);
    allowed_internal_formats.push_back(GL_RGBA16F);
    allowed_internal_formats.push_back(GL_R32F);
    allowed_internal_formats.push_back(GL_RG32F);
    allowed_internal_formats.push_back(GL_RGB32F);
    allowed_internal_formats.push_back(GL_RGBA32F);
    allowed_internal_formats.push_back(GL_R11F_G11F_B10F);
    allowed_internal_formats.push_back(GL_RGB9_E5);
    allowed_internal_formats.push_back(GL_R8I);
    allowed_internal_formats.push_back(GL_R8UI);
    allowed_internal_formats.push_back(GL_R16I);
    allowed_internal_formats.push_back(GL_R16UI);
    allowed_internal_formats.push_back(GL_R32I);
    allowed_internal_formats.push_back(GL_R32UI);
    allowed_internal_formats.push_back(GL_RG8I);
    allowed_internal_formats.push_back(GL_RG8UI);
    allowed_internal_formats.push_back(GL_RG16I);
    allowed_internal_formats.push_back(GL_RG16UI);
    allowed_internal_formats.push_back(GL_RG32I);
    allowed_internal_formats.push_back(GL_RG32UI);
    allowed_internal_formats.push_back(GL_RGB8I);
    allowed_internal_formats.push_back(GL_RGB8UI);
    allowed_internal_formats.push_back(GL_RGB16I);
    allowed_internal_formats.push_back(GL_RGB16UI);
    allowed_internal_formats.push_back(GL_RGB32I);
    allowed_internal_formats.push_back(GL_RGB32UI);
    allowed_internal_formats.push_back(GL_RGBA8I);
    allowed_internal_formats.push_back(GL_RGBA8UI);
    allowed_internal_formats.push_back(GL_RGBA16I);
    allowed_internal_formats.push_back(GL_RGBA16UI);
    allowed_internal_formats.push_back(GL_RGBA32I);
    allowed_internal_formats.push_back(GL_RGBA32UI);
    allowed_internal_formats.push_back(GL_DEPTH_COMPONENT32F);
    allowed_internal_formats.push_back(GL_DEPTH_COMPONENT32);
    allowed_internal_formats.push_back(GL_DEPTH_COMPONENT24);
    allowed_internal_formats.push_back(GL_DEPTH_COMPONENT16);
    allowed_internal_formats.push_back(GL_DEPTH24_STENCIL8);
    allowed_internal_formats.push_back(GL_DEPTH32F_STENCIL8);
    allowed_internal_formats.push_back(GL_STENCIL_INDEX8);

    //check that we are a valid one
    for (size_t i = 0; i < allowed_internal_formats.size(); i++) {
        if(internal_format==allowed_internal_formats[i]){
            return true; //This is a valid format
        }
    }

    return false;
}

inline bool is_format_valid(const GLenum format){

    //taken from https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glTexImage2D.xhtml
    std::vector<GLenum> allowed_formats;
    allowed_formats.push_back(GL_RED);
    allowed_formats.push_back(GL_RG);
    allowed_formats.push_back(GL_RGB);
    allowed_formats.push_back(GL_BGR);
    allowed_formats.push_back(GL_RGBA);
    allowed_formats.push_back(GL_BGRA);
    allowed_formats.push_back(GL_RED_INTEGER);
    allowed_formats.push_back(GL_RG_INTEGER);
    allowed_formats.push_back(GL_RGB_INTEGER);
    allowed_formats.push_back(GL_BGR_INTEGER);
    allowed_formats.push_back(GL_RGBA_INTEGER);
    allowed_formats.push_back(GL_BGRA_INTEGER);
    allowed_formats.push_back(GL_STENCIL_INDEX);
    allowed_formats.push_back(GL_DEPTH_COMPONENT);
    allowed_formats.push_back(GL_DEPTH_STENCIL);

    //check that we are a valid one
    for (size_t i = 0; i < allowed_formats.size(); i++) {
        if(format==allowed_formats[i]){
            return true; //This is a valid format
        }
    }

    return false;
}

inline bool is_type_valid(const GLenum type){

    //taken from https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glTexImage2D.xhtml
    std::vector<GLenum> allowed_types;
    allowed_types.push_back(GL_UNSIGNED_BYTE);
    allowed_types.push_back(GL_BYTE);
    allowed_types.push_back(GL_UNSIGNED_SHORT);
    allowed_types.push_back(GL_SHORT);
    allowed_types.push_back(GL_UNSIGNED_INT);
    allowed_types.push_back(GL_INT);
    allowed_types.push_back(GL_FLOAT);
    allowed_types.push_back(GL_HALF_FLOAT);
    allowed_types.push_back(GL_UNSIGNED_BYTE_3_3_2);
    allowed_types.push_back(GL_UNSIGNED_BYTE_2_3_3_REV);
    allowed_types.push_back(GL_UNSIGNED_SHORT_5_6_5);
    allowed_types.push_back(GL_UNSIGNED_SHORT_5_6_5_REV);
    allowed_types.push_back(GL_UNSIGNED_SHORT_4_4_4_4);
    allowed_types.push_back(GL_UNSIGNED_SHORT_4_4_4_4_REV);
    allowed_types.push_back(GL_UNSIGNED_SHORT_5_5_5_1);
    allowed_types.push_back(GL_UNSIGNED_SHORT_1_5_5_5_REV);
    allowed_types.push_back(GL_UNSIGNED_INT_8_8_8_8);
    allowed_types.push_back(GL_UNSIGNED_INT_8_8_8_8_REV);
    allowed_types.push_back(GL_UNSIGNED_INT_10_10_10_2);
    allowed_types.push_back(GL_UNSIGNED_INT_2_10_10_10_REV);

    //check that we are a valid one
    for (size_t i = 0; i < allowed_types.size(); i++) {
        if(type==allowed_types[i]){
            return true; //This is a valid format
        }
    }

    return false;
}

inline bool is_internal_format_valid_for_image_bind(const GLenum internal_format){
    //List is here https://www.khronos.org/opengl/wiki/Image_Load_Store

    std::vector<GLenum> allowed_internal_formats;

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
    allowed_internal_formats.push_back(GL_RGBA32UI);
    allowed_internal_formats.push_back(GL_RGBA16UI);
    allowed_internal_formats.push_back(GL_RGB10_A2UI);
    allowed_internal_formats.push_back(GL_RGBA8UI);
    allowed_internal_formats.push_back(GL_RG32UI);
    allowed_internal_formats.push_back(GL_RG16UI);
    allowed_internal_formats.push_back(GL_RG8UI);
    allowed_internal_formats.push_back(GL_R32UI);
    allowed_internal_formats.push_back(GL_R16UI);
    allowed_internal_formats.push_back(GL_R8UI);
    allowed_internal_formats.push_back(GL_RGBA32I);
    allowed_internal_formats.push_back(GL_RGBA16I);
    allowed_internal_formats.push_back(GL_RGBA8I);
    allowed_internal_formats.push_back(GL_RG32I);
    allowed_internal_formats.push_back(GL_RG16I);
    allowed_internal_formats.push_back(GL_RG8I);
    allowed_internal_formats.push_back(GL_R32I);
    allowed_internal_formats.push_back(GL_R16I);
    allowed_internal_formats.push_back(GL_R8I);


    //check that we are a valid one
    for (size_t i = 0; i < allowed_internal_formats.size(); i++) {
        if(internal_format==allowed_internal_formats[i]){
            return true; //This is a valid format
        }
    }

    return false;
}
