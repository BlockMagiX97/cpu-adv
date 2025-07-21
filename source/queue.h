#ifndef QUEUE_H
#define QUEUE_H

#include <stdint.h>
#include <stdlib.h>

#define queue_of(T)                                                            \
	struct {                                                                   \
		uint32_t capacity;                                                     \
		uint32_t size;                                                         \
		T *elements;                                                           \
		int32_t back;                                                          \
		int32_t front;                                                         \
	}

#define queue_init(queue)                                                      \
	({                                                                         \
		(queue)->capacity = 8;                                                 \
		(queue)->size = 0;                                                     \
		(queue)->elements =                                                    \
			malloc(sizeof *(queue)->elements * (queue)->capacity);             \
		(queue)->front = -1;                                                   \
		(queue)->back = 0;                                                     \
		(queue)->elements != nullptr;                                          \
	})

#define queue_is_empty(queue) ((queue)->size == 0)

#define queue_push(queue, value)                                               \
	({                                                                         \
		if ((queue)->capacity == (queue)->size) {                              \
			(queue)->capacity *= 2;                                            \
			(queue)->elements =                                                \
				realloc((queue)->elements,                                     \
						sizeof *(queue)->elements * (queue)->capacity);        \
		}                                                                      \
		(queue)->elements[(queue)->back++] = value;                            \
		(queue)->size++;                                                       \
	})

#define queue_pop(queue)                                                       \
	({                                                                         \
		typeof((queue)->elements) ret = NULL;                                  \
		if (!queue_is_empty(queue)) {                                          \
			ret = &(queue)->elements[++(queue)->front];                        \
			(queue)->size--;                                                   \
		}                                                                      \
		ret;                                                                   \
	})

#define queue_peak(queue)                                                      \
	({                                                                         \
		typeof((queue)->elements) ret = NULL;                                  \
		if (!queue_is_empty(queue)) {                                          \
			ret = &(queue)->elements[(queue)->front + 1];                      \
		}                                                                      \
		ret;                                                                   \
	})

#endif // QUEUE_H
