#ifndef HASHMAP_H
#define HASHMAP_H

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define map_of(K, V)                                                           \
	struct {                                                                   \
		size_t capacity;                                                       \
		size_t size;                                                           \
		K *keys;                                                               \
		V *values;                                                             \
		char *states;                                                          \
		size_t (*hash)(K);                                                     \
		int (*eq)(K, K);                                                       \
	}

#define map_init(map, hashfunc, eqfunc)                                        \
	do {                                                                       \
		(map)->capacity = 16;                                                  \
		(map)->size = 0;                                                       \
		(map)->keys = malloc((map)->capacity * sizeof(*(map)->keys));          \
		(map)->values = malloc((map)->capacity * sizeof(*(map)->values));      \
		(map)->states = calloc((map)->capacity, sizeof(*(map)->states));       \
		(map)->hash = hashfunc;                                                \
		(map)->eq = eqfunc;                                                    \
		(map)->keys != nullptr && (map)->values != nullptr &&                  \
			(map)->states != nullptr;                                          \
	} while (0)

#define map_clear(map)                                                         \
	do {                                                                       \
		free((map)->keys);                                                     \
		free((map)->values);                                                   \
		free((map)->states);                                                   \
		(map)->capacity = 0;                                                   \
		(map)->size = 0;                                                       \
		(map)->keys = nullptr;                                                 \
		(map)->values = nullptr;                                               \
		(map)->states = nullptr;                                               \
		(map)->hash = nullptr;                                                 \
		(map)->eq = nullptr;                                                   \
	} while (0)

#define map_size(map) ((map)->size)
#define map_capacity(map) ((map)->capacity)

#define map_put(map, key, value)                                               \
	do {                                                                       \
                                                                               \
		if ((map)->size * 2 >= (map)->capacity) {                              \
			size_t _new_cap = (map)->capacity ? (map)->capacity * 2 : 16;      \
			typeof(*(map)->keys) *_old_keys = (map)->keys;                     \
			typeof(*(map)->values) *_old_values = (map)->values;               \
			char *_old_states = (map)->states;                                 \
			size_t _old_cap = (map)->capacity;                                 \
                                                                               \
			(map)->capacity = _new_cap;                                        \
			(map)->size = 0;                                                   \
			(map)->keys = malloc(_new_cap * sizeof(*(map)->keys));             \
			(map)->values = malloc(_new_cap * sizeof(*(map)->values));         \
			(map)->states = calloc(_new_cap, sizeof(*(map)->states));          \
                                                                               \
			for (size_t _i = 0; _i < _old_cap; ++_i)                           \
				if (_old_states[_i] == 1) {                                    \
					typeof(*(map)->keys) _k = _old_keys[_i];                   \
					typeof(*(map)->values) _v = _old_values[_i];               \
					size_t _h = (map)->hash(_k) & (_new_cap - 1);              \
					while ((map)->states[_h] == 1)                             \
						_h = (_h + 1) & (_new_cap - 1);                        \
					(map)->states[_h] = 1;                                     \
					(map)->keys[_h] = _k;                                      \
					(map)->values[_h] = _v;                                    \
					++(map)->size;                                             \
				}                                                              \
			free(_old_keys);                                                   \
			free(_old_values);                                                 \
			free(_old_states);                                                 \
		}                                                                      \
                                                                               \
		size_t _h = (map)->hash(key) & ((map)->capacity - 1);                  \
		while ((map)->states[_h] == 1 && !(map)->eq((map)->keys[_h], key)) {   \
			_h = (_h + 1) & ((map)->capacity - 1);                             \
		}                                                                      \
		if ((map)->states[_h] != 1) {                                          \
			(map)->states[_h] = 1;                                             \
			++(map)->size;                                                     \
		}                                                                      \
		(map)->keys[_h] = (key);                                               \
		(map)->values[_h] = (value);                                           \
	} while (0)

#define map_get(map, key)                                                      \
	({                                                                         \
		typeof(&(map)->values[0]) _r = nullptr;                                \
		if ((map)->capacity) {                                                 \
			size_t _h = (map)->hash(key) & ((map)->capacity - 1);              \
			while ((map)->states[_h] != 0) {                                   \
				if ((map)->states[_h] == 1 &&                                  \
					(map)->eq((map)->keys[_h], key)) {                         \
					_r = &(map)->values[_h];                                   \
					break;                                                     \
				}                                                              \
				_h = (_h + 1) & ((map)->capacity - 1);                         \
			}                                                                  \
		}                                                                      \
		_r;                                                                    \
	})

#define map_has(map, key) (map_get(map, key) != nullptr)

#define map_remove(map, key)                                                   \
	do {                                                                       \
		if ((map)->capacity) {                                                 \
			size_t _h = (map)->hash(key) & ((map)->capacity - 1);              \
			while ((map)->states[_h] != 0) {                                   \
				if ((map)->states[_h] == 1 &&                                  \
					(map)->eq((map)->keys[_h], key)) {                         \
					(map)->states[_h] = 2;                                     \
					--(map)->size;                                             \
					break;                                                     \
				}                                                              \
				_h = (_h + 1) & ((map)->capacity - 1);                         \
			}                                                                  \
		}                                                                      \
	} while (0)

#endif // HASHMAP_H
