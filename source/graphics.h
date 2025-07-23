// graphics.h

#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <SDL2/SDL.h>
#include <cpu.h>
#include <paging.h>
#include <stdint.h>

#define GPU_MMIO_BASE 0x3FFE0000ULL

#define GPU_DEFAULT_WIDTH 640
#define GPU_DEFAULT_HEIGHT 480

#define GPU_REG_WIDTH 0x00
#define GPU_REG_HEIGHT 0x08
#define GPU_REG_CONTROL 0x10
#define GPU_FB_BASE 0x20

#define GPU_CTRL_ENABLE 0x1
#define GPU_CTRL_HARDWARE_CURSOR 0x2
#define GPU_CTRL_MODE 0x4 // .. 0x8 two bits

#define GPU_MODE_TEXT 0
#define GPU_MODE_TEXT_NO_CURSOR 1
#define GPU_MODE_GRAPHICS 2
#define GPU_MODE_GRAPHICS_CURSOR 3

#define GPU_FB_SIZE (GPU_DEFAULT_WIDTH * GPU_DEFAULT_HEIGHT * 4)
#define GPU_MMIO_SIZE (GPU_FB_BASE + GPU_FB_SIZE)

int graphics_init(struct core *cpu);
int graphics_step(SDL_Event *event);
void graphics_shutdown(void);

#endif // GRAPHICS_H
