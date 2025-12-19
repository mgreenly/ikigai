#include "msg.h"

#include "../panic.h"
#include "../wrapper.h"

#include <time.h>

ik_mail_msg_t *ik_mail_msg_create(TALLOC_CTX *ctx,
                                   const char *from_uuid,
                                   const char *to_uuid,
                                   const char *body)
{
    ik_mail_msg_t *msg = talloc_zero_(ctx, sizeof(ik_mail_msg_t));
    if (msg == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    msg->from_uuid = talloc_strdup(msg, from_uuid);
    msg->to_uuid = talloc_strdup(msg, to_uuid);
    msg->body = talloc_strdup(msg, body);
    msg->timestamp = time(NULL);
    msg->read = false;
    msg->id = 0;

    return msg;
}
