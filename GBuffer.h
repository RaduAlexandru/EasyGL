#pragma once
#include <glad/glad.h>

#include <Texture2D.h>


#define MAX_TEXTURES 8 


//use the maximum value of an int as invalid . We don't use negative because we sometimes compare with unsigned int
#define EGL_INVALID 2147483647  

namespace gl{
    class GBuffer{
    public:
        GBuffer():
            m_width(0),
            m_height(0),
            m_fbo_id(EGL_INVALID),
            m_depth_tex("depth_gbuffer") // add a dummy name just to have in case we throw any error
            {
            glGenFramebuffers(1,&m_fbo_id);


            glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS , &m_max_color_attachments);
            m_textures.reserve(m_max_color_attachments); // we define a maximum amount of textures and preallocate the vector of textures because I do not want it to dinammy resize as it will cause textures to be deconstructed and constructed again which will cause the gl resources to be deleted

        }

        GBuffer(std::string name):
            GBuffer(){
            m_name=name; //we delegate the constructor to the main one but we cannot have in this intializer list more than that call.
        }

        ~GBuffer(){
            LOG(WARNING) << named("Destroying gbuffer");
            glDeleteFramebuffers(1, &m_fbo_id);
        }

        // rule of five (make the class non copyable)   
        GBuffer(const GBuffer& other) = delete; // copy ctor
        GBuffer& operator=(const GBuffer& other) = delete; // assignment op
        // Use default move ctors.  You have to declare these, otherwise the class will not have automatically generated move ctors.
        GBuffer (GBuffer && other) = default; //move ctor
        GBuffer & operator=(GBuffer &&) = default; //move assignment


        void add_texture(const std::string name, GLint internal_format, GLenum format, GLenum type){
            LOG_IF(FATAL, (int)m_textures.size()>= m_max_color_attachments-1  ) << named( name + " could not be added. Added to many textures. This will cause the vector to be dinamically resized and therefore move and destruct some of the textures. Please increase the MAX_TEXTURES #define inside the GBuffer class");

            m_textures.emplace_back(name); 

            if(m_width!=0 && m_height!=0) { //we already used set_size() so we have some size for the whole gbuffer so we can already allocate using a certain size
                m_textures.back().allocate_tex_storage(internal_format, format, type, m_width, m_height );
            }else{
                m_textures.back().allocate_tex_storage(internal_format, format, type, 0, 0);
            }

            //add this new texture as an attachment to the framebuffer 
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo_id);
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + m_textures.size()-1, GL_TEXTURE_2D, m_textures.back().get_tex_id(), 0);
            m_texname2attachment[name]=m_textures.size()-1;

            // restore default FBO
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

        }

        void add_depth(const std::string name){
            m_depth_tex.set_name(name);
            if(m_width!=0 && m_height!=0){
                m_depth_tex.allocate_tex_storage(GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_FLOAT, m_width, m_height);
            }else{
                m_depth_tex.allocate_tex_storage(GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_FLOAT,0,0);
            }


            //add this new texture as an attachment to the framebuffer 
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo_id);
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depth_tex.get_tex_id(), 0);

            // restore default FBO
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

        }

        void sanity_check(){
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo_id);
            GLuint status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
            if (status != GL_FRAMEBUFFER_COMPLETE){
                LOG(FATAL) << named("Framebufer is not complete.");
                return;
            }
        }

        void set_size(const int w, const int h){
            m_width=w;
            m_height=h;

            //resize the sizes of all texures that don't have the same size 
            for(size_t i=0; i<m_textures.size(); i++){
                if(m_textures[i].width()!=w || m_textures[i].height()!=h ){
                    m_textures[i].resize(w,h);
                }    
            }
        
            //resize also the depth
            if( (m_depth_tex.width()!=w || m_depth_tex.height()!=h) && m_depth_tex.get_tex_storage_initialized() ){ 
                m_depth_tex.resize(w,h);
            }

        }

        void bind(){
            glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_id);
        }

        void bind_for_draw(){
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo_id);
        }
        
        void bind_for_read(){
            glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo_id);
        }

        void clear(){
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo_id);
            glClearColor(0.0, 0.0, 0.0, 0.0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }


        int get_fbo_id() const{
            return m_fbo_id;
        }

        int width() const{ LOG_IF(WARNING,m_width==0) << named("Width of the gbuffer is 0"); return m_width; };
        int height() const{ LOG_IF(WARNING,m_height==0) << named("Height of the gbuffer is 0");return m_height; };

        int attachment_nr(const std::string tex_name) const{
            auto it = m_texname2attachment.find(tex_name);
            if(it != std::end(m_texname2attachment)){
                //found 
                return it->second;
            }else{
                LOG(WARNING) << named("Texture with name: "+tex_name + " is not added to this gbuffer");
            }
        }

        Texture2D& tex_with_name(const std::string name){
            for(size_t i=0; i<m_textures.size(); i++){
                if(m_textures[i].name()==name){
                    return m_textures[i];
                }
            }
            if(m_depth_tex.name()==name){
                return m_depth_tex;
            }
            LOG(WARNING) << named("No texture with name: " + name);
        }

    private:
        int m_width;
        int m_height;

        std::string named(const std::string msg) const{
            return m_name.empty()? msg : m_name + ": " + msg; 
        }
        std::string m_name;

        GLuint m_fbo_id;
        std::vector<gl::Texture2D> m_textures; //we don't use a vector beceuase as its size increases it might need to copy elements around and I dont want to implement a move constructor in the Texture. Also the copy constructor is deleted
        gl::Texture2D m_depth_tex;

        std::unordered_map<std::string, int > m_texname2attachment; //maps from each texture name to the corresponding GL_COLOR_ATTACHMENT in the framebuffer. Useful when binding the framebuffer to be written to

        int m_max_color_attachments;

    };
}
