#!/usr/bin/env python3
import struct
import sys

CONTROL_CHARS = {
    0x00: "NUL",
    0x01: "SOH",
    0x02: "STX",
    0x03: "ETX",
    0x04: "EOT",
    0x05: "ENQ",
    0x06: "ACK",
    0x07: "BEL",
    0x08: "BS",
    0x09: "TAB",
    0x0A: "LF",
    0x0B: "VT",
    0x0C: "FF",
    0x0D: "CR",
    0x0E: "SO",
    0x0F: "SI",
    0x10: "DLE",
    0x11: "DC1",
    0x12: "DC2",
    0x13: "DC3",
    0x14: "DC4",
    0x15: "NAK",
    0x16: "SYN",
    0x17: "ETB",
    0x18: "CAN",
    0x19: "EM",
    0x1A: "SUB",
    0x1B: "ESC",
    0x1C: "FS",
    0x1D: "GS",
    0x1E: "RS",
    0x1F: "US",
    0x7F: "DEL",
}

def ascii_description(index):
    if index in CONTROL_CHARS:
        return CONTROL_CHARS[index]
    if 0x20 <= index <= 0x7E:
        return repr(chr(index))
    return ""

def parse_psf(data):
    if data[:2] == b"\x36\x04":
        return parse_psf1(data)
    if len(data) >= 4 and struct.unpack_from("<I", data, 0)[0] == 0x864AB572:
        return parse_psf2(data)
    raise ValueError("unknown PSF format")

def parse_psf1(data):
    if len(data) < 4:
        raise ValueError("invalid PSF1 file")

    mode = data[2]
    charsize = data[3]
    glyphs = 512 if mode & 0x01 else 256
    width = 8
    height = charsize
    glyph_size = charsize
    offset = 4

    required = offset + glyphs * glyph_size
    if len(data) < required:
        raise ValueError("truncated PSF1 file")

    return width, height, glyphs, glyph_size, data[offset:required]

def parse_psf2(data):
    if len(data) < 32:
        raise ValueError("invalid PSF2 file")

    magic, version, header_size, flags, glyphs, glyph_size, height, width = \
        struct.unpack_from("<8I", data, 0)

    if magic != 0x864AB572:
        raise ValueError("invalid PSF2 magic")

    if header_size > len(data):
        raise ValueError("invalid PSF2 header size")

    row_bytes = (width + 7) // 8
    expected_size = row_bytes * height

    if glyph_size < expected_size:
        raise ValueError("PSF2 glyph size is too small")

    required = header_size + glyphs * glyph_size
    if len(data) < required:
        raise ValueError("truncated PSF2 file")

    return width, height, glyphs, glyph_size, data[header_size:required]

def bitmap_comment(row, width):
    return "".join(
        "#" if row & (1 << (width - 1 - x)) else "."
        for x in range(width)
    )

def generate_header(width, height, glyphs, glyph_size, glyph_data):
    row_bytes = (width + 7) // 8
    lines = [
        "#pragma once",
        "#include <stdint.h>",
        "",
        f"#define DEFAULT_FONT_WIDTH {width}",
        f"#define DEFAULT_FONT_HEIGHT {height}",
        "#define DEFAULT_FONT_GLYPHS 128",
        f"#define DEFAULT_FONT_ROW_BYTES {row_bytes}",
        f"#define DEFAULT_FONT_GLYPH_SIZE {glyph_size}",
        "",
        "uint8_t default_font[DEFAULT_FONT_GLYPHS][DEFAULT_FONT_GLYPH_SIZE] = {",
    ]

    for index in range(128):
        description = ascii_description(index)
        comment = f" /* 0x{index:02X} {description} */" if description else f" /* 0x{index:02X} */"
        lines.append(f"    {{{comment}")

        glyph = glyph_data[index * glyph_size:(index + 1) * glyph_size]

        for y in range(height):
            row_offset = y * row_bytes
            row = glyph[row_offset:row_offset + row_bytes]

            if row_bytes == 1:
                value = row[0]
                visual = bitmap_comment(value, width)
                lines.append(f"        0x{value:02X}, /* {visual} */")
            else:
                values = ", ".join(f"0x{byte:02X}" for byte in row)
                bits = "".join(
                    bitmap_comment(byte, 8)
                    for byte in row
                )
                lines.append(f"        {values}, /* {bits} */")

        lines.append("    },")

    lines.append("};")
    lines.append("")
    return "\n".join(lines)

def main():
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} input.psf output.h")
        sys.exit(1)

    input_path = sys.argv[1]
    output_path = sys.argv[2]

    try:
        with open(input_path, "rb") as f:
            data = f.read()

        width, height, glyphs, glyph_size, glyph_data = parse_psf(data)

        if glyphs < 128:
            raise ValueError("font contains fewer than 128 glyphs")

        header = generate_header(
            width,
            height,
            glyphs,
            glyph_size,
            glyph_data,
        )

        with open(output_path, "w", newline="\n") as f:
            f.write(header)

        print(f"wrote {output_path}")

    except (OSError, ValueError) as e:
        print(f"error: {e}", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
