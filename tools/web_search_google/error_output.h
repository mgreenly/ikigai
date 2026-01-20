#ifndef WEB_SEARCH_GOOGLE_ERROR_OUTPUT_H
#define WEB_SEARCH_GOOGLE_ERROR_OUTPUT_H

void output_error_with_event(void *ctx, const char *error, const char *error_code);
void output_error(void *ctx, const char *error, const char *error_code);

#endif
