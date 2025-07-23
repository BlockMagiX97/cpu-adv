#include <graphics.h>
#include <mmio.h>
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct core *gpu_cpu = nullptr;
static SDL_Window *gpu_window = nullptr;
static SDL_Renderer *gpu_renderer = nullptr;
static SDL_Texture *gpu_texture = nullptr;

static uint64_t gpu_width = GPU_DEFAULT_WIDTH;
static uint64_t gpu_height = GPU_DEFAULT_HEIGHT;
static uint64_t gpu_control = 0;

static uint8_t *fb_buffer = nullptr;
static size_t fb_size = 0;

static struct mmio_hook gpu_hook;

static bool realloc_framebuffer(uint64_t new_w, uint64_t new_h) {
	size_t new_size = (size_t)new_w * new_h * 4;
	uint8_t *new_buf = realloc(fb_buffer, new_size);
	if (!new_buf && new_size) {
		fprintf(stderr, "Failed to realloc framebuffer buffer\n");
		return false;
	}
	fb_buffer = new_buf;
	fb_size = new_size;
	memset(fb_buffer, 0, fb_size);

	if (gpu_texture) {
		SDL_DestroyTexture(gpu_texture);
	}
	gpu_texture = SDL_CreateTexture(gpu_renderer, SDL_PIXELFORMAT_ARGB8888,
									SDL_TEXTUREACCESS_STREAMING, new_w, new_h);
	if (!gpu_texture) {
		fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
		return false;
	}
	return true;
}

static bool gpu_mmio_read(struct core *, uintptr_t offset, void *buf,
						  size_t len) {
	if (offset < GPU_FB_BASE) {
		uint64_t val = 0;
		switch (offset) {
		case GPU_REG_WIDTH:
			val = gpu_width;
			break;
		case GPU_REG_HEIGHT:
			val = gpu_height;
			break;
		case GPU_REG_CONTROL:
			val = gpu_control;
			break;
		default:
			return false;
		}
		memcpy(buf, &val, len);
		return true;
	}

	if (offset + len > GPU_MMIO_SIZE)
		return false;
	size_t fb_off = offset - GPU_FB_BASE;
	if (fb_off + len > fb_size)
		return false;
	memcpy(buf, fb_buffer + fb_off, len);
	return true;
}

static bool gpu_mmio_write(struct core *, uintptr_t offset, const void *buf,
						   size_t len) {
	if (offset < GPU_FB_BASE) {
		uint64_t val = 0;
		memcpy(&val, buf, len);

		switch (offset) {
		case GPU_REG_WIDTH:
			gpu_width = val;
			return realloc_framebuffer(gpu_width, gpu_height);

		case GPU_REG_HEIGHT:
			gpu_height = val;
			return realloc_framebuffer(gpu_width, gpu_height);

		case GPU_REG_CONTROL:
			gpu_control = val;
			return true;

		default:
			return false;
		}
	}

	if (offset + len > GPU_MMIO_SIZE)
		return false;
	size_t fb_off = offset - GPU_FB_BASE;
	if (fb_off + len > fb_size)
		return false;
	memcpy(fb_buffer + fb_off, buf, len);
	return true;
}

int graphics_init(struct core *cpu) {
	gpu_cpu = cpu;

	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
		return -1;
	}

	gpu_window =
		SDL_CreateWindow("MMIO GPU", SDL_WINDOWPOS_UNDEFINED,
						 SDL_WINDOWPOS_UNDEFINED, gpu_width, gpu_height, 0);
	if (!gpu_window) {
		fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
		SDL_Quit();
		return -1;
	}
	SDL_ShowWindow(gpu_window);

	gpu_renderer = SDL_CreateRenderer(gpu_window, -1, SDL_RENDERER_ACCELERATED);
	if (!gpu_renderer) {
		fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
		SDL_DestroyWindow(gpu_window);
		SDL_Quit();
		return -1;
	}

	if (!realloc_framebuffer(gpu_width, gpu_height)) {
		SDL_DestroyRenderer(gpu_renderer);
		SDL_DestroyWindow(gpu_window);
		SDL_Quit();
		return -1;
	}


	gpu_hook.base = GPU_MMIO_BASE;
	gpu_hook.size = GPU_MMIO_SIZE;
	gpu_hook.read = gpu_mmio_read;
	gpu_hook.write = gpu_mmio_write;
	gpu_hook.next = nullptr;
	register_mmio_hook(&gpu_hook);

	return 0;
}

int graphics_step(SDL_Event *event) {
	SDL_Event local;
    int has_event = SDL_PollEvent(&local);
    if (has_event) {
        if (local.type == SDL_QUIT) {
            gpu_control &= ~GPU_CTRL_ENABLE;
        }
        if (event) {
            *event = local;
        }
    }

	if (!(gpu_control & GPU_CTRL_ENABLE))
		return has_event;
	uint64_t mode = (gpu_control & GPU_CTRL_MODE);
	if (mode != GPU_MODE_GRAPHICS && mode != GPU_MODE_GRAPHICS_CURSOR)
		return has_event;

	void *pixels;
	int pitch;
	if (SDL_LockTexture(gpu_texture, nullptr, &pixels, &pitch) != 0) {
		fprintf(stderr, "SDL_LockTexture: %s\n", SDL_GetError());
		return has_event;
	}

	if ((int)(gpu_width * 4) == pitch) {
		memcpy(pixels, fb_buffer, fb_size);
	} else {
		for (uint64_t y = 0; y < gpu_height; y++) {
			memcpy((uint8_t *)pixels + y * pitch, fb_buffer + y * gpu_width * 4,
				   gpu_width * 4);
		}
	}

	SDL_UnlockTexture(gpu_texture);
	SDL_RenderClear(gpu_renderer);
	SDL_RenderCopy(gpu_renderer, gpu_texture, nullptr, nullptr);
	SDL_RenderPresent(gpu_renderer);
    return has_event;
}

void graphics_shutdown(void) {
	unregister_mmio_hook(&gpu_hook);

	if (fb_buffer) {
		free(fb_buffer);
		fb_buffer = nullptr;
	}
	if (gpu_texture)
		SDL_DestroyTexture(gpu_texture);
	if (gpu_renderer)
		SDL_DestroyRenderer(gpu_renderer);
	if (gpu_window)
		SDL_DestroyWindow(gpu_window);
	SDL_Quit();
}
