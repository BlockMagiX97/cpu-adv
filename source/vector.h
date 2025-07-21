#ifndef VECTOR_H
#define VECTOR_H

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define vector_of(T)                                                           \
	struct {                                                                   \
		int capacity;                                                          \
		int length;                                                            \
		T *data;                                                               \
	}

#define vector_init(vector)                                                    \
	({                                                                         \
		(vector)->capacity = 8;                                                \
		(vector)->length = 0;                                                  \
		(vector)->data = malloc(sizeof *(vector)->data * (vector)->capacity);  \
		(vector)->data != nullptr;                                             \
	})

#define vector_push(vector, elem)                                              \
	({                                                                         \
		if ((vector)->length == (vector)->capacity) {                          \
			(vector)->capacity =                                               \
				(vector)->capacity ? (vector)->capacity * 2 : 16;              \
			(vector)->data = realloc(                                          \
				(vector)->data, (vector)->capacity * sizeof(*(vector)->data)); \
		}                                                                      \
		(vector)->data[(vector)->length++] = (elem);                           \
	})

#define vector_concat(a, b)                                                    \
	({                                                                         \
		if ((a)->capacity < (a)->length + (b)->length) {                       \
			(a)->capacity = (a)->length + (b)->length;                         \
			(a)->data =                                                        \
				realloc((a)->data, (a)->capacity * sizeof(*(a)->data));        \
		}                                                                      \
		memcpy((a)->data + (a)->length, (b)->data,                             \
			   (b)->length * sizeof(*(a)->data));                              \
		(a)->length += (b)->length;                                            \
	})

#define vector_pop_back(vector) ({ (vector)->data[--(vector)->length]; })

#define vector_erase(vector, i)                                                \
	({                                                                         \
		assert((vector)->length);                                              \
		memmove((vector)->data + (i), (vector)->data + (i) + 1,                \
				((vector)->length - (i) - 1) * sizeof(*(vector)->data));       \
		(vector)->length--;                                                    \
	})

#define vector_get(vector, i) ({ (vector)->data[i]; })

#define vector_len(vector) ({ (vector)->length; })

#define vector_back(vector) ({ (vector)->data[(vector)->length - 1]; })

#define vector_empty(vector) ({ (vector)->length = 0; })

#define vector_realloc(vector, len)                                            \
	({                                                                         \
		if ((len) > (vector)->capacity) {                                      \
			(vector)->data =                                                   \
				realloc((vector)->data, (len) * sizeof(*(vector)->data));      \
			(vector)->capacity = (len);                                        \
		}                                                                      \
	})

#define vector_zero(vector)                                                    \
	({                                                                         \
		if ((vector)->length) {                                                \
			memset((vector)->data, 0,                                          \
				   (vector)->length * sizeof(*(vector)->data));                \
		}                                                                      \
	})

#define vector_clear(vector)                                                   \
	({                                                                         \
		if ((vector)->capacity) {                                              \
			free((vector)->data);                                              \
		}                                                                      \
		(vector)->length = 0;                                                  \
		(vector)->capacity = 0;                                                \
		(vector)->data = nullptr;                                              \
	})

#endif // VECTOR_H
