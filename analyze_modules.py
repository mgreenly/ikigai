#!/usr/bin/env python3
"""Analyze C codebase to extract modules, functions, and cross-module dependencies."""

import re
import os
from pathlib import Path
from collections import defaultdict

# Parse function definitions from header files
def extract_functions_from_header(filepath):
    """Extract function declarations from a header file."""
    functions = []
    try:
        with open(filepath, 'r') as f:
            content = f.read()

        # Remove comments
        content = re.sub(r'//.*', '', content)
        content = re.sub(r'/\*.*?\*/', '', content, flags=re.DOTALL)

        # Find function declarations (simple pattern)
        # Matches: return_type function_name(params);
        pattern = r'\b([a-zA-Z_][a-zA-Z0-9_]*(?:\s*\*)*)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*\([^)]*\)\s*;'
        matches = re.findall(pattern, content)

        for return_type, func_name in matches:
            # Skip typedefs and common macros
            if func_name not in ['typedef', 'static', 'extern', 'inline']:
                functions.append(func_name)
    except Exception as e:
        print(f"Error reading {filepath}: {e}")

    return functions

# C keywords to exclude from function detection
C_KEYWORDS = {
    'if', 'else', 'while', 'for', 'do', 'switch', 'case', 'default',
    'return', 'break', 'continue', 'goto', 'sizeof', 'typedef',
    'static', 'extern', 'inline', 'register', 'auto', 'const', 'volatile',
    'struct', 'union', 'enum', 'void', 'int', 'char', 'short', 'long',
    'float', 'double', 'signed', 'unsigned', 'bool'
}

# Parse function calls from source files with context
def extract_function_calls_with_context(filepath, all_functions):
    """Extract function calls from a C source file, tracking which function makes each call."""
    function_to_calls = defaultdict(set)  # function_name -> set of called functions

    try:
        with open(filepath, 'r') as f:
            content = f.read()

        # Remove comments but keep line structure
        content = re.sub(r'//.*', '', content)
        content = re.sub(r'/\*.*?\*/', '', content, flags=re.DOTALL)

        # Find function definitions and their bodies
        # Pattern matches: return_type function_name(params) { ... }
        # This is a simple approach - won't handle all edge cases but good enough
        func_def_pattern = r'\b([a-zA-Z_][a-zA-Z0-9_]*(?:\s*\*)*)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*\([^)]*\)\s*\{'

        # Find all function definitions
        for match in re.finditer(func_def_pattern, content):
            func_name = match.group(2)
            start_pos = match.end()

            # Find the matching closing brace
            brace_count = 1
            pos = start_pos
            while pos < len(content) and brace_count > 0:
                if content[pos] == '{':
                    brace_count += 1
                elif content[pos] == '}':
                    brace_count -= 1
                pos += 1

            if brace_count == 0:
                # Extract function body
                func_body = content[start_pos:pos-1]

                # Remove string literals from body to avoid false matches
                func_body = re.sub(r'"[^"]*"', '', func_body)

                # Find all function calls in this body
                call_pattern = r'\b([a-zA-Z_][a-zA-Z0-9_]*)\s*\('
                for call_match in re.finditer(call_pattern, func_body):
                    called_func = call_match.group(1)
                    # Only track calls to functions in our known function list, exclude C keywords
                    if (called_func in all_functions and
                        called_func != func_name and
                        called_func not in C_KEYWORDS):
                        function_to_calls[func_name].add(called_func)

    except Exception as e:
        print(f"Error reading {filepath}: {e}")

    return function_to_calls

def get_module_name(filepath):
    """Get module name from file path."""
    path = Path(filepath)
    # For input_buffer/core.c -> input_buffer
    # For openai/client.c -> openai
    # For config.c -> config
    rel_path = path.relative_to('src')

    # Skip vendor files
    if 'vendor' in rel_path.parts:
        return None

    # If file is in a subdirectory, use the directory name as the module
    # Otherwise use the filename without extension
    if rel_path.parent.name and rel_path.parent.name != '.':
        return rel_path.parent.name
    else:
        return path.stem

def main():
    src_dir = Path('src')

    # First pass: collect all header files and their functions (public API)
    module_functions = defaultdict(list)  # module -> list of functions (public)
    all_functions_set = set()
    file_to_module = {}  # filepath -> module name

    print("Analyzing header files...")
    for header_file in src_dir.rglob('*.h'):
        module = get_module_name(header_file)
        if module and module != 'version':  # Skip version.h
            functions = extract_functions_from_header(header_file)
            # Append to existing module functions (multiple headers per module)
            module_functions[module].extend(functions)
            all_functions_set.update(functions)
            file_to_module[str(header_file)] = module

    # Print summary after processing all headers
    for module in sorted(module_functions.keys()):
        print(f"  {module}: {len(module_functions[module])} public functions")

    # Also collect static/internal functions from .c files
    module_internal_functions = defaultdict(set)  # module -> set of internal functions
    func_def_pattern = r'\b([a-zA-Z_][a-zA-Z0-9_]*(?:\s*\*)*)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*\([^)]*\)\s*\{'

    print("\nAnalyzing source files for internal functions...")
    for src_file in src_dir.rglob('*.c'):
        module = get_module_name(src_file)
        if not module or 'vendor' in str(src_file):
            continue

        try:
            with open(src_file, 'r') as f:
                content = f.read()
            # Remove comments
            content = re.sub(r'//.*', '', content)
            content = re.sub(r'/\*.*?\*/', '', content, flags=re.DOTALL)

            # Find all function definitions
            for match in re.finditer(func_def_pattern, content):
                func_name = match.group(2)
                # Add to internal functions if not already in public API
                if func_name not in all_functions_set:
                    module_internal_functions[module].add(func_name)
                    all_functions_set.add(func_name)
        except Exception as e:
            print(f"Error reading {src_file}: {e}")

    # Merge internal functions into module_functions for display
    for module, internal_funcs in module_internal_functions.items():
        module_functions[module].extend(sorted(internal_funcs))
        print(f"  {module}: {len(internal_funcs)} internal functions")

    # Second pass: analyze source files for function calls
    # Track function-to-function calls: (source_module, source_func) -> (target_module, target_func)
    function_calls = []  # List of (source_module, source_func, target_module, target_func) tuples
    module_source_files = defaultdict(list)

    # Build reverse mapping: function -> module
    func_to_module = {}
    for module, funcs in module_functions.items():
        for func in funcs:
            func_to_module[func] = module

    print("\nAnalyzing source files...")
    for src_file in src_dir.rglob('*.c'):
        file_module = get_module_name(src_file)  # Module based on file location
        if not file_module or 'vendor' in str(src_file):
            continue

        module_source_files[file_module].append(str(src_file))
        func_to_calls = extract_function_calls_with_context(src_file, all_functions_set)

        # Build function-to-function call list
        total_calls = 0
        for source_func, called_funcs in func_to_calls.items():
            # Determine which module the SOURCE function belongs to (by declaration)
            source_module = func_to_module.get(source_func, file_module)

            for called_func in called_funcs:
                target_module = func_to_module.get(called_func)
                if target_module and target_module != source_module:
                    function_calls.append((source_module, source_func, target_module, called_func))
                    total_calls += 1

        print(f"  {file_module}: {total_calls} cross-module function calls")

    # Generate Graphviz dot file
    print("\nGenerating Graphviz dot file...")

    # Helper function to sanitize module names for node IDs
    def sanitize_node_id(module_name):
        return module_name.replace('/', '_')

    # Build function to module mapping for quick lookup
    func_to_module = {}
    for module, funcs in module_functions.items():
        for func in funcs:
            func_to_module[func] = module

    with open('module_structure.dot', 'w') as f:
        f.write('digraph ModuleStructure {\n')
        f.write('  rankdir=LR;\n')
        f.write('  node [shape=record, style=filled, fillcolor=lightblue];\n')
        f.write('  edge [color=gray40, arrowsize=0.7];\n\n')

        # Create nodes for each module with functions as fields
        for module in sorted(module_functions.keys()):
            functions = module_functions[module]
            if not functions:
                continue

            node_id = sanitize_node_id(module)
            # Escape special characters and create record label
            label_parts = [f"<m>{module}"]
            # Show all functions for complete visualization
            for func in sorted(functions):
                label_parts.append(f"<{func}>{func}")

            label = "|".join(label_parts)
            f.write(f'  {node_id} [label="{label}"];\n')

        f.write('\n')

        # Create function-to-function edges
        # Each edge shows which specific function calls which specific function
        for source_module, source_func, target_module, target_func in sorted(function_calls):
            source_id = sanitize_node_id(source_module)
            target_id = sanitize_node_id(target_module)
            f.write(f'  {source_id}:{source_func} -> {target_id}:{target_func};\n')

        f.write('}\n')

    print("\nGenerated: module_structure.dot")
    print(f"\nTotal function-to-function calls: {len(function_calls)}")

    # Print module dependency summary
    module_deps = defaultdict(set)
    for source_module, _, target_module, _ in function_calls:
        module_deps[source_module].add(target_module)

    print("\nModule dependency summary:")
    for source_module in sorted(module_deps.keys()):
        deps = sorted(module_deps[source_module])
        print(f"  {source_module} -> {', '.join(deps)}")

if __name__ == '__main__':
    main()
