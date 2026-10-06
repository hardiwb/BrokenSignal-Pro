#pragma once
#include <Arduino.h>

static inline uint16_t rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint16_t)(r >> 3) << 11) | ((uint16_t)(g >> 2) << 5) | (b >> 3);
}

struct Theme
{
    uint16_t bg, hdrBg, accent1, accent2, accent3, textBright, textMid, textDim, barBg, selRow;
    const char *name;
};

static const Theme T_NEON = {
    rgb(  2,  4,  8),   // bg        #020408
    rgb(  5, 14, 24),   // hdrBg     #050e18
    rgb(255, 45,120),   // accent1   #ff2d78  magenta  (playing, bar fill tip)
    rgb(  0,245,255),   // accent2   #00f5ff  cyan     (selected, bar fill)
    rgb(245,230, 66),   // accent3   #f5e642  yellow   (counter, volume)
    rgb(200,234,245),   // textBright #c8eaf5
    rgb( 58,106,122),   // textMid   mid blue-grey
    rgb( 14, 48, 64),   // textDim   #0e3040  very dim
    rgb(  9, 21, 32),   // barBg     #091520
    rgb( 10, 30, 46),   // selRow    #0a1e2e  (mockup gradient start)
    "NEON NOIR"
};

static const Theme T_TERM = {
    rgb(0, 4, 0),       // bg
    rgb(0, 12, 0),      // hdrBg
    rgb(0, 255, 65),    // accent1
    rgb(255, 145, 0),   // accent2
    rgb(25, 230, 69),   // accent3
    rgb(190, 255, 200), // textBright
    rgb(0, 195, 55),    // textMide
    rgb(0, 70, 15),     // textDims
    rgb(0, 16, 2),      // barBgh
    rgb(0, 28, 6),      // selRow
    "GLITCH TERMINAL"};

static const Theme T_CORP = {
    rgb(8, 10, 15),     // bg
    rgb(12, 15, 24),    // hdrBg
    rgb(200, 168, 75),  // accent1
    rgb(232, 201, 106), // accent2
    rgb(200, 168, 75),  // accent3
    rgb(200, 207, 224), // textBright
    rgb(58, 74, 106),   // textMid
    rgb(28, 34, 53),    // textDim
    rgb(28, 34, 53),    // barBg
    rgb(14, 18, 30),    // selRow
    "CORPO CHROME"};

static const Theme T_MIAMI = {
    rgb(  2,  6, 16),   // bg        very dark navy
    rgb(  5, 12, 30),   // hdrBg     deep navy
    rgb(255, 62,181),   // accent1   #ff3eb5  hot pink  (playing, bar tip)
    rgb(  0,229,204),   // accent2   #00e5cc  turquoise (selected, bar fill)
    rgb(255,210,  0),   // accent3   #ffd200  gold      (counter, volume)
    rgb(255,200,235),   // textBright pale pink-white
    rgb(120, 80,140),   // textMid   muted violet
    rgb( 40, 20, 55),   // textDim   dark purple
    rgb(  8, 10, 28),   // barBg     near-black navy
    rgb( 20, 10, 38),   // selRow    deep violet tint
    "MIAMI VICE"};

static const Theme T_ASH = {
    rgb(16, 16, 16),    // bg
    rgb(22, 22, 22),    // hdrBg
    rgb(255, 255, 255), // accent1
    rgb(180, 180, 180), // accent2
    rgb(140, 140, 140), // accent3
    rgb(230, 230, 230), // textBright
    rgb(140, 140, 140), // textMid
    rgb(60, 60, 60),    // textDim
    rgb(35, 35, 35),    // barBg
    rgb(30, 30, 30),    // selRow
    "ASH"};

static const Theme T_BRUCE_ORANGE = {
    rgb(3, 2, 1),       // bg         warm black
    rgb(15, 8, 2),      // hdrBg      near-black orange tint
    rgb(255, 130, 0),   // accent1    Bruce orange
    rgb(255, 222, 184), // accent2    warm orange-white
    rgb(255, 177, 74),  // accent3    soft amber
    rgb(255, 241, 224), // textBright warm white
    rgb(198, 119, 47),  // textMid    muted orange
    rgb(91, 44, 12),    // textDim    dark burnt orange
    rgb(27, 13, 4),     // barBg      dark orange-black
    rgb(43, 20, 5),     // selRow     warm selection row
    "BRUCE ORANGE"};

static const Theme *THEMES[] = {
    &T_NEON, &T_TERM, &T_CORP, &T_MIAMI, &T_ASH, &T_BRUCE_ORANGE};
static constexpr uint8_t THEME_COUNT = sizeof(THEMES) / sizeof(THEMES[0]);
