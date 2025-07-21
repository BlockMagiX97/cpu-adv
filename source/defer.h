#ifndef DEFER_H
#define DEFER_H

#ifndef __clang__
#define DEFER_CONCAT_INNER(a, b) a##b
#define DEFER_CONCAT(a, b) DEFER_CONCAT_INNER(a, b)

#define DEFER_IMPL(N)                                                          \
	auto void DEFER_CONCAT(_defer_cleanup_, N)(int *);                         \
	__attribute__((cleanup(                                                    \
		DEFER_CONCAT(_defer_cleanup_, N)))) int DEFER_CONCAT(_defer_var_, N)   \
		__attribute__((unused));                                               \
	auto void DEFER_CONCAT(_defer_cleanup_, N)(int *)

#define defer DEFER_IMPL(__COUNTER__)
#else
#define defer
#endif

#endif // DEFER_H
