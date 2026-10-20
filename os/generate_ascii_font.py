import sys
from pathlib import Path


def main():
    if len(sys.argv) != 3:
        raise SystemExit("Usage: generate_ascii_font.py asc48.bin output.h")

    font_path, output_path = map(Path, sys.argv[1:])
    font = font_path.read_bytes()
    expected_size = 63 * 288
    if len(font) != expected_size:
        raise ValueError("ASC48 font must contain exactly {} bytes, got {}".format(
            expected_size, len(font)))

    lines = ["static const uint8_t ascii_glyph_bitmaps[ASC48_CHAR_COUNT][ASC48_CHAR_SIZE] = {"]
    for start in range(0, len(font), 288):
        lines.append("    {")
        glyph = font[start:start + 288]
        for row in range(0, len(glyph), 16):
            lines.append("        " + ", ".join(
                "0x{:02X}".format(value) for value in glyph[row:row + 16]) + ",")
        lines.append("    },")
    lines.append("};")
    output_path.write_text("\n".join(lines) + "\n", encoding="ascii")
    print("Embedded {} ASC48 glyphs.".format(len(font) // 288))


if __name__ == "__main__":
    main()