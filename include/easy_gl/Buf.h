#pragma once
#include <glad/glad.h>

// DO NOT USE A IFDEF because other C++ libs may include this Viewer.h without the compile definitions and therefore the Viewer.h that was used to compile easypbr and the one included will be different leading to issues
// #ifdef EASYPBR_WITH_TORCH
    // #include "torch/torch.h"
    // #include <cuda_gl_interop.h>
    // #include "c10/core/ScalarType.h" //for the function elementSize which return the number of bytes for a certain scalartype of torch
// #endif


#include <iostream>
#include <vector>
#include <cstring> //memcpy

//forward declare
struct cudaGraphicsResource;
namespace at{
    class Tensor;
}
namespace at{
    // class ScalarType;
}
namespace torch{
    // class ScalarType;
}
namespace c10{
    // class ScalarType;
}


//use the maximum value of an int as invalid . We don't use negative because we sometimes compare with unsigned int
#define EGL_INVALID 2147483647

namespace gl{
    class Buf{
    public:
        Buf();
        Buf(std::string name);
        ~Buf();

        //rule of five (make the class non copyable)
        Buf(const Buf& other) = delete; // copy ctor
        Buf& operator=(const Buf& other) = delete; // assignment op
        // Use default move ctors.  You have to declare these, otherwise the class will not have automatically generated move ctors.
        Buf (Buf && other) = default; //move ctor
        Buf & operator=(Buf &&) = default; //move assignment


        void set_name(const std::string name);
        std::string name() const;
        void set_type(GLenum type);
        void set_target(const GLenum target);
        void set_target_array_buffer();
        void set_target_element_array_buffer();


        // #ifdef EASYPBR_WITH_TORCH
        void enable_cuda_transfer();
        void disable_cuda_transfer();
        void register_for_cuda();
        void unregister_cuda();
        bool is_cuda_transfer_enabled();
        // #endif


        void orphan();
        void allocate_storage(const GLsizei size_bytes, const GLenum usage_hints );
        void upload_data(const GLenum target, const GLsizei size_bytes, const void* data_ptr, const GLenum usage_hints );
        //same as above but without specifying the target as we use the one that is already set
        void upload_data(const GLsizei size_bytes, const void* data_ptr, const GLenum usage_hints );
        //same as above but without specifying the target nor the usage hints
        void upload_data(const GLsizei size_bytes, const void* data_ptr );
        void upload_sub_data(const GLenum target, const GLintptr offset, const GLsizei size_bytes, const void* data_ptr);
        //same without target
        void upload_sub_data(const GLintptr offset, const GLsizei size_bytes, const void* data_ptr);
        //same without target and with offset zero
        void upload_sub_data( const GLsizei size_bytes, const void* data_ptr);
        void bind_for_modify(const GLint uniform_location);
        //allocate inmutable texture storage
        void allocate_inmutable( const GLenum target,  const GLsizei size_bytes, const void* data_ptr, const GLbitfield flags);
        //allocate inmutable texture storage (ASSUMES TARGET WAS SET BEFORE )
        void allocate_inmutable(const GLsizei size_bytes, const void* data_ptr, const GLbitfield flags);

        //clear the dat assuming the buffer is composed of floats and only 1 per element
        void clear_to_float(const float val);


        // #ifdef EASYPBR_WITH_TORCH
        void from_tensor(at::Tensor& tensor);
        // torch::Tensor to_tensor(const torch::ScalarType scalar_type_tensor);
        template<typename T>
        at::Tensor to_tensor();
        // #endif


        void bind() const;
        void unbind() const;
        GLenum type() const;
        GLenum target() const;
        int buf_id() const;
        bool storage_initialized () const;
        void set_cpu_dirty(const bool dirty);
        void set_gpu_dirty(const bool dirty);
        bool is_cpu_dirty();
        bool is_gpu_dirty();
        void set_width(const int width);
        void set_height(const int height);
        void set_depth(const int depth);
        int width() const;
        int height() const;
        int depth() const;
        int size_bytes();
        //download from gpu to cpu
        void download(void* destination_data_ptr, const int bytes_to_copy);


    private:
        int m_width;
        int m_height;
        int m_depth;

        std::string named(const std::string msg) const{
            return m_name.empty()? msg : m_name + ": " + msg;
        }
        std::string m_name;

        GLuint m_buf_id;
        bool m_buf_storage_initialized;
        bool m_buf_is_inmutable;

        GLenum m_type;
        GLenum m_target;
        GLenum m_usage_hints;
        GLsizei m_size_bytes;

        //usefult for when you run algorithms on the buffer and we need to sometimes syncronize using sync()
        bool m_is_cpu_dirty; //the data changed on the gpu buffer, we need to do a download
        bool m_is_gpu_dirty; //the data changed on the cpu , we need to do a upload


        //if we allocate a new storage for the texture, we need to update the cuda_resource
        bool m_cuda_transfer_enabled;
        struct cudaGraphicsResource *m_cuda_resource=nullptr;



    };
}
