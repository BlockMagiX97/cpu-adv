#ifndef RINGBUF_H
#define RINGBUF_H

#include <stdint.h>
#include <stdlib.h>

#define ringbuf_of(T, Max)                                                     \
	struct {                                                                   \
		T *buffer;                                                             \
		int32_t head;                                                          \
		int32_t tail;                                                          \
		struct {                                                               \
			char tag[(Max)];                                                   \
		} *_phantom;                                                           \
	}

#define ringbuf_capacity(rb) ((int32_t)sizeof(*(rb)->_phantom))

#define ringbuf_init(rb)                                                       \
	({                                                                         \
		size_t _cap = ringbuf_capacity(rb);                                    \
		(rb)->buffer = malloc(_cap * sizeof(*(rb)->buffer));                   \
		(rb)->head = 0;                                                        \
		(rb)->tail = 0;                                                        \
		(rb)->_phantom = (void *)1;                                            \
		(rb)->buffer != nullptr;                                               \
	})

#define ringbuf_deinit(rb)                                                     \
	do {                                                                       \
		free((rb)->buffer);                                                    \
		(rb)->buffer = nullptr;                                                \
		(rb)->_phantom = nullptr;                                              \
	} while (0)

#define ringbuf_full(rb) (((rb)->head + 1) % ringbuf_capacity(rb) == (rb)->tail)

#define ringbuf_empty(rb) ((rb)->head == (rb)->tail)

#define ringbuf_push(rb, val)                                                  \
	({                                                                         \
		bool _ok = !ringbuf_full(rb);                                          \
		if (_ok) {                                                             \
			(rb)->buffer[(rb)->head] = (val);                                  \
			(rb)->head = ((rb)->head + 1) % ringbuf_capacity(rb);              \
		}                                                                      \
		_ok;                                                                   \
	})

#define ringbuf_pop(rb, out)                                                   \
	({                                                                         \
		bool _ok = !ringbuf_empty(rb);                                         \
		if (_ok) {                                                             \
			*(out) = (rb)->buffer[(rb)->tail];                                 \
			(rb)->tail = ((rb)->tail + 1) % ringbuf_capacity(rb);              \
		}                                                                      \
		_ok;                                                                   \
	})

#endif // RINGBUF_H
