from PIL import Image, ImageDraw, ImageFont

# ========== 配置区 ==========
FONT_SIZE = 48
CHAR_W = 48
CHAR_H = 48
BIN_OUT = "asc48.bin"
H_OUT = "asc48_map.h"
TXT_PATH = "commod.txt"

BYTES_PER_ROW = CHAR_W // 8
CHAR_BYTES = CHAR_H * BYTES_PER_ROW  # 48*6=288

# Windows黑体
try:
    font = ImageFont.truetype("simhei.ttf", FONT_SIZE)
except Exception as e:
    print("⚠️ 找不到simhei.ttf，使用默认字体，字形可能偏移")
    font = ImageFont.load_default()


def main():
    # 读取 commod.txt
    with open(TXT_PATH, "r", encoding="utf-8") as f:
        raw = f.read()

    # 去重，跳过空白换行
    char_list = []
    seen = set()
    for ch in raw:
        if not ch.isspace() and ch not in seen:
            seen.add(ch)
            char_list.append(ch)
    print(f"从commod.txt读取字符: {char_list}")
    print(f"一共 {len(char_list)} 个字符")

    bin_data = bytearray()

    # 渲染点阵
    for ch in char_list:
        img = Image.new("1", (CHAR_W, CHAR_H), 0)
        draw = ImageDraw.Draw(img)
        draw.text((0, 0), ch, font=font, fill=1)

        for y in range(CHAR_H):
            row_val = 0
            for x in range(CHAR_W):
                pix = img.getpixel((x, y))
                if pix:
                    row_val |= 1 << (CHAR_W - 1 - x)
            bin_data.extend(row_val.to_bytes(BYTES_PER_ROW, byteorder="big"))

    # 写出 bin
    with open(BIN_OUT, "wb") as fw:
        fw.write(bin_data)

    # 写出 C 头文件 asc48_map.h
    h_lines = []
    h_lines.append("#ifndef ASC48_MAP_H")
    h_lines.append("#define ASC48_MAP_H")
    h_lines.append("")
    h_lines.append(f"#define ASC48_CHAR_SIZE {CHAR_BYTES}")
    h_lines.append(f"#define ASC48_CHAR_COUNT {len(char_list)}")
    h_lines.append("")
    # 字符查找表
    h_lines.append("static const char asc48_chars[] = {")
    for c in char_list:
        h_lines.append(f"    '{c}',")
    h_lines.append("};")
    h_lines.append("")
    # 简单查找函数，输入字符返回索引，找不到返回 -1
    h_lines.append("static inline int asc48_find_idx(char c) {")
    h_lines.append("    for(int i=0; i<ASC48_CHAR_COUNT; i++){")
    h_lines.append("        if(asc48_chars[i] == c) return i;")
    h_lines.append("    }")
    h_lines.append("    return -1;")
    h_lines.append("}")
    h_lines.append("")
    h_lines.append("#endif")

    with open(H_OUT, "w", encoding="utf-8") as f:
        f.write("\n".join(h_lines))

    print(f"\n✅ 输出完成：")
    print(f"  {BIN_OUT}  点阵二进制")
    print(f"  {H_OUT}    C头映射表")
    print(f"  bin总大小: {len(bin_data)} bytes")


if __name__ == "__main__":
    main()
