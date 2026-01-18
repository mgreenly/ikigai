#ifndef ERROR_H
#define ERROR_H

void output_error(void *ctx, const char *err, const char *code);
void output_error_with_event(void *ctx, const char *err, const char *code);

#endif
