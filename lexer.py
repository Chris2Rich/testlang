import argparse
import collections
import os
from enum import Enum

class TokenType(Enum):
    EOF = 0
    ID = 1
    STR = 2
    ARR = 3
    NUM = 4
    BND = 5
    NL = 6
    EXP = 7

    PPOP = 10
    FLIP = 11
    DUPE = 12

    NOT = 20

    BNT = 30
    BAN = 31
    BOR = 32
    BXR = 33

    ADD = 40
    SUB = 41
    MUL = 42
    DIV = 43
    MOD = 44
    EQU = 45
    RSH = 46
    LSH = 47

    RARR = 50
    LARR = 51
    RBRA = 52
    LBRA = 53
    RSQU = 54
    LSQU = 55
    RCUR = 56
    LCUR = 57

    IFZERO = 60
    IFLESSZERO = 61

    IMPORT = 110
    IMPORTC = 111

    DEF_START = 100
    DEF_END = 101
    LABEL = 102


Token = collections.namedtuple('Token', ['value', 'type'])

def _is_complex(body):
    """Checks if a function body contains control flow tokens."""
    for token in body:
        if token.type in (TokenType.LABEL, TokenType.IFZERO, TokenType.IFLESSZERO):
            return True
    return False

def tokenize(source_code):
    c = 0
    single_char_map = {
        '\n': TokenType.NL, '+': TokenType.ADD, '*': TokenType.MUL, '%': TokenType.MOD,
        '@': TokenType.EXP, ';': TokenType.PPOP, ':': TokenType.FLIP, '.': TokenType.DUPE,
        '&': TokenType.BAN, '|': TokenType.BOR, '^': TokenType.BXR, '~': TokenType.BNT,
        '(': TokenType.LBRA, ')': TokenType.RBRA, '[': TokenType.LSQU, ']': TokenType.RSQU,
        '{': TokenType.LCUR, '}': TokenType.RCUR,
    }

    while c < len(source_code):
        char = source_code[c]
        
        if char == '?':
            c += 1
            # Skip optional whitespace
            if c < len(source_code) and source_code[c] == "<":
                c += 1
                while c < len(source_code) and source_code[c] in " ,\t":
                    c += 1
                start = c
                while c < len(source_code) and (source_code[c].isalnum()):
                    c += 1
                value = source_code[start:c]
                yield Token(value, TokenType.IFLESSZERO)
            else:
                while c < len(source_code) and source_code[c] in " ,\t":
                    c += 1
                start = c
                while c < len(source_code) and (source_code[c].isalnum()):
                    c += 1
                value = source_code[start:c]
                yield Token(value, TokenType.IFZERO)
            continue

        if char in " ,":
            c += 1
            continue

        if char == '/':
            if c + 1 < len(source_code):
                if source_code[c+1] == '/':
                    while c < len(source_code) and source_code[c] != '\n':
                        c += 1
                else:
                    yield Token(char, TokenType.DIV)
                    c += 1
            else:
                yield Token(char, TokenType.DIV)
                c += 1
            continue

        if char in single_char_map:
            yield Token(char, single_char_map[char])
            c += 1
            continue
        
        if char == '-':
            if c + 1 < len(source_code):
                if not source_code[c + 1].isdigit():
                    yield Token(char, TokenType.SUB)
                    c += 1
                    continue
                else:
                    start = c
                    has_dot = False
                    has_sub = False
                    while c < len(source_code) and (source_code[c].isdigit() or (source_code[c] == '.' and not has_dot) or (source_code[c] == "-" and not has_sub)):
                        if source_code[c] == '.':
                            has_dot = True
                        if source_code[c] == '-':
                            has_sub = True
                        c += 1
                    value = source_code[start:c]
                    yield Token(value, TokenType.NUM)
                    continue
            else:
                yield Token(char, TokenType.SUB)
                c += 1
                continue

        if char == '<':
            if c + 1 < len(source_code) and source_code[c+1] == '<':
                yield Token("<<", TokenType.LSH)
                c += 2
            else:
                yield Token("<", TokenType.LARR)
                c += 1
            continue

        if char == '>':
            if c + 1 < len(source_code) and source_code[c+1] == '>':
                yield Token(">>", TokenType.RSH)
                c += 2
            else:
                yield Token(">", TokenType.RARR)
                c += 1
            continue

        if char == '=':
            if c + 1 < len(source_code) and source_code[c+1] == '>':
                yield Token("=>", TokenType.BND)
                c += 2
            else:
                yield Token("=", TokenType.EQU)
                c += 1
            continue
        
        if char == '_':
            if c + 1 < len(source_code) and source_code[c+1] == '(':
                c += 2
                start = c
                while c < len(source_code) and source_code[c] != ')':
                    c += 1
                value = source_code[start:c]
                yield Token(value, TokenType.LABEL)
                c += 1
                continue
        
        if char.isalpha() or char == '_':
            start = c
            has_dot = False
            while c < len(source_code) and (source_code[c].isalnum() or source_code[c] == '_' or (source_code[c] == '.' and not has_dot)):
                if source_code[c] == '.':
                    has_dot = True
                c += 1
            value = source_code[start:c]
            
            # Handle import "module" and importc "module" syntax
            if value == 'import' or value == 'importc':
                # Skip whitespace
                while c < len(source_code) and source_code[c] in " \t":
                    c += 1
                # Expect quote
                if c < len(source_code) and source_code[c] in '"\'':
                    quote = source_code[c]
                    c += 1
                    module_start = c
                    while c < len(source_code) and source_code[c] != quote:
                        c += 1
                    module_name = source_code[module_start:c]
                    c += 1  # Skip closing quote
                    token_type = TokenType.IMPORT if value == 'import' else TokenType.IMPORTC
                    yield Token(module_name, token_type)
                    continue
            
            yield Token(value, TokenType.ID)
        elif char.isdigit():
            start = c
            has_dot = False
            while c < len(source_code) and (source_code[c].isdigit() or (source_code[c] == '.' and not has_dot)):
                if source_code[c] == '.':
                    has_dot = True
                c += 1
            value = source_code[start:c]
            yield Token(value, TokenType.NUM)
        else:
            c += 1

    yield Token("eof", TokenType.EOF)



def collect_all_identifiers(tokens):
    """Collect all function definitions from the token stream (including imported modules)."""
    identifiers = {}
    i = 0
    while i < len(tokens):
        # Look for pattern: ID BND ...
        if (tokens[i].type == TokenType.ID and 
            i + 1 < len(tokens) and 
            tokens[i + 1].type == TokenType.BND):
            name = tokens[i].value
            # Find the end of the body (next NL)
            body_start = i + 2
            body_end = body_start
            while body_end < len(tokens) and tokens[body_end].type != TokenType.NL:
                body_end += 1
            body = tokens[body_start:body_end]
            identifiers[name] = body
            i = body_end + 1 if body_end < len(tokens) else body_end
        else:
            i += 1
    return identifiers


def process_bindings(tokens):
    token_list = list(tokens)
    
    # Collect ALL identifiers including those from imported modules
    identifiers = collect_all_identifiers(token_list)
    
    processed_procedural_code = []

    def _check_is_recursive(name, current_identifiers):
        q = collections.deque()
        q.append(name)
        visited = {name}
        
        while q:
            current_name = q.popleft()
            body = current_identifiers.get(current_name, [])
            for token in body:
                if token.type != TokenType.ID:
                    continue
                
                called_func = token.value
                if called_func == name:
                    return True
                
                if called_func not in visited:
                    visited.add(called_func)
                    q.append(called_func)
        return False

    i = 0
    while i < len(token_list):
        line_end = i
        while line_end < len(token_list) and token_list[line_end].type != TokenType.NL:
            line_end += 1
        line_tokens = token_list[i:line_end]
        
        is_definition = (len(line_tokens) >= 2 and 
                         line_tokens[0].type == TokenType.ID and 
                         line_tokens[1].type == TokenType.BND)

        if is_definition:
            # Skip definitions here - we'll handle them in the DEF_START section
            pass
        elif line_tokens:
            expanded = True
            while expanded:
                expanded = False
                new_line = []
                for token in line_tokens:
                    if token.type == TokenType.ID and token.value in identifiers:
                        # Check for recursion AND complexity
                        is_recursive = _check_is_recursive(token.value, identifiers)
                        is_complex = _is_complex(identifiers[token.value])
                        
                        # Only inline if it is SIMPLE and NON-RECURSIVE
                        if not is_recursive and not is_complex:
                            new_line.extend(identifiers[token.value])
                            expanded = True
                        else:
                            new_line.append(token) # Keep it as a function call
                    else:
                        new_line.append(token)
                line_tokens = new_line
            
            processed_procedural_code.extend(line_tokens)

        if line_end < len(token_list):
            processed_procedural_code.append(token_list[line_end])
        i = line_end + 1

    needed_ids = {token.value for token in processed_procedural_code if token.type == TokenType.ID}

    defs_to_write = set()
    if needed_ids:
        q = collections.deque(list(needed_ids))
        visited = set(needed_ids)
        while q:
            func_name = q.popleft()
            if func_name in identifiers:
                defs_to_write.add(func_name)
                for token in identifiers.get(func_name, []):
                    if token.type == TokenType.ID and token.value not in visited:
                        visited.add(token.value)
                        q.append(token.value)
    
    final_stream = []
    
    if defs_to_write:
        final_stream.append(Token('__DEF_START__', TokenType.DEF_START))
        final_stream.append(Token('\n', TokenType.NL))
        
        for name in sorted(list(defs_to_write)):
            if name in identifiers:
                final_stream.append(Token(name, TokenType.ID))
                final_stream.append(Token('=>', TokenType.BND))
                final_stream.extend(identifiers[name])
                final_stream.append(Token('\n', TokenType.NL))

        final_stream.append(Token('__DEF_END__', TokenType.DEF_END))
        final_stream.append(Token('\n', TokenType.NL))

    final_stream.extend(processed_procedural_code)

    return final_stream
    
class TokenIterator:
    def __init__(self, tokens):
        self._iterator = iter(tokens)
        self._peeked = None

    def peek(self):
        if self._peeked is None:
            try:
                self._peeked = next(self._iterator)
            except StopIteration:
                return None
        return self._peeked

    def next(self):
        if self._peeked is not None:
            token = self._peeked
            self._peeked = None
            return token
        try:
            return next(self._iterator)
        except StopIteration:
            return None

def parse_arrays(tokens):
    def _parse_single_array(iterator):
        contents = []
        
        while iterator.peek() and iterator.peek().type != TokenType.RSQU:
            token = iterator.next()
            
            if token.type == TokenType.LSQU:
                contents.append(_parse_single_array(iterator))
            else:
                contents.append(token)

        closing_bracket = iterator.next()
        if not closing_bracket or closing_bracket.type != TokenType.RSQU:
            raise SyntaxError("Syntax Error: Mismatched brackets. Expected ']' but found EOF or other token.")

        if not contents:
            return Token(value=([0], []), type=TokenType.ARR)

        is_nested_array = (contents[0].type == TokenType.ARR)
        
        for item in contents[1:]:
            if (item.type == TokenType.ARR) != is_nested_array:
                raise TypeError("Syntax Error: Inconsistent element types in array. Cannot mix literals and sub-arrays at the same level.")

        if not is_nested_array:
            shape = [len(contents)]
            data = []
            for item in contents:
                if item.type == TokenType.NUM:
                    data.append(float(item.value))
                else:
                    raise TypeError("Syntax Error: Array elements must be numeric")
            return Token(value=(shape, data), type=TokenType.ARR)
        else:
            first_shape = contents[0].value[0]
            for item in contents[1:]:
                if item.value[0] != first_shape:
                    raise TypeError(f"Syntax Error: Ragged arrays are not supported. Mismatched shapes: {first_shape} and {item.value[0]}")
            
            shape = [len(contents)] + first_shape
            
            data = []
            for item in contents:
                data.extend(item.value[1])
            
            return Token(value=(shape, data), type=TokenType.ARR)

    iterator = TokenIterator(tokens)
    
    while True:
        token = iterator.peek()
        if not token or token.type == TokenType.EOF:
            break

        if token.type == TokenType.LSQU:
            iterator.next()
            yield _parse_single_array(iterator)
        elif token.type == TokenType.RSQU:
            raise SyntaxError("Syntax Error: Unexpected ']' at top level.")
        else:
            yield iterator.next()

def remove_nl(token_stream):
    if not token_stream:
        return []

    cleaned_stream = []
    cleaned_stream.append(token_stream[0])

    for i in range(1, len(token_stream)):
        current_token = token_stream[i]
        last_added_token = cleaned_stream[-1]

        if not (current_token.type == TokenType.NL and last_added_token.type == TokenType.NL) and not (current_token.type == TokenType.NL and (last_added_token.type == TokenType.DEF_START or last_added_token.type == TokenType.DEF_END)):
            cleaned_stream.append(current_token)
            
    return cleaned_stream


def cleanup_eof_tokens(token_stream):
    """Remove all EOF tokens and add a single one at the end."""
    if not token_stream:
        return [Token("eof", TokenType.EOF)]
    
    # Filter out all EOF tokens
    non_eof = [t for t in token_stream if t.type != TokenType.EOF]
    
    # Add a single EOF at the end
    non_eof.append(Token("eof", TokenType.EOF))
    
    return non_eof


# Global variable for base import directory
_base_import_dir = "."

def get_search_paths():
    """Get the list of paths to search for modules."""
    paths = []
    # First check base import directory
    global _base_import_dir
    if _base_import_dir:
        paths.append(_base_import_dir)
    # Check TESTLANG_PATH environment variable
    testlang_path = os.environ.get('TESTLANG_PATH', '')
    if testlang_path:
        paths.extend(testlang_path.split(':'))
    # Also check current directory
    paths.append('.')
    return paths


def find_module_file(module_name):
    """Find a .stack module file in the search paths."""
    search_paths = get_search_paths()
    for path in search_paths:
        full_path = os.path.join(path, f"{module_name}.stack")
        if os.path.isfile(full_path):
            return full_path
    return None


def find_c_module_file(module_name):
    """Find a C object file in the search paths."""
    # Handle relative paths starting with ./ or ../
    global _base_import_dir
    if module_name.startswith('./') or module_name.startswith('../'):
        full_path = os.path.normpath(os.path.join(_base_import_dir, module_name))
        if os.path.isfile(full_path):
            return full_path
    
    # Otherwise search in standard paths
    search_paths = get_search_paths()
    extensions = ['.o', '.a', '.so']
    prefixes = ['', 'lib']
    
    for path in search_paths:
        for prefix in prefixes:
            for ext in extensions:
                full_path = os.path.join(path, f"{prefix}{module_name}{ext}")
                if os.path.isfile(full_path):
                    return full_path
    return None


class CircularImportError(Exception):
    """Raised when a circular import is detected."""
    pass


def load_module_tokens(module_name, module_stack=None, c_objects=None):
    """
    Recursively load a module and its dependencies.
    
    Args:
        module_name: Name of the module to load
        module_stack: Stack of currently loading modules (for circular detection)
        c_objects: Set to collect C object file paths
    
    Returns:
        List of tokens from the module with prefixed names
    """
    if module_stack is None:
        module_stack = []
    if c_objects is None:
        c_objects = set()
    
    # Check for circular imports
    if module_name in module_stack:
        chain = ' -> '.join(module_stack + [module_name])
        raise CircularImportError(f"Circular import detected: {chain}")
    
    module_path = find_module_file(module_name)
    if not module_path:
        raise FileNotFoundError(f"Module '{module_name}' not found in search paths")
    
    with open(module_path, 'r') as f:
        source = f.read()
    
    # Tokenize the module source
    tokens = list(tokenize(source))
    
    # Process imports and collect module tokens
    result_tokens = []
    imported_modules = set()
    
    i = 0
    while i < len(tokens):
        token = tokens[i]
        
        if token.type == TokenType.IMPORT:
            imported_module = token.value
            if imported_module not in imported_modules:
                imported_modules.add(imported_module)
                # Recursively load the imported module
                sub_tokens = load_module_tokens(
                    imported_module, 
                    module_stack + [module_name],
                    c_objects
                )
                result_tokens.extend(sub_tokens)
            i += 1
            # Skip following NL if present
            if i < len(tokens) and tokens[i].type == TokenType.NL:
                i += 1
            continue
        
        elif token.type == TokenType.IMPORTC:
            imported_c_module = token.value
            c_path = find_c_module_file(imported_c_module)
            if c_path:
                c_objects.add(c_path)
            else:
                raise FileNotFoundError(f"C module '{imported_c_module}' not found in search paths")
            i += 1
            # Skip following NL if present
            if i < len(tokens) and tokens[i].type == TokenType.NL:
                i += 1
            continue
        
        result_tokens.append(token)
        i += 1
    
    # Prefix all function definitions and calls with module name
    prefixed_tokens = prefix_module_tokens(result_tokens, module_name)
    
    return prefixed_tokens


def prefix_module_tokens(tokens, module_name):
    """
    Prefix all identifiers in a module with the module name.
    
    Function definitions: func => body  becomes  module.func => body
    Function calls: func  becomes  module.func (if not already qualified)
    Qualified calls (other.func) are left as-is.
    """
    result = []
    defined_functions = set()
    
    # First pass: collect all defined function names in this module
    i = 0
    while i < len(tokens):
        token = tokens[i]
        # Look for pattern: ID BND ...
        if (token.type == TokenType.ID and 
            i + 1 < len(tokens) and 
            tokens[i + 1].type == TokenType.BND):
            defined_functions.add(token.value)
        i += 1
    
    # Second pass: prefix tokens
    i = 0
    while i < len(tokens):
        token = tokens[i]
        
        if token.type == TokenType.ID:
            # Check if this is a function definition
            if i + 1 < len(tokens) and tokens[i + 1].type == TokenType.BND:
                # This is a definition: prefix the function name
                result.append(Token(f"{module_name}.{token.value}", TokenType.ID))
            else:
                # This is a function call
                if '.' in token.value:
                    # Already qualified (e.g., other.func), leave as-is
                    result.append(token)
                elif token.value in defined_functions:
                    # Call to a function defined in this module: prefix it
                    result.append(Token(f"{module_name}.{token.value}", TokenType.ID))
                else:
                    # Could be a builtin or undefined, leave as-is for now
                    # Actually, for safety, we should prefix all calls to defined functions
                    result.append(token)
        else:
            result.append(token)
        
        i += 1
    
    return result


def prefix_main_tokens(tokens, main_module_name="main"):
    """
    Prefix all function definitions and calls in the main file.
    
    Function definitions: func => body  becomes  main.func => body
    Function calls: func  becomes  main.func (for locally defined functions)
    Qualified calls (module.func) are left as-is.
    """
    result = []
    defined_functions = set()
    
    # First pass: collect all defined function names (only those without dots - main file functions)
    i = 0
    while i < len(tokens):
        token = tokens[i]
        if (token.type == TokenType.ID and 
            i + 1 < len(tokens) and 
            tokens[i + 1].type == TokenType.BND and
            '.' not in token.value):  # Only collect main file functions (no dots)
            defined_functions.add(token.value)
        i += 1
    
    # Second pass: prefix tokens
    i = 0
    while i < len(tokens):
        token = tokens[i]
        
        if token.type == TokenType.ID:
            # If already contains a dot, it's already module-qualified - leave as-is
            if '.' in token.value:
                result.append(token)
            # Check if this is a function definition
            elif i + 1 < len(tokens) and tokens[i + 1].type == TokenType.BND:
                # This is a definition: prefix the function name
                result.append(Token(f"{main_module_name}.{token.value}", TokenType.ID))
            else:
                # This is a function call
                if token.value in defined_functions:
                    # Call to a locally defined function: prefix it
                    result.append(Token(f"{main_module_name}.{token.value}", TokenType.ID))
                else:
                    # Undefined or builtin, leave as-is
                    result.append(token)
        else:
            result.append(token)
        
        i += 1
    
    return result


def process_imports(tokens):
    """
    Process all imports in a token stream and return combined tokens.
    
    Returns: (combined_tokens, c_object_files)
    """
    result_tokens = []
    c_objects = set()
    imported_modules = set()
    
    i = 0
    while i < len(tokens):
        token = tokens[i]
        
        if token.type == TokenType.IMPORT:
            imported_module = token.value
            if imported_module not in imported_modules:
                imported_modules.add(imported_module)
                # Load the module recursively
                module_tokens = load_module_tokens(imported_module, c_objects=c_objects)
                result_tokens.extend(module_tokens)
            i += 1
            # Skip following NL if present
            if i < len(tokens) and tokens[i].type == TokenType.NL:
                i += 1
            continue
        
        elif token.type == TokenType.IMPORTC:
            imported_c_module = token.value
            c_path = find_c_module_file(imported_c_module)
            if c_path:
                c_objects.add(c_path)
            else:
                raise FileNotFoundError(f"C module '{imported_c_module}' not found in search paths")
            i += 1
            # Skip following NL if present
            if i < len(tokens) and tokens[i].type == TokenType.NL:
                i += 1
            continue
        
        result_tokens.append(token)
        i += 1
    
    return result_tokens, c_objects

def main():
    parser = argparse.ArgumentParser(description="An efficient interpreter for a custom language.")
    parser.add_argument("input", nargs='?', default="tests/features/array_parse.txt")
    parser.add_argument("output", nargs='?', default="out.txt")
    parser.add_argument("--c-objects", nargs='?', default=None, help="Output file for C object file paths")
    parser.add_argument("--base-dir", nargs='?', default=None, help="Base directory for resolving relative imports")
    args = parser.parse_args()

    # Set base directory for imports
    global _base_import_dir
    if args.base_dir:
        _base_import_dir = args.base_dir
    else:
        # Default to current directory
        _base_import_dir = "."

    print(f"Starting Processing. Input file is {args.input}")

    try:
        with open(args.input, "r") as i_file:
            source = i_file.read()
    except FileNotFoundError:
        print(f"Error: Input file not found at '{args.input}'")
        return

    try:
        # Tokenize the source
        token_stream = list(tokenize(source))
        
        # Process imports and get C object files
        imported_tokens, c_objects = process_imports(token_stream)
        
        # Prefix main file tokens
        prefixed_tokens = prefix_main_tokens(imported_tokens)
        
        # Process bindings
        processed_tokens = process_bindings(prefixed_tokens)
        
        # Clean up multiple EOF tokens before parse_arrays (which stops on EOF)
        cleaned_eof_tokens = cleanup_eof_tokens(processed_tokens)
        
        # Parse arrays
        arrayed_tokens = list(parse_arrays(cleaned_eof_tokens))
        final_tokens = remove_nl(arrayed_tokens)

        # Write tokens to output file
        with open(args.output, "w") as o_file:
            for token in final_tokens:
                o_file.write(str(token) + "\n")
        
        # Write C object files to separate file if requested
        if args.c_objects and c_objects:
            with open(args.c_objects, "w") as c_file:
                for obj_path in c_objects:
                    c_file.write(obj_path + "\n")
            print(f"C object files written to {args.c_objects}")
        
        print(f"Processing complete. Output written to {args.output}")
        
    except CircularImportError as e:
        print(f"Error: {e}")
        return 1
    except FileNotFoundError as e:
        print(f"Error: {e}")
        return 1


if __name__ == "__main__":
    main()
