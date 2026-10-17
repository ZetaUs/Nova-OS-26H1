import sys
from pathlib import Path


def main():
    if len(sys.argv) != 4:
        raise SystemExit("Usage: generate_chinese_font.py glyphs.txt HZK48 output.h")

    glyphs_path, font_path, output_path = map(Path, sys.argv[1:])
    glyphs = list(dict.fromkeys(glyphs_path.read_text(encoding="utf-8")))
    font = font_path.read_bytes()
    glyph_bytes = 48 * 48 // 8
    codepoints = []
    bitmaps = []

    for character in glyphs:
        try:
            encoded = character.encode("gb2312")
        except UnicodeEncodeError:
            continue
        if len(encoded) != 2:
            continue

        zone, position = encoded[0] - 0xA0, encoded[1] - 0xA0
        offset = ((zone - 1) * 94 + position - 1) * glyph_bytes
        bitmap = font[offset:offset + glyph_bytes]
        if len(bitmap) != glyph_bytes:
            raise ValueError("The HZK48 font is missing glyph U+{:04X}".format(ord(character)))
        codepoints.append(ord(character))
        bitmaps.append(bitmap)

    if not codepoints:
        raise ValueError("The glyph list does not contain any GB2312 Chinese characters.")

    lines = [
        "#define CHINESE_GLYPH_COUNT {}".format(len(codepoints)),
        "static const uint32_t chinese_glyph_codepoints[CHINESE_GLYPH_COUNT] = {",
        "    " + ", ".join("0x{:04X}".format(value) for value in codepoints),
        "};",
        "static const uint8_t chinese_glyph_bitmaps[CHINESE_GLYPH_COUNT][288] = {",
    ]

    for bitmap in bitmaps:
        lines.append("    {")
        for start in range(0, len(bitmap), 16):
            lines.append("        " + ", ".join("0x{:02X}".format(value) for value in bitmap[start:start + 16]) + ",")
        lines.append("    },")

    lines.append("};")
    output_path.write_text("\n".join(lines) + "\n", encoding="ascii")
    print("Generated {} Chinese glyphs.".format(len(codepoints)))


if __name__ == "__main__":
    main()
