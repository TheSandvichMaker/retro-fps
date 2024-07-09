// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#pragma once

fn string_t os_get_working_directory(arena_t *arena);
fn bool os_set_working_directory(string_t directory);

#define push_working_directory(x) \
    for (string_t __wd = os_get_working_directory(m_get_temp(NULL, 0)); __wd.count && os_set_working_directory(x); (os_set_working_directory(__wd), __wd.count = 0))

fn bool os_execute(string_t command, int *exit_code);
fn bool os_execute_capture(string_t command, int *exit_code, arena_t *arena, string_t *out, string_t *err);
// TODO: async os_execute, ability to capture stderr/stdout, provide stdin

fn hires_time_t os_hires_time(void);
fn double os_seconds_elapsed(hires_time_t start, hires_time_t end);
fn uint64_t os_estimate_cpu_timer_frequency(uint64_t wait_ms);

fn void *vm_reserve (void *address, size_t size);
fn bool  vm_commit  (void *address, size_t size);
fn void  vm_decommit(void *address, size_t size);
fn void  vm_release (void *address);

fn void os_show_loud_error   (const char *fmt, ...);
fn void os_show_loud_error_va(const char *fmt, va_list args);
