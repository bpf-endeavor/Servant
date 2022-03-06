#ifndef LABELS_H
#define LABELS_H
#define UNUSED __attribute__((unused))
#define OUT
#define IN
#define DEPRECATED

#define LIKELY(CONDITION) __builtin_expect(!!(CONDITION), 1)
#define UNLIKELY(CONDITION) __builtin_expect(!!(CONDITION), 0)
#endif
