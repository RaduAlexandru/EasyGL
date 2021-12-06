#include "easy_gl/Buf.h"

#ifdef EASYPBR_WITH_TORCH
    #include "torch/torch.h"
    #include <cuda_gl_interop.h>
    #include "c10/core/ScalarType.h" //for the function elementSize which return the number of bytes for a certain scalartype of torch
#endif


#include <iostream>
#include <vector>
#include <cstring> //memcpy

//loguru
#define LOGURU_REPLACE_GLOG 1
#include <loguru.hpp>


//use the maximum value of an int as invalid . We don't use negative because we sometimes compare with unsigned int
#define EGL_INVALID 2147483647


namespace gl{

Buf::Buf():
    m_width(0),
    m_height(0),
    m_buf_id(EGL_INVALID),
    m_buf_storage_initialized(false),
    m_buf_is_inmutable(false),
    m_type(EGL_INVALID),
    m_target(EGL_INVALID),
    m_usage_hints(EGL_INVALID),
    m_size_bytes(EGL_INVALID),
    m_is_cpu_dirty(false),
    m_is_gpu_dirty(false),
    m_cuda_transfer_enabled(false)
    {

    glGenBuffers(1,&m_buf_id);
}

Buf::Buf(std::string name):
    Buf(){
    m_name=name; //we delegate the constructor to the main one but we cannot have in this intializer list more than that call.
}

Buf::~Buf(){
    // LOG(WARNING) << named("Destroying buffer");
    #ifdef EASYPBR_WITH_TORCH
        disable_cuda_transfer();
    #endif

    glDeleteBuffers(1, &m_buf_id);
}

//rule of five (make the class non copyable)
// Buf(const Buf& other) = delete; // copy ctor
// Buf& operator=(const Buf& other) = delete; // assignment op
// // Use default move ctors.  You have to declare these, otherwise the class will not have automatically generated move ctors.
// Buf (Buf && other) = default; //move ctor
// Buf & operator=(Buf &&) = default; //move assignment

void Buf::set_name(const std::string name){
    m_name=name;
}

std::string Buf::name() const{
    return m_name;
}

void Buf::set_type(GLenum type){
    m_type=type;
}

void Buf::set_target(const GLenum target){
    m_target=target; //can be either GL_ARRAY_BUFFER, GL_SHADER_STORAGE_BUFFER etc...
}
void Buf::set_target_array_buffer(){
    m_target=GL_ARRAY_BUFFER;
}
void Buf::set_target_element_array_buffer(){
    m_target=GL_ELEMENT_ARRAY_BUFFER;
}


#ifdef EASYPBR_WITH_TORCH
    void Buf::enable_cuda_transfer(){ //enabling cuda transfer has a performance cost for allocating memory of the texture so we leave this as optional
        if(!m_cuda_transfer_enabled){
            m_cuda_transfer_enabled=true;
            register_for_cuda();
        }
    }

    void Buf::disable_cuda_transfer(){
        m_cuda_transfer_enabled=false;
        unregister_cuda(); 
    }

    void Buf::register_for_cuda(){
        unregister_cuda(); 
        if (m_buf_storage_initialized){
            bind();
            cudaGraphicsGLRegisterBuffer(&m_cuda_resource, m_buf_id, cudaGraphicsRegisterFlagsNone);
        }
    }

    void Buf::unregister_cuda(){
            if(m_cuda_resource){
            cudaGraphicsUnregisterResource(m_cuda_resource);
            m_cuda_resource=nullptr;
        }
    }

    bool Buf::is_cuda_transfer_enabled(){
        return m_cuda_transfer_enabled;
    }
#endif


void Buf::orphan(){
    // sanity_check();

    //orphaning required to use the same usage hints it had before https://www.khronos.org/opengl/wiki/Buffer_Object_Streaming
    if(m_target==EGL_INVALID)  LOG(FATAL) << named("Target not set. Use upload_data or allocate_inmutable first");
    if(m_buf_is_inmutable) LOG(FATAL) << named("Storage is inmutable so it cannot be orphaned. You should make it mutable using upload_data with NULL as data");
    if(m_usage_hints==EGL_INVALID) LOG(FATAL) << named("Usage hints have not been assigned. They will get assign by using upload_data.");
    if(m_size_bytes==EGL_INVALID) LOG(FATAL) << named("Size have not been assigned. It will get assign by using upload_data.");

    glBindBuffer(m_target, m_buf_id);

    #ifdef EASYPBR_WITH_TORCH
        if (m_cuda_transfer_enabled){
            unregister_cuda();
        }
    #endif

    glBufferData(m_target, m_size_bytes, NULL, m_usage_hints);

    #ifdef EASYPBR_WITH_TORCH
        if (m_cuda_transfer_enabled){
            register_for_cuda();
        }
    #endif
}

void Buf::allocate_storage(const GLsizei size_bytes, const GLenum usage_hints ){
    if(m_buf_is_inmutable) LOG(FATAL) << named("Storage is inmutable so you cannot use glBufferData. You need to use glBufferStorage");
    if(m_target==EGL_INVALID)  LOG(FATAL) << named("Target not set. Use upload_data or allocate_inmutable first");
    if(size_bytes==0) return;

    glBindBuffer(m_target, m_buf_id);

    #ifdef EASYPBR_WITH_TORCH
        if (m_cuda_transfer_enabled){
            unregister_cuda();
        }
    #endif

    glBufferData(m_target, size_bytes, NULL, usage_hints);

    m_size_bytes=size_bytes;
    m_usage_hints=usage_hints;
    m_buf_storage_initialized=true;

    //update the cuda resource since we have changed the memory of the texture
    #ifdef EASYPBR_WITH_TORCH
        if (m_cuda_transfer_enabled){
            register_for_cuda();
        }
    #endif
}

void Buf::upload_data(const GLenum target, const GLsizei size_bytes, const void* data_ptr, const GLenum usage_hints ){
    if(m_buf_is_inmutable) LOG(FATAL) << named("Storage is inmutable so you cannot use glBufferData. You need to use glBufferStorage");
    if(size_bytes==0) return;

    glBindBuffer(target, m_buf_id);

    #ifdef EASYPBR_WITH_TORCH
        if (m_cuda_transfer_enabled){
            unregister_cuda();
        }
    #endif


    glBufferData(target, size_bytes, data_ptr, usage_hints);

    m_target=target;
    m_size_bytes=size_bytes;
    m_usage_hints=usage_hints;
    m_buf_storage_initialized=true;

    //update the cuda resource since we have changed the memory of the texture
    #ifdef EASYPBR_WITH_TORCH
        if (m_cuda_transfer_enabled){
            register_for_cuda();
        }
    #endif
}

//same as above but without specifying the target as we use the one that is already set
void Buf::upload_data(const GLsizei size_bytes, const void* data_ptr, const GLenum usage_hints ){
    if(m_buf_is_inmutable) LOG(FATAL) << named("Storage is inmutable so you cannot use glBufferData. You need to use glBufferStorage");
    if(m_target==EGL_INVALID)  LOG(FATAL) << named("Target not set. Use upload_data or allocate_inmutable first");
    if(size_bytes==0) return;

    glBindBuffer(m_target, m_buf_id);

    #ifdef EASYPBR_WITH_TORCH
        if (m_cuda_transfer_enabled){
            unregister_cuda();
        }
    #endif

    glBufferData(m_target, size_bytes, data_ptr, usage_hints);

    m_size_bytes=size_bytes;
    m_usage_hints=usage_hints;
    m_buf_storage_initialized=true;

    //update the cuda resource since we have changed the memory of the texture
    #ifdef EASYPBR_WITH_TORCH
        if (m_cuda_transfer_enabled){
            register_for_cuda();
        }
    #endif
}


//same as above but without specifying the target nor the usage hints
void Buf::upload_data(const GLsizei size_bytes, const void* data_ptr ){
    if(m_buf_is_inmutable) LOG(FATAL) << named("Storage is inmutable so you cannot use glBufferData. You need to use glBufferStorage");
    if(m_target==EGL_INVALID)  LOG(FATAL) << named("Target not set. Use upload_data or allocate_inmutable first");
    if(m_usage_hints==EGL_INVALID) LOG(FATAL) << named("Usage hints have not been assigned. They will get assign by using upload_data.");
    if(size_bytes==0) return;

    glBindBuffer(m_target, m_buf_id);

    #ifdef EASYPBR_WITH_TORCH
        if (m_cuda_transfer_enabled){
            unregister_cuda();
        }
    #endif

    glBufferData(m_target, size_bytes, data_ptr, m_usage_hints);

    m_size_bytes=size_bytes;
    m_buf_storage_initialized=true;

    //update the cuda resource since we have changed the memory of the texture
    #ifdef EASYPBR_WITH_TORCH
        if (m_cuda_transfer_enabled){
            register_for_cuda();
        }
    #endif
}

void Buf::upload_sub_data(const GLenum target, const GLintptr offset, const GLsizei size_bytes, const void* data_ptr){
    if(!m_buf_storage_initialized) LOG(FATAL) << named("Buffer has no storage initialized. Use upload_data, or allocate_inmutable.");

    glBindBuffer(target, m_buf_id);
    glBufferSubData(target, offset, size_bytes, data_ptr);
}

//same without target
void Buf::upload_sub_data(const GLintptr offset, const GLsizei size_bytes, const void* data_ptr){
    if(!m_buf_storage_initialized) LOG(FATAL) << named("Buffer has no storage initialized. Use upload_data, or allocate_inmutable.");
    if(m_target==EGL_INVALID)  LOG(FATAL) << named("Target not set. Use upload_data or allocate_inmutable first");

    glBindBuffer(m_target, m_buf_id);
    glBufferSubData(m_target, offset, size_bytes, data_ptr);
}

//same without target and with offset zero
void Buf::upload_sub_data( const GLsizei size_bytes, const void* data_ptr){
    if(!m_buf_storage_initialized) LOG(FATAL) << named("Buffer has no storage initialized. Use upload_data, or allocate_inmutable.");
    if(m_target==EGL_INVALID)  LOG(FATAL) << named("Target not set. Use upload_data or allocate_inmutable first");

    glBindBuffer(m_target, m_buf_id);
    glBufferSubData(m_target, 0, size_bytes, data_ptr);
}

void Buf::bind_for_modify(const GLint uniform_location){
    if(m_target==EGL_INVALID)  LOG(FATAL) << named("Target not set. Use upload_data or allocate_inmutable first");
    if(uniform_location==EGL_INVALID)  LOG(WARNING) << named("Uniform location does not exist");

    glBindBufferBase(m_target, uniform_location, m_buf_id);
}



//allocate inmutable texture storage
void Buf::allocate_inmutable( const GLenum target,  const GLsizei size_bytes, const void* data_ptr, const GLbitfield flags){


    glBindBuffer(target, m_buf_id);

    #ifdef EASYPBR_WITH_TORCH
        if (m_cuda_transfer_enabled){
            unregister_cuda();
        }
    #endif

    glBufferStorage(target, size_bytes, data_ptr, flags);

    m_target=target;
    m_size_bytes=size_bytes;
    m_buf_is_inmutable=true;
    m_buf_storage_initialized=true;

    //update the cuda resource since we have changed the memory of the texture
    #ifdef EASYPBR_WITH_TORCH
        if (m_cuda_transfer_enabled){
            register_for_cuda();
        }
    #endif
}

//allocate inmutable texture storage (ASSUMES TARGET WAS SET BEFORE )
void Buf::allocate_inmutable(const GLsizei size_bytes, const void* data_ptr, const GLbitfield flags){
    if(m_target==EGL_INVALID)  LOG(FATAL) << named("Target not set. Use set_target, upload_data or allocate_inmutable first");

    glBindBuffer(m_target, m_buf_id);

    #ifdef EASYPBR_WITH_TORCH
        if (m_cuda_transfer_enabled){
            unregister_cuda();
        }
    #endif

    glBufferStorage(m_target, size_bytes, data_ptr, flags);

    m_size_bytes=size_bytes;
    m_buf_is_inmutable=true;
    m_buf_storage_initialized=true;

    //update the cuda resource since we have changed the memory of the texture
    #ifdef EASYPBR_WITH_TORCH
        if (m_cuda_transfer_enabled){
            register_for_cuda();
        }
    #endif
}

//clear the dat assuming the buffer is composed of floats and only 1 per element
void Buf::clear_to_float(const float val){
    CHECK(m_buf_storage_initialized) << "Buffer storage not initialized. Use allocate_inmutable or upload_data first";

    std::vector<float> clear_color(1, val);
    glClearNamedBufferSubData( m_buf_id, GL_R32F, 0, m_size_bytes, GL_RED, GL_FLOAT, clear_color.data() );
}


#ifdef EASYPBR_WITH_TORCH
    void Buf::from_tensor(torch::Tensor& tensor){
        CHECK(m_cuda_transfer_enabled) << "You must enable first the cuda transfer with tex.enable_cuda_transfer(). This incurrs a performance cost for memory realocations so try to keep the texture in memory mostly unchanged.";

        //check that the tensor has correct format
        CHECK(tensor.scalar_type()==torch::kFloat32 || tensor.scalar_type()==torch::kUInt8 || tensor.scalar_type()==torch::kInt32 ) << "Tensor should be of type float uint8 or int32. However it is " << tensor.scalar_type();
        CHECK(tensor.device().is_cuda() ) << "tensor should be on the GPU but it is on device " << tensor.device();

        //calculate the nr of bytes of the tensor
        size_t nr_elements_tensor=torch::numel(tensor);
        size_t nr_bytes_tensor=nr_elements_tensor* elementSize( tensor.scalar_type() );


        //if the buffer already has the correct size we just copy into it
        if(m_buf_storage_initialized && nr_bytes_tensor==(size_t)m_size_bytes){
            //nothing to do here

        }else{
            //we allocate a new bufffer with the corresponding size
            allocate_storage(nr_bytes_tensor, GL_DYNAMIC_DRAW);

        }

        CHECK(m_cuda_resource!=nullptr) << "The cuda resource should be something different than null. Something went wrong";

        //bind the buffer and map it to cuda https://developer.download.nvidia.com/GTC/PDF/GTC2012/PresentationPDF/S0267A-GTC2012-Mixing-Graphics-Compute.pdf
        bind();
        cudaGraphicsMapResources(1, &m_cuda_resource, 0);

        //get cuda ptr
        void *buf_data_ptr;
        size_t size_vieweable_by_cuda;
        cudaGraphicsResourceGetMappedPointer(&buf_data_ptr, &size_vieweable_by_cuda, m_cuda_resource);
        CHECK(size_vieweable_by_cuda==nr_bytes_tensor) << "nr_bytes_tensor and size_vieweable_by_cuda should be the same. But nr_bytes_tensor is " << nr_bytes_tensor << " size_vieweable_by_cuda is " << size_vieweable_by_cuda;

        //copy from tensor to tex_data_ptr http://leadsense.ilooktech.com/sdk/docs/page_samplewithopengl.html
        tensor=tensor.contiguous();
        cudaMemcpy(buf_data_ptr, tensor.data_ptr(), nr_bytes_tensor, cudaMemcpyDeviceToDevice);
        cudaDeviceSynchronize(); //we need to syncronize here because the memcopy si asyncronnous ans we dont want to start using the texture until the copy is done


        //unmap
        cudaGraphicsUnmapResources(1, &m_cuda_resource, 0);


        unbind();


    }

    // torch::Tensor Buf::to_tensor(const torch::ScalarType scalar_type_tensor){
    //     // torch::Tensor tensor = torch::zeros({ m_height, m_width, nr_channels_tensor }, torch::dtype(scalar_type_tensor).device(torch::kCUDA, 0) );
    //     int nr_bytes_per_element=elementSize(scalar_type_tensor);
    //     int nr_elements_buf=m_size_bytes/nr_bytes_per_element;
    //     size_t nr_bytes_tensor=nr_elements_buf* elementSize( scalar_type_tensor );


    //     torch::Tensor tensor = torch::zeros({ nr_elements_buf }, torch::dtype(scalar_type_tensor).device(torch::kCUDA, 0) );


    //     //bind the buffer and map it to cuda https://developer.download.nvidia.com/GTC/PDF/GTC2012/PresentationPDF/S0267A-GTC2012-Mixing-Graphics-Compute.pdf
    //     bind();
    //     cudaGraphicsMapResources(1, &m_cuda_resource, 0);

    //     //get cuda ptr
    //     void *buf_data_ptr;
    //     size_t size_vieweable_by_cuda;
    //     cudaGraphicsResourceGetMappedPointer(&buf_data_ptr, &size_vieweable_by_cuda, m_cuda_resource);
    //     CHECK(size_vieweable_by_cuda==nr_bytes_tensor) << "nr_bytes_tensor and size_vieweable_by_cuda should be the same. But nr_bytes_tensor is " << nr_bytes_tensor << " size_vieweable_by_cuda is " << size_vieweable_by_cuda;

    //     //copy from tensor to tex_data_ptr http://leadsense.ilooktech.com/sdk/docs/page_samplewithopengl.html
    //     tensor=tensor.contiguous();
    //     cudaMemcpy(tensor.data_ptr(), buf_data_ptr, nr_bytes_tensor, cudaMemcpyDeviceToDevice);
    //     cudaDeviceSynchronize(); //we need to syncronize here because the memcopy si asyncronnous ans we dont want to start using the texture until the copy is done


    //     //unmap
    //     cudaGraphicsUnmapResources(1, &m_cuda_resource, 0);

    //     return tensor;
    // }


    template <typename T>
    torch::Tensor Buf::to_tensor(){
        // torch::Tensor tensor = torch::zeros({ m_height, m_width, nr_channels_tensor }, torch::dtype(scalar_type_tensor).device(torch::kCUDA, 0) );
        int nr_bytes_per_element=elementSize(   torch::CppTypeToScalarType<T>() );
        int nr_elements_buf=m_size_bytes/nr_bytes_per_element;
        size_t nr_bytes_tensor=nr_elements_buf* elementSize( torch::CppTypeToScalarType<T>() );


        torch::Tensor tensor = torch::zeros({ nr_elements_buf }, torch::dtype( torch::CppTypeToScalarType<T>() ).device(torch::kCUDA, 0) );


        //bind the buffer and map it to cuda https://developer.download.nvidia.com/GTC/PDF/GTC2012/PresentationPDF/S0267A-GTC2012-Mixing-Graphics-Compute.pdf
        bind();
        cudaGraphicsMapResources(1, &m_cuda_resource, 0);

        //get cuda ptr
        void *buf_data_ptr;
        size_t size_vieweable_by_cuda;
        cudaGraphicsResourceGetMappedPointer(&buf_data_ptr, &size_vieweable_by_cuda, m_cuda_resource);
        CHECK(size_vieweable_by_cuda==nr_bytes_tensor) << "nr_bytes_tensor and size_vieweable_by_cuda should be the same. But nr_bytes_tensor is " << nr_bytes_tensor << " size_vieweable_by_cuda is " << size_vieweable_by_cuda;

        //copy from tensor to tex_data_ptr http://leadsense.ilooktech.com/sdk/docs/page_samplewithopengl.html
        tensor=tensor.contiguous();
        cudaMemcpy(tensor.data_ptr(), buf_data_ptr, nr_bytes_tensor, cudaMemcpyDeviceToDevice);
        cudaDeviceSynchronize(); //we need to syncronize here because the memcopy si asyncronnous ans we dont want to start using the texture until the copy is done


        //unmap
        cudaGraphicsUnmapResources(1, &m_cuda_resource, 0);

        return tensor;
    }


#endif


void Buf::bind() const{
    if(m_target==EGL_INVALID)  LOG(FATAL) << named("Target not set. Use upload_data or allocate_inmutable first");
    glBindBuffer( m_target, m_buf_id );
}

void Buf::unbind() const{
    if(m_target==EGL_INVALID)  LOG(FATAL) << named("Target not set. Use upload_data or allocate_inmutable first");
    glBindBuffer( m_target, 0 );
}

GLenum Buf::type() const{
    return m_type;
}

GLenum Buf::target() const {
    return m_target;
}

int Buf::buf_id() const{
    return m_buf_id;
}

bool Buf::storage_initialized () const{
    return m_buf_storage_initialized;
}

void Buf::set_cpu_dirty(const bool dirty){
    m_is_cpu_dirty=dirty;
}
void Buf::set_gpu_dirty(const bool dirty){
    m_is_gpu_dirty=dirty;
}
bool Buf::is_cpu_dirty(){
    return m_is_cpu_dirty;
}
bool Buf::is_gpu_dirty(){
    return m_is_gpu_dirty;
}

void Buf::set_width(const int width){
    m_width=width;
}
void Buf::set_height(const int height){
    m_height=height;
}
void Buf::set_depth(const int depth){
    m_depth=depth;
}
int Buf::width() const{ LOG_IF(WARNING,m_width==0) << "Width of the buffer is 0"; return m_width; };
int Buf::height() const{ LOG_IF(WARNING,m_height==0) << "Height of the buffer is 0";return m_height; };
int Buf::depth() const{ LOG_IF(WARNING,m_depth==0) << "Depth of the buffer is 0";return m_depth; };
int Buf::size_bytes(){
    return m_size_bytes;
}

//download from gpu to cpu
void Buf::download(void* destination_data_ptr, const int bytes_to_copy){
    if(m_target==EGL_INVALID)  LOG(FATAL) << named("Target not set. Use upload_data or allocate_inmutable first");
    if(m_size_bytes==EGL_INVALID) LOG(FATAL) << named("Size have not been assigned. It will get assign by using upload_data.");

    glBindBuffer(m_target, m_buf_id);
    void* ptr = (void*)glMapBuffer(m_target, GL_READ_ONLY);
    memcpy ( destination_data_ptr, ptr, bytes_to_copy );
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
}


// private:
// int m_width;
// int m_height;
// int m_depth;

// std::string named(const std::string msg) const{
//     return m_name.empty()? msg : m_name + ": " + msg;
// }
// std::string m_name;

// GLuint m_buf_id;
// bool m_buf_storage_initialized;
// bool m_buf_is_inmutable;

// GLenum m_type;
// GLenum m_target;
// GLenum m_usage_hints;
// GLsizei m_size_bytes;

// //usefult for when you run algorithms on the buffer and we need to sometimes syncronize using sync()
// bool m_is_cpu_dirty; //the data changed on the gpu buffer, we need to do a download
// bool m_is_gpu_dirty; //the data changed on the cpu , we need to do a upload


// //if we allocate a new storage for the texture, we need to update the cuda_resource
// bool m_cuda_transfer_enabled;
// // #ifdef EASYPBR_WITH_TORCH
//     struct cudaGraphicsResource *m_cuda_resource=nullptr;
// // #endif


// Here is the explicit instanciation
#ifdef EASYPBR_WITH_TORCH
    template torch::Tensor Buf::to_tensor<int>();
    template torch::Tensor Buf::to_tensor<float>();
#endif




} //namespace gl

