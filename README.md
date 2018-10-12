# EasyGL

EasyGL is a header-only library which allows a high level, object orientated abstraction of most commmon OpenGL functionalities without sacrificing performance.

### Features
 - Shader compilation and error checking.
 - Easy binding of textures or images for usage with samplers or with image loade/store respectively.
 - Texture uploading and downloading to GPU using PBO.
 - Integration with OpenCV and easy conversion between GL texture and CV Mat.

### Usage
Simply add the header files and include them in your project

```c++
#include <iostream>
#include <opencv2/opencv.hpp>
#include "Texture2D.h"
#include "Texture2DArray.h"
#include "Texture3D.h"
#include "Buf.h"
#include "Shader.h"

int main()
{
    cv::Mat image;
    cv_rgb = cv::imread("../test/cat.bmp", 0);
    cv::Mat cv_rgba; //It's more efficient to deal with images with 1,2 or 4 channels instead of 3
    create_alpha_mat(cv_rgb, cv_rgba);

    //Texture creation
    gl::Texture2D rgba_tex;
    rgba_tex.set_wrap_mode(GL_CLAMP_TO_BORDER);
    rgba_tex.set_filter_mode(GL_LINEAR);
    rgba_tex.upload_from_cv_mat(cv_rgba);

    //shader compilation
    gl::Shader blurx_shader.compile("./shaders/blurx_compute.glsl");

    //Allocate output texture for the shader
    gl::Texture2D blurred_x_tex;
    if(!blurred_x_tex.get_tex_storage_initialized()){
        blurred_x_tex.allocate_tex_storage_inmutable(GL_RGBA32F, rgba_tex.width(), rgba_tex.height() );
    }

    //Use the shade to blur in the x direction
    int radius=5;
    blurx_shader.use();
    blurx_shader.uniform_int(radius,"radius");
    blurx_shader.bind_texture(rgba_tex,"in_img");
    blurx_shader.bind_image(blurred_x_tex, GL_WRITE_ONLY, "blurred_x_tex");
    blurx_shader.dispatch(rgba_tex.width(), rgba_tex.height(), 16 , 16);

    //Download the texture into a cv mat and view it
    cv::Mat blurred_mat=blurred_x_tex.download_to_cv_mat();
    cv::namedWindow("Display Image", cv::WINDOW_AUTOSIZE );
    cv::imshow("Display Image", blurred_mat);
    cv::waitKey(0);
}

```
