// text_renderer_gl.h - OpenGL text rendering (header-only declarations)
#ifndef TEXT_RENDERER_GL_H
#define TEXT_RENDERER_GL_H

#include "text_renderer.h"
#include <vector>

// Forward declare OpenGL types
typedef unsigned int GLuint;
typedef int GLsizei;

// Simple bitmap font renderer for OpenGL
// Requires OpenGL 3.3+ (implementation in text_renderer_gl.cpp)
class GLTextRenderer : public ITextRenderer {
public:
    GLTextRenderer();
    ~GLTextRenderer() override;
    
    bool initialize() override;
    void shutdown() override;
    void beginText(int screenWidth, int screenHeight) override;
    void renderText(const char* text, TextPosition position, TextColor color, float scale = 1.0f) override;
    void endText() override;
    
private:
    GLuint m_vao;
    GLuint m_vbo;
    GLuint m_shader;
    GLuint m_fontTexture;
    int m_screenWidth;
    int m_screenHeight;
    std::vector<float> m_vertices;
    
    void addVertex(float x, float y, float u, float v, const TextColor& color);
    void createBitmapFont();
};

#endif // TEXT_RENDERER_GL_H
