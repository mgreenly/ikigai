#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <talloc.h>

typedef struct ik_mail_msg {
    int64_t id;
    char *from_uuid;
    char *to_uuid;
    char *body;
    int64_t timestamp;
    bool read;
} ik_mail_msg_t;

ik_mail_msg_t *ik_mail_msg_create(TALLOC_CTX *ctx,
                                   const char *from_uuid,
                                   const char *to_uuid,
                                   const char *body);
