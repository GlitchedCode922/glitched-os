#include <stdio.h>
#include <stdint.h>

/* Manual string-to-int decoder (base 10 only) */
int manual_parse(const char *s) {
    int result = 0;
    int negative = 0;

    if (*s == '-') {
        negative = 1;
        s++;
    }

    while (*s) {
        if (*s < '0' || *s > '9') {
            printf("Invalid numeric input.\n");
            return -1;
        }

        result = result * 10 + (*s - '0');
        s++;
    }

    return negative ? -result : result;
}

void trigger_exception(int vector) {
    switch (vector) {

        case 0: {  // Divide by zero
            volatile int x = 1;
            volatile int y = 0;
            volatile int z = x / y;
            (void)z;
            break;
        }

        case 3: {  // Breakpoint
            __asm__("int3");
            break;
        }

        case 6: {  // Invalid opcode
            __asm__(".byte 0x0f, 0x0b"); // UD2
            break;
        }

        case 13: { // General protection fault
            __asm__(
                "mov $0xdeadbeef, %rax\n"
                "mov %rax, %cr3\n"
            );
            break;
        }

        case 14: { // Page fault
            volatile int *ptr = (int*)0x0;
            *ptr = 42;
            break;
        }

        default:
            printf("Vector %d not implemented.\n", vector);
    }
}

int main(int argc, char *argv[]) {

    if (argc < 2) {
        printf("Usage: %s <interrupt_vector>\n", argv[0]);
        return 1;
    }

    int vector = manual_parse(argv[1]);

    if (vector < 0 || vector >= 32) {
        printf("Invalid vector.\n");
        return 1;
    }

    printf("Triggering interrupt vector %d...\n", vector);

    trigger_exception(vector);

    return 0;
}
