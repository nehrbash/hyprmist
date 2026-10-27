#pragma once
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/Framebuffer.hpp>
#include <GLES3/gl32.h>
#include <array>
#include <stdexcept>
#include "Particles.hpp"

namespace Mist {
struct Cloud {
    Box box{};
    Vec direction{0, -1};
    double progress = 0, elapsed = 0, seed = 0, stamp = 0;
    bool incoming = true, active = false;
    Box domain{};
    SP<Render::IFramebuffer> field, smoke;
    double prepared = -1;
};

class GLState {
    GLint program, vao, drawFB, readFB, activeTexture, texture0, texture1;
    std::array<GLint, 4> viewport;
    std::array<GLboolean, 4> colorMask;
    std::array<GLboolean, 4> enabled;
    static constexpr std::array<GLenum, 4> caps{GL_BLEND, GL_SCISSOR_TEST, GL_STENCIL_TEST, GL_DEPTH_TEST};
  public:
    GLState() {
        glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFB);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFB);
        glGetIntegerv(GL_VIEWPORT, viewport.data());
        glGetBooleanv(GL_COLOR_WRITEMASK, colorMask.data());
        glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
        glActiveTexture(GL_TEXTURE0); glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture0);
        glActiveTexture(GL_TEXTURE1); glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture1);
        for (size_t i = 0; i < caps.size(); ++i) { enabled[i] = glIsEnabled(caps[i]); glDisable(caps[i]); }
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }
    ~GLState() {
        glUseProgram(program); glBindVertexArray(vao);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFB); glBindFramebuffer(GL_READ_FRAMEBUFFER, readFB);
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
        for (size_t i = 0; i < caps.size(); ++i) if (enabled[i]) glEnable(caps[i]); else glDisable(caps[i]);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texture0);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, texture1);
        glActiveTexture(activeTexture);
    }
};

class CloudRenderer {
    GLuint program = 0, vao = 0;
    enum Uniform { Mode, Source, Field, Size, Domain, FieldDomain, Window, Direction, Progress, Elapsed, Seed, Incoming, Count };
    std::array<GLint, Count> uniforms{};
    static GLuint compile(GLenum type, const char* source) {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr); glCompileShader(shader);
        GLint ok = 0; glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            std::array<char, 4096> log{};
            glGetShaderInfoLog(shader, log.size(), nullptr, log.data());
            glDeleteShader(shader);
            throw std::runtime_error(std::string("hyprmist shader: ") + log.data());
        }
        return shader;
    }
    void boxUniform(Uniform u, Box b) { glUniform4f(uniforms[u], b.x, b.y, b.w, b.h); }
    void draw(int mode, SP<Render::ITexture> source, SP<Render::IFramebuffer>& target,
              Vector2D size, Box domain, const Cloud& cloud) {
        GLState saved;
        if (!target) target = g_pHyprRenderer->createFB("hyprmist");
        target->alloc(std::max(1, int(size.x)), std::max(1, int(size.y)), DRM_FORMAT_ABGR8888);
        if (source) target->setImageDescription(source->m_imageDescription);
        target->bind();
        glViewport(0, 0, target->m_size.x, target->m_size.y);
        glUseProgram(program); glBindVertexArray(vao);
        glUniform1i(uniforms[Mode], mode);
        glUniform1i(uniforms[Source], 0); glUniform1i(uniforms[Field], 1);
        glUniform2f(uniforms[Size], target->m_size.x, target->m_size.y);
        boxUniform(Domain, domain); boxUniform(FieldDomain, cloud.domain); boxUniform(Window, cloud.box);
        glUniform2f(uniforms[Direction], cloud.direction.x, cloud.direction.y);
        glUniform1f(uniforms[Progress], cloud.progress); glUniform1f(uniforms[Elapsed], cloud.elapsed);
        glUniform1f(uniforms[Seed], cloud.seed); glUniform1i(uniforms[Incoming], cloud.incoming);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, source ? source->m_texID : 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, mode != 1 && cloud.field ? cloud.field->getTexture()->m_texID : 0);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
  public:
    void init() {
        GLState saved;
        const char* vertex = R"GLSL(#version 320 es
void main() {
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
})GLSL";
        const char* fragment = R"GLSL(#version 320 es
precision highp float;
uniform int mode, incoming;
uniform sampler2D sourceTex, fieldTex;
uniform vec2 targetSize, direction;
uniform vec4 domain, fieldDomain, windowBox;
uniform float progress, elapsed, seed;
out vec4 fragColor;
float hash(vec2 p) {
    vec3 q = fract(vec3(p.xyx) * .1031);
    q += dot(q, q.yzx + 33.33);
    return fract((q.x + q.y) * q.z);
}
float noise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f*f*(3.0-2.0*f);
    return mix(mix(hash(i),hash(i+vec2(1,0)),f.x),mix(hash(i+vec2(0,1)),hash(i+1.0),f.x),f.y);
}
float fbm(vec2 p) {
    float n=0.0, a=.55;
    for(int i=0;i<4;i++) { n+=a*noise(p); p=mat2(1.6,1.2,-1.2,1.6)*p+3.7; a*=.48; }
    return n;
}
void main() {
    vec2 uv=gl_FragCoord.xy/targetSize;
    if(mode==3) { fragColor=texture(sourceTex,uv); return; }
    vec2 world=domain.xy+vec2(uv.x,1.0-uv.y)*domain.zw;
    if(mode!=1) {
        vec2 fuv=(world-fieldDomain.xy)/fieldDomain.zw;
        vec4 field=texture(fieldTex,vec2(fuv.x,1.0-fuv.y));
        if(mode==0) fragColor=texture(sourceTex,uv)*field.r;
        else {
            vec3 color=mix(vec3(.10,.085,.13),vec3(.48,.44,.53),field.b);
            fragColor=vec4(color*field.g,field.g);
        }
        return;
    }
    vec2 rel=(world-windowBox.xy)/max(windowBox.zw,vec2(1));
    float along=dot(rel-.5,direction)/max(dot(abs(direction),vec2(1)),.01)+.5;
    float front=mix(-.35,1.35,progress);
    vec2 p=(rel-.5)*windowBox.zw/150.0 + seed*31.0;
    p-=direction*elapsed*1.6;
    p.y+=elapsed*1.1;
    vec2 curl=vec2(fbm(p+4.7),fbm(p-8.3));
    float density=fbm(p+curl*2.8);
    float q=along+(density-.5)*.30;
    float reveal=smoothstep(q-.10,q+.10,front);
    float mask=incoming!=0 ? reveal : 1.0-reveal;
    vec2 outside=max(abs((rel-.5)*windowBox.zw)-windowBox.zw*.5,vec2(0));
    float edge=1.0-smoothstep(25.0,150.0,length(outside));
    float band=exp(-pow((q-front)/.22,2.0));
    float envelope=pow(max(0.0,sin(progress*3.14159265)),.45);
    float smoke=clamp(band*edge*envelope*(.45+density*.9),0.0,.92);
    fragColor=vec4(mask,smoke,density,1);
})GLSL";
        GLuint vs = compile(GL_VERTEX_SHADER, vertex), fs = compile(GL_FRAGMENT_SHADER, fragment);
        program = glCreateProgram(); glAttachShader(program, vs); glAttachShader(program, fs); glLinkProgram(program);
        glDeleteShader(vs); glDeleteShader(fs);
        GLint ok = 0; glGetProgramiv(program, GL_LINK_STATUS, &ok);
        if (!ok) { release(); throw std::runtime_error("hyprmist shader link failed"); }
        constexpr const char* names[]{"mode","sourceTex","fieldTex","targetSize","domain","fieldDomain","windowBox","direction","progress","elapsed","seed","incoming"};
        for (size_t i=0;i<Count;++i) uniforms[i]=glGetUniformLocation(program,names[i]);
        glGenVertexArrays(1,&vao);
    }
    void release() { if(program) glDeleteProgram(program); if(vao) glDeleteVertexArrays(1,&vao); program=vao=0; }
    void prepare(Cloud& c) {
        if (c.prepared == c.stamp) return;
        c.domain = {c.box.x-160,c.box.y-160,c.box.w+320,c.box.h+320};
        double scale=std::min(.5,1024.0/std::max(c.domain.w,c.domain.h));
        Vector2D size{std::ceil(c.domain.w*scale),std::ceil(c.domain.h*scale)};
        draw(1,nullptr,c.field,size,c.domain,c);
        draw(2,nullptr,c.smoke,size,c.domain,c);
        c.prepared=c.stamp;
    }
    SP<Render::IFramebuffer> mask(SP<Render::IFramebuffer> input, SP<Render::IFramebuffer>& output, Box domain, Cloud& c) {
        prepare(c);
        draw(0,input->getTexture(),output,input->m_size,domain,c);
        return output;
    }
    void copy(SP<Render::IFramebuffer> input, SP<Render::IFramebuffer>& output) {
        draw(3,input->getTexture(),output,input->m_size,{},{});
    }
    bool verifyField() {
        GLState saved;
        Cloud c;
        c.box={0,0,640,480};
        c.direction={1,0};
        auto sample=[&](double progress, bool incoming) {
            c.progress=progress; c.incoming=incoming; c.stamp+=1;
            prepare(c); c.field->bind();
            GLint framebuffer = 0;
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &framebuffer);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
            std::array<uint8_t,4> pixel{};
            glReadPixels(c.field->m_size.x/2,c.field->m_size.y/2,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel.data());
            return pixel;
        };
        auto start=sample(0,true), end=sample(1,true), middle=sample(.5,true);
        auto closingStart=sample(0,false), closingEnd=sample(1,false);
        return start[0]==0 && end[0]==255 && start[1]==0 && end[1]==0 && middle[1]>80
            && closingStart[0]==255 && closingEnd[0]==0;
    }
};
}
