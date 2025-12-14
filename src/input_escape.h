#ifndef IK_INPUT_ESCAPE_H
#define IK_INPUT_ESCAPE_H

#include "input.h"

// Parse escape sequence byte
void ik_input_parse_escape_sequence(ik_input_parser_t *parser, char byte,
                                     ik_input_action_t *action_out);

#endif // IK_INPUT_ESCAPE_H
