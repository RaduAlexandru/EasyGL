#pragma once
#include <glad/glad.h>

#include <iostream>

namespace gl{
    class Buf{
    public:
        Buf():
            m_buf_id(-1),
            m_buf_storage_initialized(false),
            m_buf_is_inmutable(false),
            m_target(-1),
            m_usage_hints(-1),
            m_size_bytes(-1),
            m_is_cpu_dirty(false),
            m_is_gpu_dirty(false){

            glGenBuffers(1,&m_buf_id);
        }

        Buf(std::string name):
            Buf(){
            m_name=name; //we delegate the constructor to the main one but we cannot have in this intializer list more than that call.
        }

        ~Buf(){
            glDeleteBuffers(1, &m_buf_id);
        }


        void set_target(const GLenum target){
            m_target=target; //can be either GL_ARRAY_BUFFER, GL_SHADER_STORAGE_BUFFER etc...
        }

        void orphan(){
            // sanity_check();

            //orphaning required to use the same usage hints it had before https://www.khronos.org/opengl/wiki/Buffer_Object_Streaming
            if(m_target==-1)  LOG(FATAL) << err("Target not set. Use upload_data or allocate_inmutable first");
            if(m_buf_is_inmutable) LOG(FATAL) << err("Storage is inmutable so it cannot be orphaned. You should make it mutable using upload_data with NULL as data");
            if(m_usage_hints==-1) LOG(FATAL) << err("Usage hints have not been assigned. They will get assign by using upload_data.");
            if(m_size_bytes==-1) LOG(FATAL) << err("Size have not been assigned. It will get assign by using upload_data.");

            glBindBuffer(m_target, m_buf_id);
            glBufferData(m_target, m_size_bytes, NULL, m_usage_hints);
        }

        void upload_data(const GLenum target, const GLsizei size_bytes, const void* data_ptr, const GLenum usage_hints ){
            if(m_buf_is_inmutable) LOG(FATAL) << err("Storage is inmutable so it cannot be orphaned. You should make it mutable using upload_data with NULL as data");

            glBindBuffer(target, m_buf_id);
            glBufferData(target, size_bytes, data_ptr, usage_hints);

            m_target=target;
            m_size_bytes=size_bytes;
            m_usage_hints=usage_hints;
            m_buf_storage_initialized=true;
        }

        //same as above but without specifying the target as we use the one that is already set
        void upload_data(const GLsizei size_bytes, const void* data_ptr, const GLenum usage_hints ){
            if(m_buf_is_inmutable) LOG(FATAL) << err("Storage is inmutable so it cannot be orphaned. You should make it mutable using upload_data with NULL as data");
            if(m_target==-1)  LOG(FATAL) << err("Target not set. Use upload_data or allocate_inmutable first");

            glBindBuffer(m_target, m_buf_id);
            glBufferData(m_target, size_bytes, data_ptr, usage_hints);

            m_size_bytes=size_bytes;
            m_usage_hints=usage_hints;
            m_buf_storage_initialized=true;
        }


        //same as above but without specifying the target nor the usage hints
        void upload_data(const GLsizei size_bytes, const void* data_ptr ){
            if(m_buf_is_inmutable) LOG(FATAL) << err("Storage is inmutable so it cannot be orphaned. You should make it mutable using upload_data with NULL as data");
            if(m_target==-1)  LOG(FATAL) << err("Target not set. Use upload_data or allocate_inmutable first");
            if(m_usage_hints==-1) LOG(FATAL) << err("Usage hints have not been assigned. They will get assign by using upload_data.");

            glBindBuffer(m_target, m_buf_id);
            glBufferData(m_target, size_bytes, data_ptr, m_usage_hints);

            m_size_bytes=size_bytes;
            m_buf_storage_initialized=true;
        }

        void upload_sub_data(const GLenum target, const GLintptr offset, const GLsizei size_bytes, const void* data_ptr){
            if(!m_buf_storage_initialized) LOG(FATAL) << err("Buffer has no storage initialized. Use upload_data, or allocate_inmutable.");

            glBindBuffer(target, m_buf_id);
            glBufferSubData(target, offset, size_bytes, data_ptr);
        }

        //same without target
        void upload_sub_data(const GLintptr offset, const GLsizei size_bytes, const void* data_ptr){
            if(!m_buf_storage_initialized) LOG(FATAL) << err("Buffer has no storage initialized. Use upload_data, or allocate_inmutable.");
            if(m_target==-1)  LOG(FATAL) << err("Target not set. Use upload_data or allocate_inmutable first");

            glBindBuffer(m_target, m_buf_id);
            glBufferSubData(m_target, offset, size_bytes, data_ptr);
        }

        void bind_for_modify(const GLint uniform_location){
            if(m_target==-1)  LOG(FATAL) << err("Target not set. Use upload_data or allocate_inmutable first");
            if(uniform_location==-1)  LOG(WARNING) << err("Uniform location does not exist");

            glBindBufferBase(m_target, uniform_location, m_buf_id);
        }



        //allocate inmutable texture storage
        void allocate_inmutable( const GLenum target,  const GLsizei size_bytes, const void* data_ptr, const GLbitfield flags){


            glBindBuffer(target, m_buf_id);
            glBufferStorage(target, size_bytes, data_ptr, flags);

            m_target=target;
            m_size_bytes=size_bytes;
            m_buf_is_inmutable=true;
            m_buf_storage_initialized=true;
        }

        //allocate inmutable texture storage (ASSUMES TARGET WAS SET BEFORE )
        void allocate_inmutable(const GLsizei size_bytes, const void* data_ptr, const GLbitfield flags){
            if(m_target==-1)  LOG(FATAL) << err("Target not set. Use set_target, upload_data or allocate_inmutable first");

            glBindBuffer(m_target, m_buf_id);
            glBufferStorage(m_target, size_bytes, data_ptr, flags);

            m_size_bytes=size_bytes;
            m_buf_is_inmutable=true;
            m_buf_storage_initialized=true;
        }

        //clear the dat assuming the buffer is composed of floats and only 1 per element
        void clear_to_float(const float val){
            CHECK(m_buf_storage_initialized) << "Buffer storage not initialized. Use allocate_inmutable or upload_data first";

            std::vector<float> clear_color(1, val);
            glClearNamedBufferSubData( m_buf_id, GL_R32F, 0, m_size_bytes, GL_RED, GL_FLOAT, clear_color.data() );
        }


        void bind(){
            if(m_target==-1)  LOG(FATAL) << err("Target not set. Use upload_data or allocate_inmutable first");
            glBindBuffer( m_target, m_buf_id );
        }

        GLenum get_target() const {
            return m_target;
        }

        int get_buf_id() const{
            return m_buf_id;
        }

        bool get_buf_storage_initialized (){
            return m_buf_storage_initialized;
        }

        void set_cpu_dirty(const bool dirty){
            m_is_cpu_dirty=dirty;
        }
        void set_gpu_dirty(const bool dirty){
            m_is_gpu_dirty=dirty;
        }
        bool is_cpu_dirty(){
            return m_is_cpu_dirty;
        }
        bool is_gpu_dirty(){
            return m_is_gpu_dirty;
        }

        void set_width(const int width){
            m_width=width;
        }
        void set_height(const int height){
            m_height=height;
        }
        void set_depth(const int depth){
            m_depth=depth;
        }
        int width() const{ LOG_IF(WARNING,m_width==0) << "Width of the texture is 0"; return m_width; };
        int height() const{ LOG_IF(WARNING,m_height==0) << "Height of the texture is 0";return m_height; };
        int depth() const{ LOG_IF(WARNING,m_depth==0) << "Depth of the texture is 0";return m_depth; };


        //download from gpu to cpu
        void download(void* destination_data_ptr, const int bytes_to_copy){
            if(m_target==-1)  LOG(FATAL) << err("Target not set. Use upload_data or allocate_inmutable first");
            if(m_size_bytes==-1) LOG(FATAL) << err("Size have not been assigned. It will get assign by using upload_data.");

            glBindBuffer(m_target, m_buf_id);
            void* ptr = (void*)glMapBuffer(m_target, GL_READ_ONLY);
            memcpy ( destination_data_ptr, ptr, bytes_to_copy );
            glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        }


    private:
        int m_width;
        int m_height;
        int m_depth;

        std::string err(const std::string msg){
            if(m_name.empty()){
                return msg;
            }else{
                return m_name + ": " + msg;
            }
        }

        std::string m_name;

        GLuint m_buf_id;
        bool m_buf_storage_initialized;
        bool m_buf_is_inmutable;

        GLenum m_target;
        GLenum m_usage_hints;
        GLsizei m_size_bytes;

        //usefult for when you run algorithms on the buffer and we need to sometimes syncronize using sync()
        bool m_is_cpu_dirty; //the data changed on the gpu buffer, we need to do a download
        bool m_is_gpu_dirty; //the data changed on the cpu , we need to do a upload



    };
}
