#include "easy_gl/VertexArrayObject.h"


#include <glad/glad.h>

#include <iostream>

#include "easy_gl/Shader.h"
#include "easy_gl/Buf.h"

#include "easy_gl/UtilsGL.h"


//loguru
#define LOGURU_REPLACE_GLOG 1
#include <loguru.hpp>


namespace gl{

VertexArrayObject::VertexArrayObject(){
    glGenVertexArrays(1,&m_id);
}

VertexArrayObject::VertexArrayObject(std::string name):
    VertexArrayObject(){
    m_name=name; //we delegate the constructor to the main one but we cannot have in this intializer list more than that call.
}

VertexArrayObject::~VertexArrayObject(){
    // LOG(WARNING) << named("Destroying VAO");
    glDeleteVertexArrays(1, &m_id);
}


void VertexArrayObject::set_name(const std::string name){
    m_name=name;
}

void VertexArrayObject::bind() const{
    glBindVertexArray(m_id);
}

void VertexArrayObject::vertex_attribute(const gl::Shader& prog, const std::string& attrib_name, const gl::Buf& buf, const int size) const{
    CHECK(buf.storage_initialized()) << "Cannot set this vertex atribute to the buffer " << buf.name() << " because the buffer has no storage yet. Use buffer.upload_data first";

    this->bind();
    buf.bind();
    GLint attribute_location=prog.get_attrib_location(attrib_name);
    if(attribute_location==-1){
        LOG_IF(WARNING,attribute_location==-1) << named("Attribute location for name ") << attrib_name << " is invalid. Are you sure you are using the attribute in the shader? Maybe you are also binding too many stuff.";
        return;
    }

    // GLenum type_buffer=buf.type();
    // CHECK(type_buffer!=EGL_INVALID) <<"Type of buffer is not valid. Please use buf.set_type() to set a type for it";

    // glVertexAttribPointer(attribute_location, size, type_buffer, GL_FALSE, 0, 0);
    glVertexAttribPointer(attribute_location, size, GL_FLOAT, GL_FALSE, 0, 0); // TODO watch out, we assume that they are all floats but for some reason it still works fine even if we upload ints
    glEnableVertexAttribArray(attribute_location);
}

void VertexArrayObject::indices(const gl::Buf& buf) const{
    GL_C( this->bind() );
    GL_C( buf.bind() );
}



std::string VertexArrayObject::named(const std::string msg) const{
    return m_name.empty()? msg : m_name + ": " + msg;
}


} //namespace gl