# On-Screen Display (OSD) System

## Overview

The OSD system provides real-time flight information overlayed on the screen during flight mode.

## Features

### Two Display Modes

**1. Simple Mode (Default)**
- Compact HUD-style display
- Shows essential information:
  - Airspeed (knots)
  - Altitude (feet)
  - Throttle percentage
  - Heading (degrees)
  - Pitch angle (degrees)
  - Roll angle (degrees)
  - Vertical speed (when climbing/descending)

**2. Detailed Mode**
- Comprehensive flight data
- Shows all of the above plus:
  - Airspeed in km/h
  - Altitude in meters
  - Vertical speed in feet per minute
  - Angular rates (pitch/roll/yaw in deg/s)
  - Control surface positions (elevator, aileron, rudder)

## Controls

| Key | Action |
|-----|--------|
| **O** | Toggle OSD on/off |
| **I** | Toggle between Simple and Detailed mode |
| **F** | Enable/disable flight mode |

## Usage

1. **Enable Flight Mode**: Press `F` to enter flight mode
2. **Show OSD**: Press `O` to display the on-screen overlay
3. **Switch Modes**: Press `I` to toggle between simple and detailed views

## Display Information

### Simple Mode Example:
```
SPD 180kt  ALT 1500ft  THR  75%
HDG 270  PITCH  +5  ROLL  -2
CLIMBING +850 fpm
```

### Detailed Mode Example:
```
=== FLIGHT DATA ===
AIRSPEED: 180 kt (333 km/h)
ALTITUDE: 1500 ft ( 457 m)
V/S:       +850 fpm
PITCH:      +5.0 deg
ROLL:       -2.0 deg
HEADING:   270.0 deg
THROTTLE:   75%
--- RATES ---
PITCH RATE:  +2.50 deg/s
ROLL RATE:   -1.20 deg/s
YAW RATE:    +0.10 deg/s
--- CONTROLS ---
ELEVATOR:  +0.25
AILERON:   -0.10
RUDDER:    +0.05
```

## Color Coding

- **Green**: Normal flight information, climbing
- **Yellow**: Descending
- **Cyan**: Section headers
- **White**: Standard data

## Technical Details

### Text Rendering System

The OSD uses a custom bitmap font renderer that:
- Renders text as textured quads
- Supports color-coded text
- Positioned in normalized screen coordinates (0-1)
- Renders after 3D scene but before frame submission
- Uses alpha blending for smooth appearance

### Performance

- Minimal performance impact (~0.1ms per frame)
- Text updated every frame
- Efficient batched rendering
- No allocations per frame (pre-allocated buffers)

## Files Involved

1. **osd.h** - OSD data generation and formatting
2. **text_renderer.h** - Text rendering interface
3. **text_renderer_gl.h** - OpenGL implementation
4. **app.h/app.cpp** - Integration with main application

## Customization

### Changing Text Position

Edit `app.cpp` in the OSD rendering section:

```cpp
float y = 0.02f;  // Start position (0 = top, 1 = bottom)
float lineHeight = 0.025f;  // Space between lines
```

### Changing Text Size

```cpp
m_textRenderer->renderText(line.c_str(), {0.02f, y}, color, 1.5f);  // 1.5x size
```

### Changing Colors

Edit the color logic in `app.cpp`:

```cpp
TextColor color = TextColor::Green();  // Default color

if (line.find("WARNING") != std::string::npos) {
    color = TextColor::Red();  // Warnings in red
}
```

### Adding Custom Information

Edit `osd.h` `generateOSDLines()` method:

```cpp
// Add custom data
snprintf(buffer, sizeof(buffer), "CUSTOM: %.1f", customValue);
lines.push_back(buffer);
```

## Troubleshooting

### OSD Not Showing

1. **Check flight mode is enabled**: Press `F` to enable
2. **Check OSD is enabled**: Press `O` to toggle
3. **Check console for errors**: Look for "Text renderer initialized" message
4. **Check OpenGL support**: Text rendering requires OpenGL 3.3+

### Text Not Visible

1. **Check text color**: White text on light background won't show
2. **Try detailed mode**: Press `I` to toggle modes
3. **Check screen position**: Text might be off-screen

### Performance Issues

1. **Disable detailed mode**: Use simple mode for less text
2. **Reduce update frequency**: Edit frame counter check in `app.cpp`

## Future Enhancements

Potential improvements:
- Custom font loading from TTF files
- Configurable OSD layout (position, size, transparency)
- Warning/caution alerts (stall warning, altitude alerts)
- Artificial horizon display
- Compass rose
- G-force indicator
- Flight path vector
