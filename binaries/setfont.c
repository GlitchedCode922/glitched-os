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

    PSF1_Header header;
    if (read(font_file, &header, sizeof(PSF1_Header)) != sizeof(PSF1_Header)) {
        printf("Error: Could not read font header\n");
        close(font_file);
        return 1;
    }
    if (header.magic != PSF1_FONT_MAGIC) {
        printf("Error: Invalid font file (magic number mismatch)\n");
        close(font_file);
        return 1;
    }

    font_t font;
    char glyphs[128][header.characterSize];
    font.width = 8;
    font.height = header.characterSize;

    for (int i = 0; i < 128; i++) {
        font.ascii[i] = glyphs[i];
    }

    if (read(font_file, glyphs, sizeof(glyphs)) != sizeof(glyphs)) {
        printf("Error: Invalid font file (file truncated)\n");
        close(font_file);
        return 1;
    }

    syscall(SYSCALL_SETFONT, (uint64_t)&font, 0, 0, 0, 0, 0);
    return 0;
}
