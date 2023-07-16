#ifndef CONVAR_H
#define CONVAR_H

#include "core/api_types.h"

typedef void (*concommand_t)(void);

typedef enum convar_kind_t
{
    CONVAR_CMD,
    CONVAR_INT,
    CONVAR_FLT,
    CONVAR_STR,
} convar_kind_t;

// TODO: string pool / interning, use strings to store values

typedef struct convar_t
{
    struct convar_t *next;

    STRING_STORAGE(64) key;
    convar_kind_t kind;
    union
    {
        concommand_t cmdval;
        int64_t      intval;
        float        fltval;
        string_t     strval;
    };
} convar_t;

void con_register(string_t key, const convar_t *var);
convar_t *con_find(string_t key);

#endif /* CONVAR_H */
