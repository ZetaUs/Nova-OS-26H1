typedef unsigned char uint8_t;
typedef signed char int8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

static void put_pixel(int x, int y, uint32_t color);
static void pixel(uint16_t x, uint16_t y, uint8_t color);

#include "logo_data.h"
#include "chinese_font.h"

static volatile uint32_t* framebuffer;
static uint32_t screen_width;
static uint32_t screen_height;
static uint32_t screen_pitch;
static bool start_menu_open;
static int mouse_x;
static int mouse_y;
static uint8_t mouse_packet[3];
static uint8_t mouse_packet_stage;
static uint8_t mouse_buttons;
static bool mouse_cursor_drawn;
static bool keyboard_extended;
static uint32_t mouse_cursor_background[32 * 40];

static const uint16_t mouse_cursor_outline[20] = {
    0xE000, 0xF000, 0xF800, 0x9E00, 0x8F00,
    0x83C0, 0x81E0, 0x8070, 0x803C, 0x81FE,
    0x81FE, 0x98C0, 0xBCE0, 0xFC60, 0xEE70,
    0xC630, 0x8738, 0x03F8, 0x01F0, 0x01C0
};

static const uint16_t mouse_cursor_fill[20] = {
    0x0000, 0xC000, 0xE000, 0xF800, 0xFC00,
    0xFE00, 0xFF80, 0xFFC0, 0xFFF0, 0xFFF8,
    0xFF00, 0xFF80, 0xEF80, 0xC7C0, 0x83C0,
    0x03E0, 0x01F0, 0x01E0, 0x0080, 0x0000
};

static const uint8_t font_5x7[36][7] = {
    {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30},
    {14,17,16,16,16,17,14}, {30,17,17,17,17,17,30},
    {31,16,16,30,16,16,31}, {31,16,16,30,16,16,16},
    {14,17,16,23,17,17,15}, {17,17,17,31,17,17,17},
    {14,4,4,4,4,4,14}, {7,2,2,2,18,18,12},
    {17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
    {17,27,21,21,17,17,17}, {17,25,21,19,17,17,17},
    {14,17,17,17,17,17,14}, {30,17,17,30,16,16,16},
    {14,17,17,17,21,18,13}, {30,17,17,30,20,18,17},
    {15,16,16,14,1,1,30}, {31,4,4,4,4,4,4},
    {17,17,17,17,17,17,14}, {17,17,17,17,17,10,4},
    {17,17,17,21,21,21,10}, {17,17,10,4,10,17,17},
    {17,17,10,4,4,4,4}, {31,1,2,4,8,16,31},
    {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14},
    {14,17,1,2,4,8,31}, {30,1,1,14,1,1,30},
    {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14}, {31,1,2,4,8,8,8},
    {14,17,17,14,17,17,14}, {14,17,17,15,1,1,14}
};

static const uint32_t logo_colors[16] = {
    0x000000, 0x172554, 0x14532D, 0x155E75,
    0x7F1D1D, 0x701A75, 0x854D0E, 0xCBD5E1,
    0x64748B, 0x60A5FA, 0x4ADE80, 0x67E8F9,
    0xF87171, 0xE879F9, 0x75D8FF, 0xFFFFFF
};

static void pixel(uint16_t x, uint16_t y, uint8_t color) {
    put_pixel(x, y, logo_colors[color & 15]);
}

static uint32_t mix_color(uint32_t a, uint32_t b, uint32_t amount, uint32_t total) {
    const uint32_t inverse = total - amount;
    const uint32_t red = (((a >> 16) & 255) * inverse + ((b >> 16) & 255) * amount) / total;
    const uint32_t green = (((a >> 8) & 255) * inverse + ((b >> 8) & 255) * amount) / total;
    const uint32_t blue = ((a & 255) * inverse + (b & 255) * amount) / total;
    return (red << 16) | (green << 8) | blue;
}

static void put_pixel(int x, int y, uint32_t color) {
    if (x >= 0 && y >= 0 && static_cast<uint32_t>(x) < screen_width &&
        static_cast<uint32_t>(y) < screen_height) {
        framebuffer[static_cast<uint32_t>(y) * (screen_pitch / 4) + static_cast<uint32_t>(x)] = color;
    }
}

static void fill_rect(int x, int y, int width, int height, uint32_t color) {
    if (width <= 0 || height <= 0) return;
    int left = x < 0 ? 0 : x;
    int top = y < 0 ? 0 : y;
    int right = x + width > static_cast<int>(screen_width) ? static_cast<int>(screen_width) : x + width;
    int bottom = y + height > static_cast<int>(screen_height) ? static_cast<int>(screen_height) : y + height;
    for (int row = top; row < bottom; ++row) {
        volatile uint32_t* line = framebuffer + static_cast<uint32_t>(row) * (screen_pitch / 4) + left;
        for (int column = left; column < right; ++column) *line++ = color;
    }
}

static void draw_triangle(int x1, int y1, int x2, int y2, int x3, int y3, uint32_t color) {
    if (y1 > y2) { int t=x1; x1=x2; x2=t; t=y1; y1=y2; y2=t; }
    if (y2 > y3) { int t=x2; x2=x3; x3=t; t=y2; y2=y3; y3=t; }
    if (y1 > y2) { int t=x1; x1=x2; x2=t; t=y1; y1=y2; y2=t; }
    for (int y = y1; y <= y3; ++y) {
        if (y < 0 || y >= static_cast<int>(screen_height)) continue;
        int long_x = x1 + (y3 == y1 ? 0 : (x3 - x1) * (y - y1) / (y3 - y1));
        int short_x;
        if (y < y2) short_x = x1 + (y2 == y1 ? 0 : (x2 - x1) * (y - y1) / (y2 - y1));
        else short_x = x2 + (y3 == y2 ? 0 : (x3 - x2) * (y - y2) / (y3 - y2));
        if (long_x > short_x) { int t = long_x; long_x = short_x; short_x = t; }
        fill_rect(long_x, y, short_x - long_x + 1, 1, color);
    }
}

static int glyph_index(char value) {
    if (value >= 'a' && value <= 'z') value = static_cast<char>(value - 'a' + 'A');
    if (value >= 'A' && value <= 'Z') return value - 'A';
    if (value >= '0' && value <= '9') return value - '0' + 26;
    return -1;
}

static int find_chinese_glyph(uint32_t codepoint) {
    for (int index = 0; index < CHINESE_GLYPH_COUNT; ++index) {
        if (chinese_glyph_codepoints[index] == codepoint) return index;
    }
    return -1;
}

static void draw_chinese_glyph(int glyph_index, int x, int y, int scale, uint32_t color) {
    if (glyph_index < 0) return;
    const uint8_t* bitmap = chinese_glyph_bitmaps[glyph_index];
    for (int row = 0; row < 24; ++row) {
        for (int column = 0; column < 24; ++column) {
            bool filled = false;
            for (int dy = 0; dy < 2 && !filled; ++dy) {
                for (int dx = 0; dx < 2; ++dx) {
                    const int source_x = column * 2 + dx;
                    const int source_y = row * 2 + dy;
                    const uint8_t bits = bitmap[source_y * 6 + source_x / 8];
                    if (bits & (0x80 >> (source_x & 7))) {
                        filled = true;
                        break;
                    }
                }
            }
            if (filled) {
                fill_rect(x + column * scale, y + row * scale, scale, scale, color);
            }
        }
    }
}

static void draw_ascii_character(char value, int x, int y, int scale, uint32_t color) {
    const int index = glyph_index(value);
    const int pixel_scale = scale * 3;
    if (index >= 0) {
        for (int row = 0; row < 7; ++row) {
            for (int column = 0; column < 5; ++column) {
                if (font_5x7[index][row] & (1 << (4 - column))) {
                    fill_rect(x + column * pixel_scale, y + row * pixel_scale,
                              pixel_scale, pixel_scale, color);
                }
            }
        }
    } else if (value == '.') {
        fill_rect(x, y + 6 * pixel_scale, pixel_scale, pixel_scale, color);
    } else if (value == ':') {
        fill_rect(x, y + 2 * pixel_scale, pixel_scale, pixel_scale, color);
        fill_rect(x, y + 5 * pixel_scale, pixel_scale, pixel_scale, color);
    } else if (value == '-') {
        fill_rect(x, y + 3 * pixel_scale, 4 * pixel_scale, pixel_scale, color);
    }
}

static void draw_text(const char* text, int x, int y, int scale, uint32_t color) {
    const uint8_t* current = reinterpret_cast<const uint8_t*>(text);
    while (*current) {
        uint32_t codepoint;
        if (*current < 0x80) {
            codepoint = *current++;
        } else if ((*current & 0xE0) == 0xC0 && current[1] != 0) {
            codepoint = ((*current & 0x1F) << 6) | (current[1] & 0x3F);
            current += 2;
        } else if ((*current & 0xF0) == 0xE0 && current[1] != 0 && current[2] != 0) {
            codepoint = ((*current & 0x0F) << 12) |
                        ((current[1] & 0x3F) << 6) | (current[2] & 0x3F);
            current += 3;
        } else {
            ++current;
            x += 24 * scale;
            continue;
        }

        if (codepoint >= 0x4E00 && codepoint <= 0x9FFF) {
            draw_chinese_glyph(find_chinese_glyph(codepoint), x, y, scale, color);
            x += 24 * scale;
        } else if (codepoint == ' ') {
            x += 12 * scale;
        } else {
            draw_ascii_character(static_cast<char>(codepoint), x, y, scale, color);
            x += 18 * scale;
        }
    }
}

static void draw_logo(int x, int y, int scale) {
    for (uint32_t row = 0; row < logo_height; ++row) {
        for (uint32_t column = 0; column < logo_width; ++column) {
            const uint8_t index = logo_data[row * logo_width + column];
            if (index == 0) continue;
            fill_rect(x + static_cast<int>(column) * scale,
                     y + static_cast<int>(row) * scale,
                     scale, scale, logo_colors[index & 15]);
        }
    }
}

static void draw_app_icon(int x, int y, bool explorer) {
    fill_rect(x, y, 72, 72, explorer ? 0x117FCA : 0x096DB2);
    fill_rect(x + 3, y + 3, 66, 66, explorer ? 0x46B2E6 : 0x58BDF4);
    if (explorer) {
        fill_rect(x + 15, y + 19, 42, 34, 0xF6C95A);
        fill_rect(x + 15, y + 16, 22, 8, 0xFFE08B);
        fill_rect(x + 19, y + 28, 34, 21, 0xFFF0C7);
    } else {
        fill_rect(x + 14, y + 15, 44, 40, 0xE6F6FF);
        fill_rect(x + 17, y + 18, 38, 34, 0x4389C5);
        fill_rect(x + 34, y + 18, 4, 34, 0xE6F6FF);
        fill_rect(x + 17, y + 33, 38, 4, 0xE6F6FF);
    }
}

static void draw_nova_mark(int x, int y) {
    const uint32_t color = 0x75D8FF;
    fill_rect(x, y, 44, 44, 0x103653);
    fill_rect(x + 2, y + 2, 40, 40, 0x15527A);
    fill_rect(x + 9, y + 10, 6, 24, color);
    fill_rect(x + 29, y + 10, 6, 24, color);
    draw_triangle(x + 15, y + 10, x + 21, y + 10, x + 29, y + 34, color);
    draw_triangle(x + 15, y + 10, x + 23, y + 34, x + 29, y + 34, color);
}

static void draw_wallpaper() {
    for (uint32_t y = 0; y < screen_height; ++y) {
        const uint32_t color = mix_color(0x07172A, 0x102F50, y, screen_height);
        volatile uint32_t* row = framebuffer + y * (screen_pitch / 4);
        for (uint32_t x = 0; x < screen_width; ++x) row[x] = color;
    }
    draw_triangle(930, 120, 2490, 180, 1880, 1310, 0x0B2947);
    draw_triangle(1260, 230, 2460, 350, 1890, 1170, 0x0D3558);
    draw_triangle(1530, 370, 2370, 460, 1940, 1030, 0x10446C);
    fill_rect(0, 0, static_cast<int>(screen_width), 2, 0x28A9E0);

    const int mark_x = static_cast<int>(screen_width) - 840;
    const int mark_y = 330;
    fill_rect(mark_x - 38, mark_y - 38, 556, 556, 0x123D62);
    fill_rect(mark_x - 31, mark_y - 31, 542, 542, 0x0C2C4A);
    draw_logo(mark_x, mark_y, 6);
    draw_text("星云操作系统", mark_x - 14, mark_y + 540, 2, 0xFFFFFF);
    draw_text("2026 上半年版", mark_x + 48, mark_y + 595, 1, 0x92CFF2);
}

static void draw_start_menu() {
    fill_rect(24, static_cast<int>(screen_height) - 618, 570, 530, 0xE8142942);
    fill_rect(24, static_cast<int>(screen_height) - 618, 6, 530, 0x36BDF0);
    draw_text("开始菜单", 62, static_cast<int>(screen_height) - 574, 1, 0xFFFFFF);
    draw_text("星云操作系统 26H1", 62, static_cast<int>(screen_height) - 536, 1, 0x75D8FF);
    draw_app_icon(66, static_cast<int>(screen_height) - 454, false);
    draw_text("此电脑", 160, static_cast<int>(screen_height) - 431, 1, 0xFFFFFF);
    draw_app_icon(66, static_cast<int>(screen_height) - 350, true);
    draw_text("文件管理", 160, static_cast<int>(screen_height) - 329, 1, 0xFFFFFF);
    fill_rect(52, static_cast<int>(screen_height) - 120, 510, 1, 0x38536F);
    draw_text("按Win键开关菜单", 62, static_cast<int>(screen_height) - 117, 1, 0x9BB1C8);
}

static void draw_desktop() {
    draw_wallpaper();

    draw_app_icon(54, 62, false);
    draw_text("此电脑", 54, 143, 1, 0xFFFFFF);
    draw_app_icon(54, 218, true);
    draw_text("文件管理", 54, 299, 1, 0xFFFFFF);

    fill_rect(0, static_cast<int>(screen_height) - 76, static_cast<int>(screen_width), 76, 0xE8102033);
    fill_rect(0, static_cast<int>(screen_height) - 76, static_cast<int>(screen_width), 1, 0x31506C);
    fill_rect(22, static_cast<int>(screen_height) - 64, 52, 52, start_menu_open ? 0x15527A : 0x102B45);
    draw_nova_mark(26, static_cast<int>(screen_height) - 60);
    fill_rect(92, static_cast<int>(screen_height) - 58, 1, 40, 0x38516A);
    draw_text("星云系统", 116, static_cast<int>(screen_height) - 50, 1, 0xFFFFFF);
    draw_text("64位", static_cast<int>(screen_width) - 220, static_cast<int>(screen_height) - 50, 1, 0xA8C4DB);
    fill_rect(static_cast<int>(screen_width) - 120, static_cast<int>(screen_height) - 52, 1, 30, 0x38516A);
    draw_text("26H1版", static_cast<int>(screen_width) - 106, static_cast<int>(screen_height) - 50, 1, 0x75D8FF);

    if (start_menu_open) draw_start_menu();
}

static uint8_t in8(uint16_t port) {
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void out8(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static bool wait_mouse_input_ready() {
    for (uint32_t timeout = 0; timeout < 1000000; ++timeout) {
        if ((in8(0x64) & 2) == 0) return true;
    }
    return false;
}

static bool wait_mouse_output_ready() {
    for (uint32_t timeout = 0; timeout < 1000000; ++timeout) {
        if ((in8(0x64) & 1) != 0) return true;
    }
    return false;
}

static bool send_mouse_command(uint8_t command) {
    if (!wait_mouse_input_ready()) return false;
    out8(0x64, 0xD4);
    if (!wait_mouse_input_ready()) return false;
    out8(0x60, command);
    if (!wait_mouse_output_ready()) return false;
    return in8(0x60) == 0xFA;
}

static bool initialize_mouse() {
    while ((in8(0x64) & 1) != 0) in8(0x60);
    if (!wait_mouse_input_ready()) return false;
    out8(0x64, 0xA8);
    if (!send_mouse_command(0xF6)) return false;
    return send_mouse_command(0xF4);
}

static void restore_mouse_cursor() {
    if (!mouse_cursor_drawn) return;

    for (int row = 0; row < 40; ++row) {
        const int y = mouse_y + row;
        if (y < 0 || y >= static_cast<int>(screen_height)) continue;
        for (int column = 0; column < 32; ++column) {
            const int x = mouse_x + column;
            if (x < 0 || x >= static_cast<int>(screen_width)) continue;
            framebuffer[static_cast<uint32_t>(y) * (screen_pitch / 4) +
                        static_cast<uint32_t>(x)] = mouse_cursor_background[row * 32 + column];
        }
    }
    mouse_cursor_drawn = false;
}

static void draw_mouse_cursor() {
    for (int row = 0; row < 20; ++row) {
        for (int column = 0; column < 16; ++column) {
            const int bit = 15 - column;
            const bool outline = (mouse_cursor_outline[row] & (1u << bit)) != 0;
            const bool fill = (mouse_cursor_fill[row] & (1u << bit)) != 0;
            for (int scale_y = 0; scale_y < 2; ++scale_y) {
                const int y = mouse_y + row * 2 + scale_y;
                if (y < 0 || y >= static_cast<int>(screen_height)) continue;
                for (int scale_x = 0; scale_x < 2; ++scale_x) {
                    const int x = mouse_x + column * 2 + scale_x;
                    if (x < 0 || x >= static_cast<int>(screen_width)) continue;

                    const int pixel_index = (row * 2 + scale_y) * 32 + column * 2 + scale_x;
                    const uint32_t offset = static_cast<uint32_t>(y) * (screen_pitch / 4) +
                                            static_cast<uint32_t>(x);
                    mouse_cursor_background[pixel_index] = framebuffer[offset];
                    if (outline) framebuffer[offset] = 0x00101A26;
                    else if (fill) framebuffer[offset] = 0x00FFFFFF;
                }
            }
        }
    }
    mouse_cursor_drawn = true;
}

static void move_mouse(int delta_x, int delta_y) {
    const int max_x = static_cast<int>(screen_width) - 1;
    const int max_y = static_cast<int>(screen_height) - 1;
    mouse_x += delta_x;
    mouse_y += delta_y;
    if (mouse_x < 0) mouse_x = 0;
    if (mouse_x > max_x) mouse_x = max_x;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_y > max_y) mouse_y = max_y;
}

static void handle_mouse_packet() {
    const uint8_t flags = mouse_packet[0];
    if ((flags & 0xC0) != 0) return;

    restore_mouse_cursor();
    move_mouse(static_cast<int8_t>(mouse_packet[1]),
               -static_cast<int8_t>(mouse_packet[2]));

    const uint8_t buttons = flags & 7;
    if ((buttons & 1) != 0 && (mouse_buttons & 1) == 0 &&
        mouse_x < 84 && mouse_y >= static_cast<int>(screen_height) - 76) {
        start_menu_open = !start_menu_open;
        draw_desktop();
    }
    mouse_buttons = buttons;
    draw_mouse_cursor();
}

extern "C" __attribute__((noreturn)) void kernel_main(
    uint32_t framebuffer_base,
    uint32_t width,
    uint32_t height,
    uint32_t pitch,
    uint32_t bpp
) {
    if (framebuffer_base == 0 || width == 0 || height == 0 || pitch < width * 4 || bpp != 32) {
        for (;;) __asm__ volatile("cli; hlt");
    }

    framebuffer = reinterpret_cast<volatile uint32_t*>(
        static_cast<unsigned long long>(framebuffer_base));
    screen_width = width;
    screen_height = height;
    screen_pitch = pitch;
    start_menu_open = false;
    mouse_x = static_cast<int>(screen_width / 2);
    mouse_y = static_cast<int>(screen_height / 2);
    mouse_packet_stage = 0;
    mouse_buttons = 0;
    mouse_cursor_drawn = false;
    keyboard_extended = false;
    draw_desktop();
    initialize_mouse();
    draw_mouse_cursor();

    for (;;) {
        const uint8_t status = in8(0x64);
        if ((status & 1) != 0) {
            const uint8_t scan_code = in8(0x60);
            if ((status & 0x20) != 0) {
                if (mouse_packet_stage == 0) {
                    if ((scan_code & 0x08) != 0) {
                        mouse_packet[0] = scan_code;
                        mouse_packet_stage = 1;
                    }
                } else {
                    mouse_packet[mouse_packet_stage++] = scan_code;
                    if (mouse_packet_stage == 3) {
                        mouse_packet_stage = 0;
                        handle_mouse_packet();
                    }
                }
            } else if (scan_code == 0xE0) {
                keyboard_extended = true;
            } else {
                const bool extended = keyboard_extended;
                keyboard_extended = false;
                if (extended && (scan_code == 0x5B || scan_code == 0x5C)) {
                    restore_mouse_cursor();
                    start_menu_open = !start_menu_open;
                    draw_desktop();
                    draw_mouse_cursor();
                } else if (scan_code == 0x01 && start_menu_open) {
                    restore_mouse_cursor();
                    start_menu_open = false;
                    draw_desktop();
                    draw_mouse_cursor();
                }
            }
        }
        __asm__ volatile("pause");
    }
}
