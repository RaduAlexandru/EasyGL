#pragma once
#include <glad/glad.h>

#include <iostream>

#include "Shader.h"
#include "Buf.h"

namespace gl{
    class VertexArrayObject{
    public:
        VertexArrayObject();
        VertexArrayObject(std::string name);
        ~VertexArrayObject();

        //rule of five (make the class non copyable)
        VertexArrayObject(const VertexArrayObject& other) = delete; // copy ctor
        VertexArrayObject& operator=(const VertexArrayObject& other) = delete; // assignment op
        // Use default move ctors.  You have to declare these, otherwise the class will not have automatically generated move ctors.
        VertexArrayObject (VertexArrayObject && other) = default; //move ctor
        VertexArrayObject & operator=(VertexArrayObject &&) = default; //move assignment


        void set_name(const std::string name);
        void bind() const;

        void vertex_attribute(const gl::Shader& prog, const std::string& attrib_name, const gl::Buf& buf, const int size) const;
        void indices(const gl::Buf& buf) const;



    private:
        std::string named(const std::string msg) const;
        std::string m_name;

        GLuint m_id;



    };
}
