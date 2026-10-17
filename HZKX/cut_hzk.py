# cut_hzk48.py 裁剪HZK48，生成精简字库 + 预览点阵
# 原版hzk48(GB2312)放到同目录
import sys

def char2gb2312(c: str) -> tuple[int, int]:
    b = c.encode("gb2312")
    q = b[0] - 0xA0
    w = b[1] - 0xA0
    return q, w

def get_hzk_offset(q: int, w: int) -> int:
    # HZK48 每个汉字 288 字节
    return ((q - 1) * 94 + (w - 1)) * 288

def render_bitmap(raw: bytes):
    # 打印48x48点阵预览
    for row in range(48):
        line = ""
        # 每行6字节(48bit)
        row_data = raw[row*6 : row*6+6]
        for byte in row_data:
            for i in range(8):
                line += "█" if (byte & (0x80 >> i)) else " "
        print(line)

def main():
    char_list = []
    with open("common.txt","r",encoding="utf-8") as f:
        txt = f.read()
        for ch in txt:
            if ch.strip() and ord(ch) > 0x80:
                char_list.append(ch)
    char_list = list(dict.fromkeys(char_list)) #去重
    print(f"待提取汉字数量：{len(char_list)}")

    src_hzk = open("hzk48","rb")
    out_bin = open("hzk48_min.bin","wb")
    idx = []

    for ch in char_list:
        try:
            q,w = char2gb2312(ch)
        except Exception as e:
            print(f"跳过无法转gb2312字符：{ch}")
            continue
        off = get_hzk_offset(q,w)
        src_hzk.seek(off)
        glyph = src_hzk.read(288)
        out_bin.write(glyph)
        idx.append((ch, q, w))
        print(f"【{ch}】区{q} 位{w}")
        # render_bitmap(glyph) #取消注释预览点阵

    src_hzk.close()
    out_bin.close()

    # 导出C头，直接嵌入64bit内核
    with open("hzk48_min.h","w",encoding="utf-8") as f:
        f.write("// Auto generated HZK48 mini font, 64bit kernel embed\n")
        f.write(f"#define FONT_CHAR_COUNT {len(idx)}\n")
        f.write("unsigned char hzk48_min_data[] = {\n")
        raw_all = open("hzk48_min.bin","rb").read()
        for b in raw_all:
            f.write(f"0x{b:02x},")
        f.write("\n};\n")
        f.write("typedef struct { char c; int q; int w; } FontIndex;\n")
        f.write("FontIndex font_index[] = {\n")
        for c,q,w in idx:
            f.write(f"{{'{c}', {q}, {w}}},\n")
        f.write("};\n")
    print("完成，输出 hzk48_min.bin 和 hzk48_min.h")

if __name__ == "__main__":
    main()
