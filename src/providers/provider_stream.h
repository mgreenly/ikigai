/**
 * @file provider_stream.h
 * @brief Provider streaming event types
 *
 * NOTE: This header is meant to be included AFTER the core provider enums
 * are defined in provider.h. It's separated out for size management only.
 */

#ifndef IK_PROVIDER_STREAM_H
#define IK_PROVIDER_STREAM_H

#include <stdint.h>
#include <stdbool.h>

/* These types must be defined before including this header */
#ifndef IK_FINISH_REASON_DEFINED
#error "provider_stream.h requires enum types from provider.h - do not include directly"
#endif

/**
 * Stream event types
 */
typedef enum {
    IK_STREAM_START = 0,           /* Stream started */
    IK_STREAM_TEXT_DELTA = 1,      /* Text content chunk */
    IK_STREAM_THINKING_DELTA = 2,  /* Thinking/reasoning chunk */
    IK_STREAM_TOOL_CALL_START = 3, /* Tool call started */
    IK_STREAM_TOOL_CALL_DELTA = 4, /* Tool call argument chunk */
    IK_STREAM_TOOL_CALL_DONE = 5,  /* Tool call complete */
    IK_STREAM_DONE = 6,            /* Stream complete */
    IK_STREAM_ERROR = 7            /* Error occurred */
} ik_stream_event_type_t;

/**
 * Stream event with variant payload
 */
struct ik_stream_event {
    ik_stream_event_type_t type; /* Event type */
    int32_t index;               /* Content block index */
    union {
        /* IK_STREAM_START */
        struct {
            const char *model; /* Model name */
        } start;

        /* IK_STREAM_TEXT_DELTA, IK_STREAM_THINKING_DELTA */
        struct {
            const char *text; /* Text fragment */
        } delta;

        /* IK_STREAM_TOOL_CALL_START */
        struct {
            const char *id;   /* Tool call ID */
            const char *name; /* Function name */
        } tool_start;

        /* IK_STREAM_TOOL_CALL_DELTA */
        struct {
            const char *arguments; /* JSON fragment */
        } tool_delta;

        /* IK_STREAM_DONE */
        struct {
            ik_finish_reason_t finish_reason; /* Completion reason */
            ik_usage_t usage;                 /* Token usage */
            const char *provider_data;        /* Provider metadata */
        } done;

        /* IK_STREAM_ERROR */
        struct {
            ik_error_category_t category; /* Error category */
            const char *message;          /* Error message */
        } error;
    } data;
};

#endif /* IK_PROVIDER_STREAM_H */
