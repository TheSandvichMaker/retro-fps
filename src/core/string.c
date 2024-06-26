// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#define STB_SPRINTF_IMPLEMENTATION
#include <stb_sprintf.h>

#define XXH_INLINE_ALL
#include "xxhash/xxhash.h"

size_t string_count(const char *string)
{
    size_t result = 0;
    for (const char *c = string; *c; c++) result++;
    return result;
}

size_t string16_count(const wchar_t *string)
{
    size_t result = 0;
    for (const wchar_t *c = string; *c; c++) result++;
    return result;
}

bool string_empty(string_t string)
{
	return string.count == 0 || !string.data;
}

null_term_string_t string_from_cstr(char *string)
{
    string_t result;
    result.count = string_count(string);
    result.data  = string;
    return result;
}

null_term_string16_t string16_from_cstr(wchar_t *string)
{
    string16_t result;
    result.count = string16_count(string);
    result.data  = string;
    return result;
}

string_t string_copy_cstr(arena_t *arena, const char *string)
{
    size_t count = string_count(string);
    char  *data  = m_alloc_string(arena, count);

    if (ALWAYS(data))
    {
        copy_memory(data, string, count);
    }

    return (string_t){ data, count };
}

string_t string_null_terminate(arena_t *arena, string_t string)
{
    char *data = m_alloc_string(arena, string.count + 1);

    if (ALWAYS(data))
    {
        copy_memory(data, string.data, string.count);    
        data[string.count] = 0;
    }

    return (string_t){ data, string.count };
}

string16_t string16_null_terminate(arena_t *arena, string16_t string)
{
    wchar_t *data = m_alloc_string16(arena, string.count + 1);

    if (ALWAYS(data))
    {
        copy_memory(data, string.data, sizeof(wchar_t)*string.count);    
        data[string.count] = 0;
    }

    return (string16_t){ data, string.count };
}

string_t string_format(arena_t *arena, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    string_t result = string_format_va(arena, fmt, args);

    va_end(args);

    return result;
}

string_t string_format_va(arena_t *arena, const char *fmt, va_list args)
{
    va_list args2;
    va_copy(args2, args);

    int len = stbsp_vsnprintf(NULL, 0, fmt, args2);

    va_end(args2);

    char *data = m_alloc_string(arena, (size_t)len + 1);
    stbsp_vsnprintf(data, (size_t)len + 1, fmt, args);

    string_t result = {
        .count = (size_t)len,
        .data  = data,
    };

    return result;
}

string_t string_format_into_buffer(char *buffer, size_t size, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    string_t result = string_format_into_buffer_va(buffer, size, fmt, args);

    va_end(args);

    return result;
}

string_t string_format_into_buffer_va(char *buffer, size_t size, const char *fmt, va_list args)
{
    if (NEVER(size > INT_MAX))  size = INT_MAX;
    int len = stbsp_vsnprintf(buffer, (int)size, fmt, args);

    string_t result = {
        .data  = buffer,
        .count = (size_t)len,
    };

    return result;
}

string_t substring(string_t string, size_t first, size_t count)
{
    first = MIN(first, string.count);
    count = MIN(string.count - first, count);

    return (string_t) {
        .data  = string.data + first,
        .count = count,
    };
}

size_t string_find_char_first(string_t string, char c)
{
    for (size_t i = 0; i < string.count; i++)
    {
        if (string.data[i] == c)
        {
            return i;
        }
    }
    return STRING_NPOS;
}

size_t string_find_char_last(string_t string, char c)
{
    size_t result = STRING_NPOS;
    for (size_t i = 0; i < string.count; i++)
    {
        if (string.data[i] == c)
        {
            result = i;
        }
    }
    return result;
}

size_t string_find_first(string_t string, string_t needle)
{
    if (needle.count > string.count)  return STRING_NPOS;

    for (size_t i = 0; i < string.count - needle.count; i++)
    {
        string_t sub = substring(string, i, needle.count);
        if (string_match(sub, needle))
        {
            return i;
        }
    }

    return STRING_NPOS;
}

string_t string_extension(string_t string)
{
    string_t result = { 0 };

    size_t pos = string_find_char_last(string, '.');
    if (pos != STRING_NPOS)
    {
        result = substring(string, pos, STRING_NPOS);
    }

    return result;
}

string_t string_strip_extension(string_t string)
{
    size_t pos = string_find_char_last(string, '.');
    return substring(string, 0, pos);
}

string_t string_normalize_path(arena_t *arena, string_t path)
{
	// TODO: Deduplicate separators as well
	string_t result = {
		.data  = m_alloc_string(arena, path.count),
		.count = path.count,
	};

	for (size_t i = 0; i < path.count; i++)
	{
		if (path.data[i] == '\\')
		{
			result.data[i] = '/';
		}
		else
		{
			result.data[i] = path.data[i];
		}
	}

	return result;
}

string_t string_path_leaf(string_t path)
{
    size_t leaf_start = 0;
    for (size_t i = 0; i < path.count; i++)
    {
        if (path.data[i] == '/' ||
            path.data[i] == '\\')
        {
            leaf_start = i + 1;
        }
    }

    return substring(path, leaf_start, STRING_NPOS);
}

string_t string_path_directory(string_t path)
{
    size_t leaf_start = 0;
    for (size_t i = 0; i < path.count; i++)
    {
        if (path.data[i] == '/' ||
            path.data[i] == '\\')
        {
            leaf_start = i;
        }
    }

    string_t result = substring(path, 0, leaf_start);
    return result;
}

bool string_path_strip_root(string_t path, string_t *out_root, string_t *out_remainder)
{
    size_t root_end = STRING_NPOS;
    for (size_t i = 0; i < path.count; i++)
    {
        if (path.data[i] == '/' ||
            path.data[i] == '\\')
        {
            root_end = i;
            break;
        }
    }

    if (out_root)
    {
        *out_root = substring(path, 0, root_end);
    }

    if (out_remainder)
    {
        *out_remainder = substring(path, root_end + 1, STRING_NPOS) ;
    }

    return root_end != STRING_NPOS;
}

bool string_match(string_t a, string_t b)
{
    if (a.count != b.count)  return false;

    for (size_t i = 0; i < a.count; i++)
    {
        if (a.data[i] != b.data[i])  
        {
            return false;
        }
    }

    return true;
}

bool string_match_nocase(string_t a, string_t b)
{
    if (a.count != b.count)  return false;

    for (size_t i = 0; i < a.count; i++)
    {
        if (to_lower(a.data[i]) != to_lower(b.data[i]))  
        {
            return false;
        }
    }

    return true;
}

bool string_match_prefix(string_t string, string_t prefix)
{
    return string_match(substring(string, 0, prefix.count), prefix);
}

int string_compare(string_t a, string_t b, string_match_flags_t flags)
{
	int diff = 0;

    for (size_t i = 0; i < MIN(a.count, b.count); i++)
    {
		char char_a = a.data[i];
		char char_b = b.data[i];

		if (flags & StringMatch_case_insensitive)
		{
			char_a = to_lower(char_a);
			char_b = to_lower(char_b);
		}

		diff = char_a - char_b;

		if (diff)
		{
			break;
		}
    }

	if (diff == 0 && a.count != b.count)
	{
		diff = a.count < b.count ? -1 : 1;
	}

    return diff;
}

void string_skip(string_t *string, size_t count)
{
    if (count > string->count) count = string->count;
    string->count -= count;
    string->data  += count;
}

string_t string_split(string_t *string, string_t separator)
{
    string_t result = *string;

    size_t at = string_find_first(*string, separator);
    if (at != STRING_NPOS)
    {
        result = substring(*string, 0, at);
        string_skip(string, at + separator.count);
    }
    else
    {
        string_skip(string, result.count);
    }

    return result;
}

string_t string_to_lower(arena_t *arena, string_t string)
{
	string_t result = {
		.data  = m_alloc_nozero(arena, string.count, 16),
		.count = string.count,
	};

	for (size_t i = 0; i < string.count; i++)
	{
		result.data[i] = to_lower(string.data[i]);
	}

	return result;
}

string_t string_trim_left_spaces(string_t string)
{
    while (string.count > 0 && is_whitespace(string.data[0])) string.count -= 1, string.data += 1;
    return string;
}

string_t string_trim_right_spaces(string_t string)
{
    while (string.count > 0 && is_whitespace(string.data[string.count - 1])) string.count -= 1;
    return string;
}

string_t string_trim_spaces(string_t string)
{
    string = string_trim_left_spaces(string);
    string = string_trim_right_spaces(string);
    return string;
}

size_t string_count_newlines(string_t string)
{
	size_t result = 0;

	for (size_t i = 0; i < string.count; i++)
	{
		if (string.data[i] == '\n') result++;
	}

	return result;
}

string_t string_split_word(string_t *string)
{
    string_t result = *string;

    for (size_t i = 0; i < string->count; i++)
    {
        if (string->data[i] == ' '  ||
            string->data[i] == '\n' ||
            string->data[i] == '\r' ||
            string->data[i] == '\t')
        {
            result = substring(*string, 0, i);
            string_skip(string, i + 1);

            break;
        }
    }

    return string_trim_left_spaces(result);
}

string_t string_split_line(string_t *string)
{
    return string_split(string, S("\n"));
}

char to_lowercase_table[256] = {
    [0]   = 0,   [1]   = 1,   [2]   = 2,   [3]   = 3,   [4]   = 4,   [5]   = 5,   [6]   = 6,   [7]   = 7,
    [8]   = 8,   [9]   = 9,   [10]  = 10,  [11]  = 11,  [12]  = 12,  [13]  = 13,  [14]  = 14,  [15]  = 15,
    [16]  = 16,  [17]  = 17,  [18]  = 18,  [19]  = 19,  [20]  = 20,  [21]  = 21,  [22]  = 22,  [23]  = 23,
    [24]  = 24,  [25]  = 25,  [26]  = 26,  [27]  = 27,  [28]  = 28,  [29]  = 29,  [30]  = 30,  [31]  = 31,
    [32]  = 32,  [33]  = 33,  [34]  = 34,  [35]  = 35,  [36]  = 36,  [37]  = 37,  [38]  = 38,  [39]  = 39,
    [40]  = 40,  [41]  = 41,  [42]  = 42,  [43]  = 43,  [44]  = 44,  [45]  = 45,  [46]  = 46,  [47]  = 47,
    [48]  = 48,  [49]  = 49,  [50]  = 50,  [51]  = 51,  [52]  = 52,  [53]  = 53,  [54]  = 54,  [55]  = 55,
    [56]  = 56,  [57]  = 57,  [58]  = 58,  [59]  = 59,  [60]  = 60,  [61]  = 61,  [62]  = 62,  [63]  = 63,
    [64]  = 64,  [65]  = 97,  [66]  = 98,  [67]  = 99,  [68]  = 100, [69]  = 101, [70]  = 102, [71]  = 103,
    [72]  = 104, [73]  = 105, [74]  = 106, [75]  = 107, [76]  = 108, [77]  = 109, [78]  = 110, [79]  = 111,
    [80]  = 112, [81]  = 113, [82]  = 114, [83]  = 115, [84]  = 116, [85]  = 117, [86]  = 118, [87]  = 119,
    [88]  = 120, [89]  = 121, [90]  = 122, [91]  = 91,  [92]  = 92,  [93]  = 93,  [94]  = 94,  [95]  = 95,
    [96]  = 96,  [97]  = 97,  [98]  = 98,  [99]  = 99,  [100] = 100, [101] = 101, [102] = 102, [103] = 103,
    [104] = 104, [105] = 105, [106] = 106, [107] = 107, [108] = 108, [109] = 109, [110] = 110, [111] = 111,
    [112] = 112, [113] = 113, [114] = 114, [115] = 115, [116] = 116, [117] = 117, [118] = 118, [119] = 119,
    [120] = 120, [121] = 121, [122] = 122, [123] = 123, [124] = 124, [125] = 125, [126] = 126, [127] = 127,
    [128] = 128, [129] = 129, [130] = 130, [131] = 131, [132] = 132, [133] = 133, [134] = 134, [135] = 135,
    [136] = 136, [137] = 137, [138] = 138, [139] = 139, [140] = 140, [141] = 141, [142] = 142, [143] = 143,
    [144] = 144, [145] = 145, [146] = 146, [147] = 147, [148] = 148, [149] = 149, [150] = 150, [151] = 151,
    [152] = 152, [153] = 153, [154] = 154, [155] = 155, [156] = 156, [157] = 157, [158] = 158, [159] = 159,
    [160] = 160, [161] = 161, [162] = 162, [163] = 163, [164] = 164, [165] = 165, [166] = 166, [167] = 167,
    [168] = 168, [169] = 169, [170] = 170, [171] = 171, [172] = 172, [173] = 173, [174] = 174, [175] = 175,
    [176] = 176, [177] = 177, [178] = 178, [179] = 179, [180] = 180, [181] = 181, [182] = 182, [183] = 183,
    [184] = 184, [185] = 185, [186] = 186, [187] = 187, [188] = 188, [189] = 189, [190] = 190, [191] = 191,
    [192] = 192, [193] = 193, [194] = 194, [195] = 195, [196] = 196, [197] = 197, [198] = 198, [199] = 199,
    [200] = 200, [201] = 201, [202] = 202, [203] = 203, [204] = 204, [205] = 205, [206] = 206, [207] = 207,
    [208] = 208, [209] = 209, [210] = 210, [211] = 211, [212] = 212, [213] = 213, [214] = 214, [215] = 215,
    [216] = 216, [217] = 217, [218] = 218, [219] = 219, [220] = 220, [221] = 221, [222] = 222, [223] = 223,
    [224] = 224, [225] = 225, [226] = 226, [227] = 227, [228] = 228, [229] = 229, [230] = 230, [231] = 231,
    [232] = 232, [233] = 233, [234] = 234, [235] = 235, [236] = 236, [237] = 237, [238] = 238, [239] = 239,
    [240] = 240, [241] = 241, [242] = 242, [243] = 243, [244] = 244, [245] = 245, [246] = 246, [247] = 247,
    [248] = 248, [249] = 249, [250] = 250, [251] = 251, [252] = 252, [253] = 253, [254] = 254, [255] = 255,
};

static const char char_to_digit[] = 
{
    ['0'] = 0,
    ['1'] = 1,
    ['2'] = 2,
    ['3'] = 3,
    ['4'] = 4,
    ['5'] = 5,
    ['6'] = 6,
    ['7'] = 7,
    ['8'] = 8,
    ['9'] = 9,
    ['a'] = 10, ['A'] = 10,
    ['b'] = 11, ['B'] = 11,
    ['c'] = 12, ['C'] = 12,
    ['d'] = 13, ['D'] = 13,
    ['e'] = 14, ['E'] = 14,
    ['f'] = 15, ['F'] = 15,
};

bool string_parse_int(string_t *inout_string, int64_t *out_value)
{
    *out_value = 0;

    string_t string = *inout_string;

    bool result = false;

    size_t i = 0;
    for (; i < string.count; i += 1)
    {
        if (!is_whitespace(string.data[i]))
            break;
    }

    int64_t sign = 1;
    for (; i < string.count; i += 1)
    {
        if      (string.data[i] == '-') { sign = -1; }
        else if (string.data[i] == '+') { sign =  1; }
        else break;
    }

    int64_t base = 10;
#if 0
    // TODO: Fix this parsing by only recognising this as a base prefix
    // if it is followed by a valid numeric digit
    // Otherwise the number '0' will be consumed as an octal prefix and
    // then cause the below code to instantly fail.
    if (string.data[i] == '0')
    { 
        base = 8; i += 1; 
        if (string.data[i] == 'x' || string.data[i] == 'X') 
        {
            base = 16; i += 1; 
        }
        else if (string.data[i] == 'b')
        {
            base = 2; i += 1;
        }
    }
#endif

    string.data += i;
    string.count -= i;
    i = 0;

    int64_t value = 0;
    for (; i < string.count; i += 1)
    {
        uint8_t c = string.data[i];
        int64_t digit = char_to_digit[c];
        if (digit == 0 && c != '0')
        {
            // non-numeric character
            break;
        }
        if (digit >= base)
        {
            // digit out of range
            break;
        }
        if (value > (INT64_MAX - digit) / base)
        {
            // overflow
            value = INT64_MAX;
            break;
        }
        value *= base;
        value += digit;
    }

    value *= sign;

    string.data += i;
    string.count -= i;

    if (i != 0)
    {
        *out_value = value;
        *inout_string = string;
        result = true;
    }

    return result;
}

parse_float_result_t string_parse_float(string_t string)
{
	parse_float_result_t result = {0};

	m_scoped_temp
	{
		char *null_terminated = string_null_terminate(temp, string).data;

		const char *strtod_end = NULL;
		result.value = (float)strtod(null_terminated, &strtod_end);

		result.is_valid = strtod_end != null_terminated;
		if (result.is_valid)
		{
			result.advance = strtod_end - null_terminated;
		}
	}

    return result;
}

uint64_t string_hash_with_seed(string_t string, uint64_t seed)
{
    return XXH3_64bits_withSeed(string.data, string.count, seed);
}

uint64_t string_hash(string_t string)
{
    return XXH3_64bits(string.data, string.count);
}

uint64_t hash_combine(uint64_t a, uint64_t b)
{
	uint64_t x[2] = { a, b };
	return XXH3_64bits(x, sizeof(x));
}

void string_storage_append_impl(string_storage_overlay_t *storage, size_t capacity, string_t string)
{
	size_t size_left = capacity - storage->count;
	size_t copy_size = MIN(string.count, size_left);

	copy_memory(&storage->data[storage->count], string.data, copy_size);
	storage->count += copy_size;
}

int calculate_edit_distance(string_t s, string_t t, string_match_flags_t flags)
{
    int n = (int)s.count;
    int m = (int)t.count;
    int stride = n + 1;

	int result = -1;

	m_scoped_temp
	{
		int *matrix = m_alloc_array(temp, (n + 1)*(m + 1), int);

		for (int i = 1; i <= n; i += 1) matrix[i       ] = i;
		for (int i = 1; i <= m; i += 1) matrix[i*stride] = i;

		for (int i = 1; i <= n; i += 1)
		{
			char s_i = s.data[i - 1];

			if (flags & StringMatch_case_insensitive)
			{
				s_i = to_lower(s_i);
			}

			for (int j = 1; j <= m; j += 1)
			{
				char t_j = t.data[j - 1];

				if (flags & StringMatch_case_insensitive)
				{
					t_j = to_lower(t_j);
				}

				int cost = (s_i == t_j ? 0 : 1);

				int cell_a = matrix[(i - 1) + (j    )*stride] + 1;
				int cell_b = matrix[(i    ) + (j - 1)*stride] + 1;
				int cell_c = matrix[(i - 1) + (j - 1)*stride] + cost;

				int value = MIN(MIN(cell_a, cell_b), cell_c);

				matrix[i + j*stride] = value;
			}
		}

		result = matrix[n + m*stride];
	}

    return result;
}

// NOTE: I took this from textit - my unfinishd text editor. I don't remember where I got this algorithm though, or what it is called.
size_t find_substring(string_t text, string_t pattern, string_match_flags_t flags)
{
    size_t m = pattern.count;
    size_t R = ~1ull;
    size_t pattern_mask[256];

	VERIFY(m < 8*sizeof(size_t));

    if (m == 0) return 0;
    if (m >= 8*sizeof(size_t)) return STRING_NPOS; // too long

    for (size_t i = 0; i < 256; i += 1) pattern_mask[i] = ~0ull;
    for (size_t i = 0; i < m;   i += 1)
    {
        uint8_t c = pattern.data[i];
        if (flags & StringMatch_case_insensitive) c = to_lower(c);
        pattern_mask[c] &= ~(1ull << i);
    }

    for (size_t i = 0; i < text.count; i += 1)
    {
        uint8_t c = text.data[i];
        if (flags & StringMatch_case_insensitive) c = to_lower(c);
        R |= pattern_mask[c];
        R <<= 1;

        if ((R & (1ull << m)) == 0)
        {
            return i - m + 1;
        }
    }

    return STRING_NPOS;
}

// NOTE: I took this from textit - my unfinishd text editor. I don't remember where I got this algorithm though, or what it is called.
size_t find_substring_backwards(string_t text, string_t pattern, string_match_flags_t flags)
{
    size_t m = pattern.count;
    size_t R = ~1ull;
    size_t pattern_mask[256];

	VERIFY(m < 8*sizeof(size_t));

    if (m == 0) return 0;
    if (m >= 8*sizeof(size_t)) return STRING_NPOS; // too long

    for (size_t i = 0; i < 256; i += 1) pattern_mask[i] = ~0ull;
    for (size_t i = 0; i < m;   i += 1)
    {
        uint8_t c = pattern.data[m - i - 1];
        if (flags & StringMatch_case_insensitive) c = to_lower(c);
        pattern_mask[c] &= ~(1ull << i);
    }

    for (size_t i = 0; i < text.count; i += 1)
    {
        uint8_t c = text.data[text.count - i - 1];
        if (flags & StringMatch_case_insensitive) c = to_lower(c);
        R |= pattern_mask[c];
        R <<= 1;

        if ((R & (1ull << m)) == 0)
        {
            return text.count - i - 1;
        }
    }

    return STRING_NPOS;
}