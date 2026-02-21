// text_renderer_gl.cpp - OpenGL text rendering implementation
#include "text_renderer_gl.h"
#include <glad/glad.h>
#include <cstring>

GLTextRenderer::GLTextRenderer() 
    : m_vao(0)
    , m_vbo(0)
    , m_shader(0)
    , m_fontTexture(0)
    , m_screenWidth(800)
    , m_screenHeight(600)
{}

GLTextRenderer::~GLTextRenderer() {
    shutdown();
}

bool GLTextRenderer::initialize() {
    printf("GLTextRenderer::initialize() - starting...\n");
    
    // Create simple shader for text rendering
    const char* vertexShader = R"(
        #version 330 core
        layout(location = 0) in vec2 aPos;
        layout(location = 1) in vec2 aTexCoord;
        layout(location = 2) in vec4 aColor;
        
        out vec2 vTexCoord;
        out vec4 vColor;
        
        void main() {
            gl_Position = vec4(aPos, 0.0, 1.0);
            vTexCoord = aTexCoord;
            vColor = aColor;
        }
    )";
    
    const char* fragmentShader = R"(
        #version 330 core
        in vec2 vTexCoord;
        in vec4 vColor;
        out vec4 FragColor;
        
        uniform sampler2D uFontTexture;
        
        void main() {
            float alpha = texture(uFontTexture, vTexCoord).r;
            FragColor = vec4(vColor.rgb, vColor.a * alpha);
        }
    )";
    
    printf("GLTextRenderer::initialize() - compiling shaders...\n");
    
    // Compile shaders
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertexShader, nullptr);
    glCompileShader(vs);
    
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragmentShader, nullptr);
    glCompileShader(fs);
    
    m_shader = glCreateProgram();
    glAttachShader(m_shader, vs);
    glAttachShader(m_shader, fs);
    glLinkProgram(m_shader);
    
    glDeleteShader(vs);
    glDeleteShader(fs);
    
    printf("GLTextRenderer::initialize() - shader program: %u\n", m_shader);
    
    // Create VAO/VBO for text quads
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    
    printf("GLTextRenderer::initialize() - VAO: %u, VBO: %u\n", m_vao, m_vbo);
    
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    
    // Vertex format: pos(2) + texcoord(2) + color(4) = 8 floats
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(4 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    glBindVertexArray(0);
    
    // Create simple bitmap font texture
    createBitmapFont();
    
    printf("GLTextRenderer::initialize() - font texture: %u\n", m_fontTexture);
    printf("GLTextRenderer::initialize() - SUCCESS\n");
    
    return true;
}

void GLTextRenderer::shutdown() {
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_shader) glDeleteProgram(m_shader);
    if (m_fontTexture) glDeleteTextures(1, &m_fontTexture);
    
    m_vao = m_vbo = m_shader = m_fontTexture = 0;
}

void GLTextRenderer::beginText(int screenWidth, int screenHeight) {
    m_screenWidth = screenWidth;
    m_screenHeight = screenHeight;
    m_vertices.clear();
    printf("GLTextRenderer::beginText() - screen: %dx%d\n", screenWidth, screenHeight);
}

void GLTextRenderer::renderText(const char* text, TextPosition position, TextColor color, float scale) {
    if (!text) return;
    
    printf("GLTextRenderer::renderText() - text: '%s', pos: (%.2f, %.2f)\n", text, position.x, position.y);
    
    float charWidth = 12.0f * scale;
    float charHeight = 16.0f * scale;
    
    // Convert normalized position to NDC (-1 to 1)
    float startX = position.x * 2.0f - 1.0f;
    float startY = 1.0f - position.y * 2.0f;  // Flip Y (top = 1)
    
    // Convert pixel size to NDC
    float ndcCharWidth = (charWidth / m_screenWidth) * 2.0f;
    float ndcCharHeight = (charHeight / m_screenHeight) * 2.0f;
    
    float x = startX;
    float y = startY;
    
    for (const char* c = text; *c; c++) {
        if (*c == '\n') {
            x = startX;
            y -= ndcCharHeight;
            continue;
        }
        
        unsigned char ch = (unsigned char)*c;
        
        // Calculate texture coordinates for this character
        // Our font texture is 16x16 grid of characters
        int charX = ch % 16;
        int charY = ch / 16;
        
        float u0 = charX / 16.0f;
        float v0 = charY / 16.0f;
        float u1 = (charX + 1) / 16.0f;
        float v1 = (charY + 1) / 16.0f;
        
        // Create quad for this character (2 triangles = 6 vertices)
        addVertex(x, y, u0, v0, color);
        addVertex(x + ndcCharWidth, y, u1, v0, color);
        addVertex(x, y - ndcCharHeight, u0, v1, color);
        
        addVertex(x + ndcCharWidth, y, u1, v0, color);
        addVertex(x + ndcCharWidth, y - ndcCharHeight, u1, v1, color);
        addVertex(x, y - ndcCharHeight, u0, v1, color);
        
        x += ndcCharWidth;
    }
}

void GLTextRenderer::endText() {
    if (m_vertices.empty()) {
        printf("GLTextRenderer::endText() - no vertices to render\n");
        return;
    }
    
    printf("GLTextRenderer::endText() - rendering %zu vertices (%zu triangles)\n", 
           m_vertices.size() / 8, (m_vertices.size() / 8) / 3);
    
    // Upload vertices
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(float), 
                 m_vertices.data(), GL_DYNAMIC_DRAW);
    
    // Render text
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    
    glUseProgram(m_shader);
    glBindVertexArray(m_vao);
    glBindTexture(GL_TEXTURE_2D, m_fontTexture);
    
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(m_vertices.size() / 8));
    
    glBindVertexArray(0);
    glUseProgram(0);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    
    printf("GLTextRenderer::endText() - done\n");
}

void GLTextRenderer::addVertex(float x, float y, float u, float v, const TextColor& color) {
    m_vertices.push_back(x);
    m_vertices.push_back(y);
    m_vertices.push_back(u);
    m_vertices.push_back(v);
    m_vertices.push_back(color.r);
    m_vertices.push_back(color.g);
    m_vertices.push_back(color.b);
    m_vertices.push_back(color.a);
}

void GLTextRenderer::createBitmapFont() {
    // Create a simple 16x16 grid of ASCII characters (256x256 texture)
    // Each character is 16x16 pixels with a 5x7 font inside
    const int texWidth = 256;
    const int texHeight = 256;
    unsigned char* fontData = new unsigned char[texWidth * texHeight];
    
    // Initialize to zero (transparent)
    memset(fontData, 0, texWidth * texHeight);
    
    // Simple 5x7 bitmap font patterns for common characters
    // Each character is defined as 7 rows of 5 bits
    const unsigned char font5x7[][7] = {
        // Space (32)
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        // ! (33)
        {0x04, 0x04, 0x04, 0x04, 0x00, 0x04, 0x00},
        // " (34)
        {0x0A, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00},
        // # to / (35-47) - simplified
        {0x0A, 0x1F, 0x0A, 0x1F, 0x0A, 0x00, 0x00}, // #
        {0x0E, 0x14, 0x0E, 0x05, 0x0E, 0x00, 0x00}, // $
        {0x18, 0x19, 0x02, 0x04, 0x13, 0x03, 0x00}, // %
        {0x08, 0x14, 0x08, 0x15, 0x0A, 0x00, 0x00}, // &
        {0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00}, // '
        {0x02, 0x04, 0x04, 0x04, 0x02, 0x00, 0x00}, // (
        {0x08, 0x04, 0x04, 0x04, 0x08, 0x00, 0x00}, // )
        {0x00, 0x0A, 0x04, 0x0A, 0x00, 0x00, 0x00}, // *
        {0x00, 0x04, 0x0E, 0x04, 0x00, 0x00, 0x00}, // +
        {0x00, 0x00, 0x00, 0x04, 0x08, 0x00, 0x00}, // ,
        {0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00}, // -
        {0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00}, // .
        {0x00, 0x01, 0x02, 0x04, 0x08, 0x00, 0x00}, // /
        // 0-9 (48-57)
        {0x0E, 0x11, 0x11, 0x11, 0x0E, 0x00, 0x00}, // 0
        {0x04, 0x0C, 0x04, 0x04, 0x0E, 0x00, 0x00}, // 1
        {0x0E, 0x11, 0x02, 0x04, 0x1F, 0x00, 0x00}, // 2
        {0x0E, 0x11, 0x06, 0x11, 0x0E, 0x00, 0x00}, // 3
        {0x02, 0x06, 0x0A, 0x1F, 0x02, 0x00, 0x00}, // 4
        {0x1F, 0x10, 0x1E, 0x01, 0x1E, 0x00, 0x00}, // 5
        {0x06, 0x08, 0x1E, 0x11, 0x0E, 0x00, 0x00}, // 6
        {0x1F, 0x01, 0x02, 0x04, 0x08, 0x00, 0x00}, // 7
        {0x0E, 0x11, 0x0E, 0x11, 0x0E, 0x00, 0x00}, // 8
        {0x0E, 0x11, 0x0F, 0x01, 0x0E, 0x00, 0x00}, // 9
        // : to @ (58-64)
        {0x00, 0x04, 0x00, 0x04, 0x00, 0x00, 0x00}, // :
        {0x00, 0x04, 0x00, 0x04, 0x08, 0x00, 0x00}, // ;
        {0x02, 0x04, 0x08, 0x04, 0x02, 0x00, 0x00}, // <
        {0x00, 0x0E, 0x00, 0x0E, 0x00, 0x00, 0x00}, // =
        {0x08, 0x04, 0x02, 0x04, 0x08, 0x00, 0x00}, // >
        {0x0E, 0x11, 0x02, 0x04, 0x00, 0x04, 0x00}, // ?
        {0x0E, 0x11, 0x17, 0x10, 0x0E, 0x00, 0x00}, // @
        // A-Z (65-90)
        {0x0E, 0x11, 0x1F, 0x11, 0x11, 0x00, 0x00}, // A
        {0x1E, 0x11, 0x1E, 0x11, 0x1E, 0x00, 0x00}, // B
        {0x0E, 0x11, 0x10, 0x11, 0x0E, 0x00, 0x00}, // C
        {0x1E, 0x11, 0x11, 0x11, 0x1E, 0x00, 0x00}, // D
        {0x1F, 0x10, 0x1E, 0x10, 0x1F, 0x00, 0x00}, // E
        {0x1F, 0x10, 0x1E, 0x10, 0x10, 0x00, 0x00}, // F
        {0x0E, 0x10, 0x17, 0x11, 0x0E, 0x00, 0x00}, // G
        {0x11, 0x11, 0x1F, 0x11, 0x11, 0x00, 0x00}, // H
        {0x0E, 0x04, 0x04, 0x04, 0x0E, 0x00, 0x00}, // I
        {0x07, 0x02, 0x02, 0x12, 0x0C, 0x00, 0x00}, // J
        {0x11, 0x12, 0x1C, 0x12, 0x11, 0x00, 0x00}, // K
        {0x10, 0x10, 0x10, 0x10, 0x1F, 0x00, 0x00}, // L
        {0x11, 0x1B, 0x15, 0x11, 0x11, 0x00, 0x00}, // M
        {0x11, 0x19, 0x15, 0x13, 0x11, 0x00, 0x00}, // N
        {0x0E, 0x11, 0x11, 0x11, 0x0E, 0x00, 0x00}, // O
        {0x1E, 0x11, 0x1E, 0x10, 0x10, 0x00, 0x00}, // P
        {0x0E, 0x11, 0x11, 0x15, 0x0E, 0x01, 0x00}, // Q
        {0x1E, 0x11, 0x1E, 0x12, 0x11, 0x00, 0x00}, // R
        {0x0E, 0x10, 0x0E, 0x01, 0x0E, 0x00, 0x00}, // S
        {0x1F, 0x04, 0x04, 0x04, 0x04, 0x00, 0x00}, // T
        {0x11, 0x11, 0x11, 0x11, 0x0E, 0x00, 0x00}, // U
        {0x11, 0x11, 0x11, 0x0A, 0x04, 0x00, 0x00}, // V
        {0x11, 0x11, 0x15, 0x1B, 0x11, 0x00, 0x00}, // W
        {0x11, 0x0A, 0x04, 0x0A, 0x11, 0x00, 0x00}, // X
        {0x11, 0x0A, 0x04, 0x04, 0x04, 0x00, 0x00}, // Y
        {0x1F, 0x02, 0x04, 0x08, 0x1F, 0x00, 0x00}, // Z
    };
    
    // Draw characters into texture with bold/thick rendering
    for (int ascii = 32; ascii < 91; ascii++) {  // Space to Z
        int charX = (ascii % 16) * 16;
        int charY = (ascii / 16) * 16;
        
        const unsigned char* pattern = font5x7[ascii - 32];
        
        // Draw 5x7 pattern centered in 16x16 cell
        // Make it bolder by drawing 2x2 pixel blocks
        for (int row = 0; row < 7; row++) {
            unsigned char rowData = pattern[row];
            for (int col = 0; col < 5; col++) {
                if (rowData & (1 << (4 - col))) {
                    // Draw a 2x2 block for each bit (makes it bolder)
                    for (int dy = 0; dy < 2; dy++) {
                        for (int dx = 0; dx < 2; dx++) {
                            int px = charX + col * 2 + dx + 2;  // 2 pixels per column
                            int py = charY + row * 2 + dy + 1;  // 2 pixels per row
                            if (px < charX + 16 && py < charY + 16) {
                                fontData[py * texWidth + px] = 255;
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Copy lowercase from uppercase (simple approach)
    for (int ascii = 97; ascii < 123; ascii++) {  // a-z
        int srcChar = ascii - 32;  // Corresponding uppercase
        int srcX = (srcChar % 16) * 16;
        int srcY = (srcChar / 16) * 16;
        int dstX = (ascii % 16) * 16;
        int dstY = (ascii / 16) * 16;
        
        for (int y = 0; y < 16; y++) {
            for (int x = 0; x < 16; x++) {
                fontData[(dstY + y) * texWidth + (dstX + x)] = 
                    fontData[(srcY + y) * texWidth + (srcX + x)];
            }
        }
    }
    
    // Create OpenGL texture
    glGenTextures(1, &m_fontTexture);
    glBindTexture(GL_TEXTURE_2D, m_fontTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, texWidth, texHeight, 0, 
                 GL_RED, GL_UNSIGNED_BYTE, fontData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    delete[] fontData;
}
