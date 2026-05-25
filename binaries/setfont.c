#include <syscall.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

typedef struct {
    char* ascii[128];
    uint8_t width;
    uint8_t height;
} __attribute__((packed)) font_t;

#define PSF1_FONT_MAGIC 0x0436

typedef struct {
    uint16_t magic; // Magic bytes for identification.
    uint8_t fontMode; // PSF font mode.
    uint8_t characterSize; // PSF character size.
} PSF1_Header;

#define PSF2_FONT_MAGIC 0x864ab572

typedef struct {
    uint32_t magic; // Magic bytes for identification.
    uint32_t version; // Version number.
    uint32_t headerSize; // Offset of bitmaps in file, 32 for PSF2.
    uint32_t flags; // PSF font flags.
    uint32_t glyphCount; // Number of glyphs.
    uint32_t glyphSize; // Size of each glyph in bytes.
    uint32_t height; // Character height in pixels.
    uint32_t width; // Character width in pixels.
} PSF2_Header;

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: setfont <font_file>\n");
        return 1;
    }

    int font_file = open_file(argv[1], 0);
    if (!font_file) {
        printf("Error: Could not open font file '%s'\n", argv[1]);
        return 1;
    }

    char buffer[sizeof(PSF2_Header)];
    PSF1_Header* psf1_header = (PSF1_Header*)buffer;
    PSF2_Header* psf2_header = (PSF2_Header*)buffer;
    if (read(font_file, &buffer, sizeof(PSF2_Header)) < sizeof(PSF1_Header)) {
        printf("Error: Could not read font header\n");
        close(font_file);
        return 1;
    }

    font_t font;
    if (psf1_header->magic == PSF1_FONT_MAGIC) {
        seek(font_file, sizeof(PSF1_Header), SEEK_START);
        char glyphs[128][psf1_header->characterSize];
        font.width = 8;
        font.height = psf1_header->characterSize;

        for (int i = 0; i < 128; i++) {
            font.ascii[i] = glyphs[i];
        }

        if (read(font_file, glyphs, sizeof(glyphs)) != sizeof(glyphs)) {
            printf("Error: Invalid font file (file truncated)\n");
            close(font_file);
            return 1;
        }
        syscall(SYSCALL_SETFONT, (uint64_t)&font, 0, 0, 0, 0, 0);
    } else if (psf2_header->magic == PSF2_FONT_MAGIC) {
        seek(font_file, sizeof(PSF2_Header), SEEK_START);
        char glyphs[psf2_header->glyphCount][psf2_header->glyphSize];
        font.width = psf2_header->width;
        font.height = psf2_header->height;

        for (int i = 0; i < 128 && i < psf2_header->glyphCount; i++) {
            font.ascii[i] = glyphs[i];
        }

        if (read(font_file, glyphs, psf2_header->glyphCount * psf2_header->glyphSize) != psf2_header->glyphCount * psf2_header->glyphSize) {
            printf("Error: Invalid font file (file truncated)\n");
            close(font_file);
            return 1;
        }
        syscall(SYSCALL_SETFONT, (uint64_t)&font, 0, 0, 0, 0, 0);
    } else {
        printf("Error: Invalid font file (wrong magic)\n");
        close(font_file);
        return 1;
    }

    return 0;
}
