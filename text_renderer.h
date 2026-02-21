// text_renderer.h - Simple text rendering for OSD
#ifndef TEXT_RENDERER_H
#define TEXT_RENDERER_H

#include <string>
#include <vector>
#include <cstdint>

// Simple 2D position for text rendering
struct TextPosition {
    float x;  // Screen X position (0-1, left to right)
    float y;  // Screen Y position (0-1, top to bottom)
};

// Text color
struct TextColor {
    float r, g, b, a;
    
    TextColor() : r(1.0f), g(1.0f), b(1.0f), a(1.0f) {}
    TextColor(float _r, float _g, float _b, float _a = 1.0f) 
        : r(_r), g(_g), b(_b), a(_a) {}
    
    static TextColor White() { return TextColor(1.0f, 1.0f, 1.0f, 1.0f); }
    static TextColor Green() { return TextColor(0.0f, 1.0f, 0.0f, 1.0f); }
    static TextColor Yellow() { return TextColor(1.0f, 1.0f, 0.0f, 1.0f); }
    static TextColor Red() { return TextColor(1.0f, 0.0f, 0.0f, 1.0f); }
    static TextColor Cyan() { return TextColor(0.0f, 1.0f, 1.0f, 1.0f); }
};

// Text rendering interface
class ITextRenderer {
public:
    virtual ~ITextRenderer() = default;
    
    // Initialize text rendering system
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    
    // Begin text rendering pass (call before rendering any text)
    virtual void beginText(int screenWidth, int screenHeight) = 0;
    
    // Render a single line of text
    // position.x, position.y are in normalized coordinates (0-1)
    virtual void renderText(const char* text, TextPosition position, TextColor color, float scale = 1.0f) = 0;
    
    // End text rendering pass
    virtual void endText() = 0;
};

#endif // TEXT_RENDERER_H
