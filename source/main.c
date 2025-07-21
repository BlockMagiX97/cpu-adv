#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "cpu.h"
#include "paging.h"
#include "interrupt.h"
#include "inst.h"

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s test.bin\n", argv[0]);
        return 1;
    }

    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        perror("fopen");
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *bin = malloc(sz);
    if (!bin) {
        perror("malloc");
        return 1;
    }
    if (fread(bin, 1, sz, f) != (size_t)sz) {
        perror("fread");
        return 1;
    }
    fclose(f);

    struct ram *mem = init_memory(1024*1024);
    if (!mem) {
        fprintf(stderr, "init_memory failed\n");
        return 1;
    }
    struct core c;
    cpu_init(&c, mem);

    memcpy((uint8_t*)mem->mem, bin, sz);

    int max_steps = 1000000, steps = 0;
    while (cpu_step(&c)) {
        if (++steps > max_steps) {
            fprintf(stderr, "ERROR: timeout (no HLT after %d steps)\n", max_steps);
            return 1;
        }
    }

    bool ok = true;
    if (c.registers[R0] != 0x8) {
        fprintf(stderr, "FAIL: r0 (0x%016lx) != 0x8\n",
                c.registers[R0]);
        ok = false;
    }
    if (c.registers[R1] != 0x12) {
        fprintf(stderr, "FAIL: r1 (0x%016lx) != 0x12\n",
                c.registers[R1]);
        ok = false;
    }
    if (c.registers[R2] != (0x12 - 0x8)) {
        fprintf(stderr, "FAIL: r2 (0x%016lx) != 0x12-0x8\n",
                c.registers[R2]);
        ok = false;
    }
    if (c.registers[R3] != (0x5 + (0x12 - 0x8))) {
        fprintf(stderr, "FAIL: r3 (0x%016lx) != 0x5+(0x12-0x8)\n",
                c.registers[R3]);
        ok = false;
    }
    if (c.registers[R4] != ((0x8 + 0x12) - c.registers[R3])) {
        fprintf(stderr, "FAIL: r4 (0x%016lx) != (0x8+0x12)-r3\n",
                c.registers[R4]);
        ok = false;
    }
    if (c.registers[R5] != ((c.registers[R4] + c.registers[R2]) - c.registers[R1])) {
        fprintf(stderr, "FAIL: r5 (0x%016lx) != (r4+r2)-r1\n",
                c.registers[R5]);
        ok = false;
    }
    if (c.registers[R6] != c.registers[R5]) {
        fprintf(stderr, "FAIL: r6 (0x%016lx) != r5\n",
                c.registers[R6]);
        ok = false;
    }
    if (c.registers[R7] != (c.registers[R0] + c.registers[R6])) {
        fprintf(stderr, "FAIL: r7 (0x%016lx) != r0+r6\n",
                c.registers[R7]);
        ok = false;
    }
    if (c.registers[R8] != 0x12) {
        fprintf(stderr, "FAIL: r8 (0x%016lx) != 0x12\n",
                c.registers[R8]);
        ok = false;
    }
    if (c.registers[R9] != 0) {
        fprintf(stderr, "FAIL: r9 (0x%016lx) != 0\n",
                c.registers[R9]);
        ok = false;
    }
    if (c.registers[R10] != 0x12) {
        fprintf(stderr, "FAIL: r10 (0x%016lx) != 0x12\n",
                c.registers[R10]);
        ok = false;
    }
    if (c.registers[R11] != 0) {
        fprintf(stderr, "FAIL: r11 (0x%016lx) != 0\n",
                c.registers[R11]);
        ok = false;
    }
    if (c.registers[R12] != 0x42) {
        fprintf(stderr, "FAIL: r12 (0x%016lx) != 0x42\n",
                c.registers[R12]);
        ok = false;
    }
    if (c.registers[R13] != 0) {
        fprintf(stderr, "FAIL: r13 (0x%016lx) != 0\n",
                c.registers[R13]);
        ok = false;
    }

    if (!ok) {
        return 1;
    }

    printf("ALL TESTS PASSED\n");
    return 0;
}
