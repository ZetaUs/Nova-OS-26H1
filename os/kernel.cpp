typedef unsigned char uint8_t;
typedef signed char int8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uintptr_t;

static void put_pixel(int x, int y, uint32_t color);
static void pixel(uint16_t x, uint16_t y, uint8_t color);
static void draw_start_menu_actions();

#include "logo_data.h"
#include "chinese_font.h"
#include "../HZKX/asc48_map.h"
#include "ascii_font.h"
#include <cstddef>

static volatile uint32_t* framebuffer;
static uint32_t* render_target;
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
static uint8_t menu_selection;
static uint32_t mouse_cursor_background[32 * 40];
static const uintptr_t backbuffer_address = 0x01000000;

static inline void copy_to_framebuffer(
    volatile uint32_t* destination,
    const uint32_t* source,
    uint32_t pixel_count
) {
    __asm__ volatile(
        "cld; rep movsl"
        : "+D"(destination), "+S"(source), "+c"(pixel_count)
        :
        : "memory"
    );
}

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
        uint32_t* line = render_target + static_cast<uint32_t>(row) * (screen_pitch / 4) + left;
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

static bool ascii_pixel_filled(const uint8_t* bitmap, int row, int column) {
    const int source_x = column * 2;
    const int byte_index = source_x / 8;
    const uint8_t left_mask = static_cast<uint8_t>(0x80 >> (source_x & 7));
    const uint8_t right_mask = static_cast<uint8_t>(left_mask >> 1);
    const uint8_t* source_top = bitmap + row * 12;
    const uint8_t* source_bottom = source_top + 6;
    return ((source_top[byte_index] & left_mask) != 0) ||
           ((source_top[byte_index] & right_mask) != 0) ||
           ((source_bottom[byte_index] & left_mask) != 0) ||
           ((source_bottom[byte_index] & right_mask) != 0);
}

static int draw_ascii_character(char value, int x, int y, int scale, uint32_t color) {
    const uint8_t* bitmap = asc48_get_bitmap(value, ascii_glyph_bitmaps[0]);
    if (bitmap == NULL) {
        const int pixel_scale = scale * 3;
        if (value == '.') {
            fill_rect(x, y + 6 * pixel_scale, pixel_scale, pixel_scale, color);
            return 6 * scale;
        }
        if (value == '-') {
            fill_rect(x, y + 3 * pixel_scale, 4 * pixel_scale, pixel_scale, color);
            return 15 * scale;
        }
        return 15 * scale;
    }

    int first_column = 24;
    int last_column = -1;
    for (int row = 0; row < 24; ++row) {
        for (int column = 0; column < 24; ++column) {
            if (!ascii_pixel_filled(bitmap, row, column)) continue;
            if (column < first_column) first_column = column;
            if (column > last_column) last_column = column;
        }
    }
    if (last_column < first_column) return 15 * scale;

    const uint32_t row_stride = screen_pitch / 4;
    for (int row = 0; row < 24; ++row) {
        for (int column = first_column; column <= last_column; ++column) {
            if (!ascii_pixel_filled(bitmap, row, column)) continue;

            const int target_x = x + (column - first_column) * scale;
            const int target_y = y + row * scale;
            for (int scale_y = 0; scale_y < scale; ++scale_y) {
                const int draw_y = target_y + scale_y;
                if (draw_y < 0 || draw_y >= static_cast<int>(screen_height)) continue;
                uint32_t* line = render_target + static_cast<uint32_t>(draw_y) * row_stride;
                for (int scale_x = 0; scale_x < scale; ++scale_x) {
                    const int draw_x = target_x + scale_x;
                    if (draw_x >= 0 && draw_x < static_cast<int>(screen_width)) line[draw_x] = color;
                }
            }
        }
        return (last_column - first_column + 1 + 2) * scale;
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
            x += 7 * scale;
        } else {
            x += draw_ascii_character(static_cast<char>(codepoint), x, y, scale, color);
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
        uint32_t* row = render_target + y * (screen_pitch / 4);
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
    draw_text("Nova", mark_x - 14, mark_y + 540, 3, 0xFFFFFF);
    draw_text("2026 下半年版", mark_x + 48, mark_y + 605, 1, 0x92CFF2);
}

static void draw_start_menu() {
    fill_rect(24, static_cast<int>(screen_height) - 618, 570, 530, 0xE8142942);
    fill_rect(24, static_cast<int>(screen_height) - 618, 6, 530, 0x36BDF0);
    draw_text("开始菜单", 62, static_cast<int>(screen_height) - 574, 1, 0xFFFFFF);
    draw_text("Nova 26H2", 62, static_cast<int>(screen_height) - 536, 1, 0x75D8FF);
    draw_app_icon(66, static_cast<int>(screen_height) - 454, false);
    draw_text("此电脑", 148, static_cast<int>(screen_height) - 431, 1, 0xFFFFFF);
    draw_app_icon(66, static_cast<int>(screen_height) - 350, true);
    draw_text("文件管理", 142, static_cast<int>(screen_height) - 329, 1, 0xFFFFFF);

    draw_start_menu_actions();

    fill_rect(52, static_cast<int>(screen_height) - 120, 510, 1, 0x38536F);
    draw_text("按Win键开关菜单", 62, static_cast<int>(screen_height) - 117, 1, 0x9BB1C8);
}

static void present_backbuffer() {
    const uint32_t pixels_per_row = screen_width;
    const uint32_t row_stride = screen_pitch / 4;
    for (uint32_t y = 0; y < screen_height; ++y) {
        volatile uint32_t* destination = framebuffer + y * row_stride;
        const uint32_t* source = render_target + y * row_stride;
        copy_to_framebuffer(destination, source, pixels_per_row);
    }
}

static void present_rect(int x, int y, int width, int height) {
    const int left = x < 0 ? 0 : x;
    const int top = y < 0 ? 0 : y;
    const int right = x + width > static_cast<int>(screen_width)
        ? static_cast<int>(screen_width) : x + width;
    const int bottom = y + height > static_cast<int>(screen_height)
        ? static_cast<int>(screen_height) : y + height;
    const uint32_t row_stride = screen_pitch / 4;
    for (int row = top; row < bottom; ++row) {
        volatile uint32_t* destination = framebuffer + static_cast<uint32_t>(row) * row_stride + left;
        const uint32_t* source = render_target + static_cast<uint32_t>(row) * row_stride + left;
        copy_to_framebuffer(destination, source, static_cast<uint32_t>(right - left));
    }
}

static void draw_start_menu_actions() {
    const int restart_y = static_cast<int>(screen_height) - 264;
    const int shutdown_y = static_cast<int>(screen_height) - 204;
    fill_rect(52, restart_y, 510, 52, menu_selection == 0 ? 0x24506F : 0x182F4A);
    fill_rect(68, restart_y + 8, 36, 36, 0x14577A);
    draw_text("重启", 120, restart_y + 14, 1, 0xFFFFFF);
    fill_rect(52, shutdown_y, 510, 52, menu_selection == 1 ? 0x24506F : 0x182F4A);
    fill_rect(68, shutdown_y + 8, 36, 36, 0x673844);
    draw_text("关机", 120, shutdown_y + 14, 1, 0xFFFFFF);
}

static void refresh_start_menu_actions() {
    render_target = reinterpret_cast<uint32_t*>(backbuffer_address);
    draw_start_menu_actions();
    present_rect(52, static_cast<int>(screen_height) - 264, 510, 112);
}

static void refresh_start_menu() {
    render_target = reinterpret_cast<uint32_t*>(backbuffer_address);
    const int top = static_cast<int>(screen_height) - 618;
    if (start_menu_open) {
        draw_start_menu();
    } else {
        const int first_row = top < 0 ? 0 : top;
        const int last_row = top + 530 > static_cast<int>(screen_height)
            ? static_cast<int>(screen_height) : top + 530;
        for (int row = first_row; row < last_row; ++row) {
            const uint32_t color = mix_color(0x07172A, 0x102F50,
                                             static_cast<uint32_t>(row), screen_height);
            fill_rect(24, row, 570, 1, color);
        }
    }
    present_rect(24, top, 570, 530);
}

static void draw_desktop() {
    render_target = reinterpret_cast<uint32_t*>(backbuffer_address);
    draw_wallpaper();

    draw_app_icon(54, 62, false);
    draw_text("此电脑", 54, 143, 1, 0xFFFFFF);
    draw_app_icon(54, 218, true);
    draw_text("文件管理", 42, 299, 1, 0xFFFFFF);

    fill_rect(0, static_cast<int>(screen_height) - 76, static_cast<int>(screen_width), 76, 0xE8102033);
    fill_rect(0, static_cast<int>(screen_height) - 76, static_cast<int>(screen_width), 1, 0x31506C);
    fill_rect(22, static_cast<int>(screen_height) - 64, 52, 52, start_menu_open ? 0x15527A : 0x102B45);
    draw_nova_mark(26, static_cast<int>(screen_height) - 60);
    fill_rect(92, static_cast<int>(screen_height) - 58, 1, 40, 0x38516A);
    draw_text("Nova", 116, static_cast<int>(screen_height) - 50, 1, 0xFFFFFF);
    draw_text("64位", static_cast<int>(screen_width) - 220, static_cast<int>(screen_height) - 50, 1, 0xA8C4DB);
    fill_rect(static_cast<int>(screen_width) - 120, static_cast<int>(screen_height) - 52, 1, 30, 0x38516A);
    draw_text("26H2版", static_cast<int>(screen_width) - 130, static_cast<int>(screen_height) - 50, 1, 0x75D8FF);

    if (start_menu_open) draw_start_menu();
    present_backbuffer();
}

static uint8_t in8(uint16_t port) {
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void out8(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static void out16(uint16_t port, uint16_t value) {
    __asm__ volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}

static uint16_t pit_read_count() {
    out8(0x43, 0);
    const uint16_t low = in8(0x40);
    const uint16_t high = in8(0x40);
    return low | (high << 8);
}

static void wait_pit_ticks(uint32_t ticks) {
    uint16_t previous = pit_read_count();
    while (ticks != 0) {
        const uint16_t current = pit_read_count();
        if (current > previous) --ticks;
        previous = current;
        __asm__ volatile("pause");
    }
}

static void draw_loading_screen() {
    render_target = reinterpret_cast<uint32_t*>(backbuffer_address);
    fill_rect(0, 0, static_cast<int>(screen_width), static_cast<int>(screen_height), 0x07172A);

    const int center_x = static_cast<int>(screen_width / 2);
    const int center_y = static_cast<int>(screen_height / 2);
    draw_logo(center_x - 160, center_y - 230, 4);
    draw_text("Nova", center_x - 108, center_y + 110, 3, 0xFFFFFF);
    draw_text("2026 下半年版", center_x - 132, center_y + 182, 1, 0x92CFF2);
    draw_text("正在启动", center_x - 96, center_y + 226, 1, 0xCBD5E1);

    const int progress_x = center_x - 180;
    const int progress_y = center_y + 270;
    fill_rect(progress_x, progress_y, 360, 8, 0x31506C);
    fill_rect(progress_x + 2, progress_y + 2, 356, 4, 0x102B45);
    present_backbuffer();

    for (int progress = 0; progress <= 100; progress += 5) {
        const int progress_width = progress * 356 / 100;
        fill_rect(progress_x + 2, progress_y + 2, progress_width, 4, 0x36BDF0);
        present_rect(progress_x + 2, progress_y + 2, progress_width, 4);
        wait_pit_ticks(1);
    }
}

static __attribute__((noreturn)) void restart_system() {
    while ((in8(0x64) & 2) != 0) __asm__ volatile("pause");
    out8(0x64, 0xFE);
    for (;;) __asm__ volatile("cli; hlt");
}

static __attribute__((noreturn)) void shutdown_system() {
    out16(0x0604, 0x2000);
    out16(0xB004, 0x2000);
    for (;;) __asm__ volatile("cli; hlt");
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
    if (!send_mouse_command(0xF3)) return false;
    if (!send_mouse_command(200)) return false;
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
    move_mouse(static_cast<int8_t>(mouse_packet[1]) * 2,
               -static_cast<int8_t>(mouse_packet[2]) * 2);

    const uint8_t buttons = flags & 7;
    if ((buttons & 1) != 0 && (mouse_buttons & 1) == 0) {
        const int screen_bottom = static_cast<int>(screen_height);
        if (start_menu_open && mouse_x >= 52 && mouse_x < 562 &&
            mouse_y >= screen_bottom - 264 && mouse_y < screen_bottom - 212) {
            restart_system();
        } else if (start_menu_open && mouse_x >= 52 && mouse_x < 562 &&
                   mouse_y >= screen_bottom - 204 && mouse_y < screen_bottom - 152) {
            shutdown_system();
        } else if (mouse_x < 84 && mouse_y >= screen_bottom - 76) {
            start_menu_open = !start_menu_open;
            refresh_start_menu();
        } else if (start_menu_open &&
                   (mouse_x < 24 || mouse_x >= 594 ||
                    mouse_y < screen_bottom - 618 || mouse_y >= screen_bottom - 88)) {
            start_menu_open = false;
            refresh_start_menu();
        }
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
    render_target = reinterpret_cast<uint32_t*>(backbuffer_address);
    start_menu_open = false;
    mouse_x = static_cast<int>(screen_width / 2);
    mouse_y = static_cast<int>(screen_height / 2);
    mouse_packet_stage = 0;
    mouse_buttons = 0;
    mouse_cursor_drawn = false;
    keyboard_extended = false;
    menu_selection = 0;
    draw_loading_screen();
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
                    refresh_start_menu();
                    draw_mouse_cursor();
                } else if (extended && start_menu_open &&
                           (scan_code == 0x48 || scan_code == 0x50)) {
                    menu_selection = scan_code == 0x48 ? 0 : 1;
                    restore_mouse_cursor();
                    refresh_start_menu_actions();
                    draw_mouse_cursor();
                } else if (!extended && start_menu_open && scan_code == 0x1C) {
                    if (menu_selection == 0) restart_system();
                    shutdown_system();
                } else if (scan_code == 0x01 && start_menu_open) {
                    restore_mouse_cursor();
                    start_menu_open = false;
                    refresh_start_menu();
                    draw_mouse_cursor();
                }
            }
        }
        __asm__ volatile("pause");
    }
}
