# Uncrustify Configuration
# GNU coding style
#
# Run with: uncrustify -c .uncrustify.cfg --replace --no-backup <file>

# General options
newlines                                 = lf
input_tab_size                           = 8
output_tab_size                          = 2
indent_columns                           = 2
indent_with_tabs                         = 0

# Indenting
indent_brace                             = 0
indent_braces                            = false
indent_braces_no_func                    = false
indent_braces_no_class                   = false
indent_braces_no_struct                  = false
indent_switch_case                       = 2

# Newline placement
nl_enum_brace                            = add
nl_union_brace                           = add
nl_struct_brace                          = add
nl_do_brace                              = add
nl_if_brace                              = add
nl_for_brace                             = add
nl_else_brace                            = add
nl_while_brace                           = add
nl_switch_brace                          = add
nl_brace_while                           = remove
nl_brace_else                            = add
nl_var_def_blk_end_func_top              = 1
nl_fcall_brace                           = add
nl_fdef_brace                            = add
nl_after_return                          = true

# Blank lines
nl_max                                   = 3
nl_before_block_comment                  = 2
nl_after_func_body                       = 2
nl_after_func_proto_group                = 2

# Spacing
sp_before_sparen                         = force
sp_after_sparen                          = force
sp_inside_sparen                         = remove
sp_sparen_brace                          = add
sp_before_ptr_star                       = force
sp_after_ptr_star                        = remove
sp_between_ptr_star                      = remove
sp_after_cast                            = remove
sp_inside_paren                          = remove
sp_paren_paren                           = remove
sp_balance_nested_parens                 = false
sp_sizeof_paren                          = remove
sp_after_comma                           = force
sp_before_comma                          = remove
sp_after_semi_for_empty                  = force
sp_func_call_paren                       = remove
sp_func_proto_paren                      = remove
sp_func_def_paren                        = remove
sp_inside_fparens                        = remove
sp_inside_fparen                         = remove
sp_square_fparen                         = remove
sp_fparen_brace                          = add
sp_before_square                         = ignore
sp_before_squares                        = ignore
sp_inside_square                         = remove

# Spacing around operators
sp_arith                                 = force
sp_assign                                = force
sp_bool                                  = force
sp_compare                               = force

# Code alignment
align_var_def_star_style                 = 2
align_var_def_amp_style                  = 2
align_func_params                        = true
align_same_func_call_params              = true
align_var_def_thresh                     = 12
align_var_def_gap                        = 1

# Comments
cmt_indent_multi                         = true
cmt_c_group                              = false
cmt_c_nl_start                           = false
cmt_c_nl_end                             = false

# Code modifying
mod_full_brace_if                        = ignore
mod_full_brace_for                       = ignore
mod_full_brace_while                     = ignore
mod_full_brace_do                        = ignore
mod_remove_extra_semicolon               = true

# Line breaking
code_width                               = 79
ls_for_split_full                        = true
ls_func_split_full                       = true
