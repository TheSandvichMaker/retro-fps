// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#pragma once

#define array_expand(type, ...) ARRAY_COUNT(((type[]){ __VA_ARGS__ })), (type[]){ __VA_ARGS__ }

// linked list macros

#define sll_push(h, n)          do { (n)->next = (h); (h) = (n); } while (0)
#define sll_pop(h)         (h); do { (h) = (h)->next; } while (0)

#define sll_push_ex(h, n, next)      do { (n)->next = (h); (h) = (n); } while (0)
#define sll_pop_ex(h, next)     (h); do { (h) = (h)->next; } while (0)

#define sll_push_front(f, l, n) do { if (!(l)) { (f) = (l) = (n); } else { (n)->next = (f); (f) = (n); } } while (0)
#define sll_push_back(f, l, n)  do { if (!(f)) { (f) = (l) = (n); } else { (l) = (l)->next = (n); } } while (0)

#define sll_push_front_ex(f, l, n, next) do { if (!(l)) { (f) = (l) = (n); } else { (n)->next = (f); (f) = (n); } } while (0)
#define sll_push_back_ex(f, l, n, next)  do { if (!(f)) { (f) = (l) = (n); } else { (l) = (l)->next = (n); } } while (0)

// these macros would be easier to write if I used a sentinel for the doubly linked lists, but that has its own complications that I don't always want

#define dll_push_front(f, l, n) \
	do {                        \
		if (!(f))               \
		{                       \
			ASSERT(!(l));       \
			(f) = (l) = (n);    \
		}                       \
		else                    \
		{                       \
			(f)->prev = (n);    \
			(n)->next = (f);    \
			(n)->prev = NULL;   \
			(f) = (n);          \
		}                       \
	} while (0)

#define dll_push_back(f, l, n) \
	do {                       \
		if (!(f))              \
		{                      \
			ASSERT(!(l));      \
			(f) = (l) = (n);   \
		}                      \
		else                   \
		{                      \
			(n)->prev = (l);   \
			(n)->next = NULL;  \
			(l)->next = (n);   \
			(l) = (n);         \
		}                      \
	} while (0)

#define dll_insert_before(f, l, i, n)               \
    do {                                            \
        if ((i) == (f)) { (f) = (n); }              \
        if ((i)->prev) { (i)->prev->next = (n); }   \
        (n)->next = (i);                            \
        (n)->prev = (i)->prev;                      \
        (i)->prev = (n);                            \
    } while (0)

#define dll_insert_after(f, l, i, n)                \
    do {                                            \
        if ((i) == (l)) { (l) = (n); }              \
        if ((i)->next) { (i)->next->prev = (n); }   \
        (n)->next = (i)->next;                      \
        (n)->prev = (i);                            \
        (i)->next = (n);                            \
    } while (0)

#define dll_remove(f, l, n)                         \
    do {                                            \
        if ((n)->prev) (n)->prev->next = (n)->next; \
        if ((n)->next) (n)->next->prev = (n)->prev; \
        if ((n) == (f)) (f) = (n)->next;            \
        if ((n) == (l)) (l) = (n)->prev;            \
    } while (0)

#define sll_append(fd, ld, fs, ls) \
    do {                           \
        if (!(fd))                 \
        {                          \
            (fd) = (fs);           \
            (ld) = (ls);           \
        }                          \
        else                       \
        {                          \
            (ld)->next = (fs);     \
            (ld) = (ls);           \
        }                          \
    } while(0)

#define has_flags_any(field, flags) (!!((field) & (flags)))
#define has_flags_all(field, flags) (((field) & (flags)) == (flags))

fn void set_memory(void *memory, size_t size, char value);
#define zero_memory(memory, size) set_memory(memory, size, 0)
#define zero_struct(memory)       set_memory(memory, sizeof(*memory), 0)
#define zero_array(memory, count) set_memory(memory, (count)*sizeof(*memory), 0)

fn void copy_memory(void *dst, const void *src, size_t size);
#define copy_struct(dst, src)       do { void *__dst = dst; copy_memory(__dst, src, sizeof(*(dst))); } while(0)
#define copy_array(dst, src, count) copy_memory(dst, src, (count)*sizeof(*(dst)))

fn uintptr_t align_forward(uintptr_t address, uintptr_t align);
fn uintptr_t align_backward(uintptr_t address, uintptr_t align);
fn void *align_address(void *address, uintptr_t align);

#define for_array(it, arr) for (size_t it = 0; it < ARRAY_COUNT(arr); it++)
