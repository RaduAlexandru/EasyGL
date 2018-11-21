#pragma once
#include <glad/glad.h>

#include <iostream>

#include "Shader.h"
#include "Buf.h"

namespace gl{
    class VertexArrayObject{
    public:
        VertexArrayObject(){
            glGenVertexArrays(1,&m_id);
        }

        VertexArrayObject(std::string name):
            VertexArrayObject(){
            m_name=name; //we delegate the constructor to the main one but we cannot have in this intializer list more than that call.
        }

        ~VertexArrayObject(){
            glDeleteVertexArrays(1, &m_id);
        }

        void set_name(const std::string name){
            m_name=name;
        }

        void bind() const{
            glBindVertexArray(m_id);
        }

        void vertex_attribute(const gl::Shader& prog, const std::string& atrib_name, const gl::Buf& buf, const int size) const{
            this->bind();
            buf.bind();
            GLint attribute_location=prog.get_attrib_location(atrib_name);
            glVertexAttribPointer(attribute_location, size, GL_FLOAT, GL_FALSE, 0, 0); //TODO it assumes all buffers are made of floats!!! Maybe change that so we can query from the buffer the data type. Therefore making the buffer into a templated one
            glEnableVertexAttribArray(attribute_location);
        }

        void indices(const gl::Buf& buf) const{
            this->bind();
            buf.bind();
        }


        

    private:
        std::string err(const std::string msg){
            if(m_name.empty()){
                return msg;
            }else{
                return m_name + ": " + msg;
            }
        }

        std::string m_name;

        GLuint m_id;

      

    };
}