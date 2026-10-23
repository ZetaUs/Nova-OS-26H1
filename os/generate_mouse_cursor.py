import struct
import sys
import zlib
from pathlib import Path

CURSOR_WIDTH = 48
CURSOR_HEIGHT = 60


def decode_png(path):
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("Mouse cursor source is not a PNG image.")

    offset = 8
    compressed = bytearray()
    width = height = bit_depth = color_type = None
    while offset < len(data):
        length = struct.unpack_from(">I", data, offset)[0]
        chunk_type = data[offset + 4:offset + 8]
        chunk = data[offset + 8:offset + 8 + length]
        offset += length + 12

        if chunk_type == b"IHDR":
            width, height, bit_depth, color_type, compression, filtering, interlace = (
                struct.unpack(">IIBBBBB", chunk)
            )
            if compression != 0 or filtering != 0 or interlace != 0:
                raise ValueError("Unsupported PNG encoding in mouse cursor.")
        elif chunk_type == b"IDAT":
            compressed.extend(chunk)
        elif chunk_type == b"IEND":
            break

    if bit_depth != 8 or color_type != 6:
        raise ValueError("Mouse cursor PNG must be 8-bit RGBA.")

    stride = width * 4
    decoded = zlib.decompress(compressed)
    pixels = bytearray(height * stride)
    source_offset = 0
    previous_row = bytearray(stride)

    for row_index in range(height):
        filter_type = decoded[source_offset]
        source_offset += 1
        row = bytearray(decoded[source_offset:source_offset + stride])
        source_offset += stride

        for index in range(stride):
            left = row[index - 4] if index >= 4 else 0
            above = previous_row[index]
            upper_left = previous_row[index - 4] if index >= 4 else 0

            if filter_type == 1:
                predictor = left
            elif filter_type == 2:
                predictor = above
            elif filter_type == 3:
                predictor = (left + above) // 2
            elif filter_type == 4:
                estimate = left + above - upper_left
                distances = (
                    abs(estimate - left),
                    abs(estimate - above),
                    abs(estimate - upper_left),
                )
                predictor = (left, above, upper_left)[distances.index(min(distances))]
            elif filter_type == 0:
                predictor = 0
            else:
                raise ValueError(f"Unsupported PNG filter {filter_type}.")

            row[index] = (row[index] + predictor) & 0xFF

        start = row_index * stride
        pixels[start:start + stride] = row
        previous_row = row

    return width, height, pixels


def resample_alpha(width, height, pixels):
    result = [0] * (CURSOR_WIDTH * CURSOR_HEIGHT)
    source_stride = width * 4

    for output_y in range(CURSOR_HEIGHT):
        source_y_fixed = ((2 * output_y + 1) * height * 32768 // CURSOR_HEIGHT) - 32768
        source_y = source_y_fixed // 65536
        fraction_y = source_y_fixed - source_y * 65536
        y0 = max(0, min(height - 1, source_y))
        y1 = max(0, min(height - 1, source_y + 1))

        for output_x in range(CURSOR_WIDTH):
            source_x_fixed = ((2 * output_x + 1) * width * 32768 // CURSOR_WIDTH) - 32768
            source_x = source_x_fixed // 65536
            fraction_x = source_x_fixed - source_x * 65536
            x0 = max(0, min(width - 1, source_x))
            x1 = max(0, min(width - 1, source_x + 1))

            def sample_alpha(y_index, x_index):
                return pixels[y_index * source_stride + x_index * 4 + 3]

            top_left = sample_alpha(y0, x0)
            top_right = sample_alpha(y0, x1)
            bottom_left = sample_alpha(y1, x0)
            bottom_right = sample_alpha(y1, x1)

            def lerp(a, b, frac):
                return (a * (65536 - frac) + b * frac + (1 << 31)) >> 32

            top_alpha = lerp(top_left, top_right, fraction_x)
            bottom_alpha = lerp(bottom_left, bottom_right, fraction_x)
            final_alpha = (top_alpha * (65536 - fraction_y) + bottom_alpha * fraction_y + (1 << 31)) >> 32
            result[output_y * CURSOR_WIDTH + output_x] = min(255, max(0, final_alpha >> 8))

    return result


def write_header(path, alpha):
    lines = [
        "#define MOUSE_CURSOR_WIDTH {}".format(CURSOR_WIDTH),
        "#define MOUSE_CURSOR_HEIGHT {}".format(CURSOR_HEIGHT),
        "static const uint8_t mouse_cursor_alpha[] = {",
    ]
    for start in range(0, len(alpha), 16):
        lines.append("    " + ", ".join("0x{:02X}".format(value) for value in alpha[start:start + 16]) + ",")
    lines.extend(("};", ""))
    path.write_text("\n".join(lines), encoding="ascii")


def main():
    if len(sys.argv) != 3:
        raise SystemExit("Usage: generate_mouse_cursor.py mouse.png output.h")

    source_path, output_path = map(Path, sys.argv[1:])
    width, height, pixels = decode_png(source_path)
    alpha = resample_alpha(width, height, pixels)
    write_header(output_path, alpha)
    print(f"Generated {CURSOR_WIDTH}x{CURSOR_HEIGHT} mouse cursor alpha mask from {width}x{height} PNG.")


if __name__ == "__main__":
    main()
