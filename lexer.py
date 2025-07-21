import argparse
import collections
from enum import Enum

class TokenType(Enum):
    EOF = 0
    ID = 1
    STR = 2
    ARR = 3
    NUM = 4
    BND = 5
    NL = 6

    POP = 10
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

    DEF_START = 100
    DEF_END = 101


Token = collections.namedtuple('Token', ['value', 'type'])

def tokenize(source_code):
    c = 0
    single_char_map = {
        '\n': TokenType.NL, '+': TokenType.ADD, '*': TokenType.MUL, '%': TokenType.MOD,
        ';': TokenType.POP, ':': TokenType.FLIP,
        '&': TokenType.BAN, '|': TokenType.BOR, '^': TokenType.BXR, '~': TokenType.BNT,
        '(': TokenType.LBRA, ')': TokenType.RBRA, '[': TokenType.LSQU, ']': TokenType.RSQU,
        '{': TokenType.LCUR, '}': TokenType.RCUR,
    }

    while c < len(source_code):
        char = source_code[c]

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
        
        if char.isalpha() or char == '_':
            start = c
            while c < len(source_code) and (source_code[c].isalnum() or source_code[c] == '_'):
                c += 1
            value = source_code[start:c]
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
        elif char == '"' or char == "'":
            quote = char
            start = c
            c += 1
            while c < len(source_code) and source_code[c] != quote:
                c += 1
            c += 1
            value = source_code[start:c]
            yield Token(value, TokenType.STR)
        else:
            c += 1

    yield Token("eof", TokenType.EOF)



def process_bindings(tokens):
    token_list = list(tokens)
    identifiers = {}
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
            name = line_tokens[0].value
            body = line_tokens[2:]
            identifiers[name] = body
        elif line_tokens:
            expanded = True
            while expanded:
                expanded = False
                new_line = []
                for token in line_tokens:
                    if token.type == TokenType.ID and token.value in identifiers:
                        if not _check_is_recursive(token.value, identifiers):
                            new_line.extend(identifiers[token.value])
                            expanded = True
                        else:
                            new_line.append(token)
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
            data = [item.value for item in contents]
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

def main():
    parser = argparse.ArgumentParser(description="An efficient interpreter for a custom language.")
    parser.add_argument("input", nargs='?', default="tests/features/array_parse.txt")
    parser.add_argument("output", nargs='?', default="out.txt")
    args = parser.parse_args()

    print(f"Starting Processing. Input file is {args.input}")

    try:
        with open(args.input, "r") as i_file:
            source = i_file.read()
    except FileNotFoundError:
        print(f"Error: Input file not found at '{args.input}'")
        return

    token_stream = tokenize(source)

    processed_tokens = process_bindings(token_stream)

    arrayed_tokens = list(parse_arrays(processed_tokens))
    final_tokens = remove_nl(arrayed_tokens)

    with open(args.output, "w") as o_file:
        for token in final_tokens:
            o_file.write(str(token) + "\n")
    
    print(f"Processing complete. Output written to {args.output}")


if __name__ == "__main__":
    main()