import struct
import sys
import zlib
from pathlib import Path


LOGO_SIZE = 192


def decode_png(path):
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("Logo source is not a PNG image.")

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
                raise ValueError("Interlaced or unsupported PNG compression is not supported.")
        elif chunk_type == b"IDAT":
            compressed.extend(chunk)
        elif chunk_type == b"IEND":
            break

    if bit_depth != 8 or color_type != 6:
        raise ValueError("Logo PNG must be 8-bit RGBA.")

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
                raise ValueError("Unsupported PNG filter {}.".format(filter_type))

            row[index] = (row[index] + predictor) & 0xFF

        start = row_index * stride
        pixels[start:start + stride] = row
        previous_row = row

    if source_offset != len(decoded):
        raise ValueError("PNG image data has an unexpected size.")
    return width, height, pixels


def resample_alpha(width, height, pixels):
    result = bytearray(LOGO_SIZE * LOGO_SIZE)
    source_stride = width * 4

    for output_y in range(LOGO_SIZE):
        source_y_fixed = ((2 * output_y + 1) * height * 32768 // LOGO_SIZE) - 32768
        source_y = source_y_fixed // 65536
        fraction_y = source_y_fixed - source_y * 65536
        y0 = max(0, min(height - 1, source_y))
        y1 = max(0, min(height - 1, source_y + 1))

        for output_x in range(LOGO_SIZE):
            source_x_fixed = ((2 * output_x + 1) * width * 32768 // LOGO_SIZE) - 32768
            source_x = source_x_fixed // 65536
            fraction_x = source_x_fixed - source_x * 65536
            x0 = max(0, min(width - 1, source_x))
            x1 = max(0, min(width - 1, source_x + 1))

            top_left = pixels[y0 * source_stride + x0 * 4 + 3]
            top_right = pixels[y0 * source_stride + x1 * 4 + 3]
            bottom_left = pixels[y1 * source_stride + x0 * 4 + 3]
            bottom_right = pixels[y1 * source_stride + x1 * 4 + 3]

            top = top_left * (65536 - fraction_x) + top_right * fraction_x
            bottom = bottom_left * (65536 - fraction_x) + bottom_right * fraction_x
            alpha = (top * (65536 - fraction_y) + bottom * fraction_y +
                     (1 << 31)) >> 32
            result[output_y * LOGO_SIZE + output_x] = (alpha * 15 + 127) // 255

    return result


def encode_runs(pixels):
    runs = bytearray()
    start = 0
    while start < len(pixels):
        value = pixels[start]
        end = start + 1
        while end < len(pixels) and pixels[end] == value and end - start < 255:
            end += 1
        runs.extend((end - start, value))
        start = end
    return runs


def write_header(path, runs):
    lines = [
        "#define LOGO_WIDTH {}".format(LOGO_SIZE),
        "#define LOGO_HEIGHT {}".format(LOGO_SIZE),
        "static const uint8_t logo_alpha_rle[] = {",
    ]
    for start in range(0, len(runs), 16):
        lines.append("    " + ", ".join(
            "0x{:02X}".format(value) for value in runs[start:start + 16]) + ",")
    lines.extend(("};", ""))
    path.write_text("\n".join(lines), encoding="ascii")


def main():
    if len(sys.argv) != 3:
        raise SystemExit("Usage: generate_logo.py logo.png output.h")

    source_path, output_path = map(Path, sys.argv[1:])
    width, height, pixels = decode_png(source_path)
    logo = resample_alpha(width, height, pixels)
    runs = encode_runs(logo)
    write_header(output_path, runs)
    print("Generated {}x{} logo alpha mask ({} RLE bytes) from {}x{} PNG.".format(
        LOGO_SIZE, LOGO_SIZE, len(runs), width, height))


if __name__ == "__main__":
    main()
