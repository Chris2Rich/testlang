import argparse
# example syntax
# i64 dot_product(i64array a, i64array b) => (reduce<+> *) where (len a == len b)
# ui5array cypher(ui5array s, i5 n) => (- s / 2 n +)

#functions contain metadata based on arguements

parser = argparse.ArgumentParser()
parser.add_argument("input")
parser.add_argument("output")

args = parser.parse_args()
i_file = open(args.input, "r")
i_text = i_file.readlines()
i_file.close()

tok = {
    eof : -1,
    identifier : -2,
    literal : -3,
    binding : -4,
    compare: -5,
    
    "+" : -6,
    "-" : -7,
    "*" : -8,
    "/" : -9,
    "%" : -10,
    ">" : -11,
    "<" : -12
}

id_str = str()
tok_stream = []

for i in i_text:
    for j in range(0, len(i)):
        if i[j] == " ":
            continue
        if i[j] == "/":
            if j + 1 == len(i):
                tok_stream.append(tok[i[j]])
            elif i[j + 1] == "/":
                break
            else:
                tok_stream.append(tok[i[j]])
        if i[j] == "*":
            tok_stream.append(tok[i[j]])
        if i[j] == "+":
            tok_stream.append(tok[i[j]])
        if i[j] == "-":
            tok_stream.append(tok[i[j]])
        if i[j] == "%":
            tok_stream.append(tok[i[j]])
        if i[j] == "=":
            tok_stream.append(tok.compare)
        if i[j] == ">":
            if len(tok_stream == 0):
                tok_stream.append(tok[i[j]])
            else:
                if tok_stream[-1] == tok.compare:
                    tok_stream[-1] = tok.binding
                else:
                    tok_stream.append(tok[i[j]])
        if i[j] == "<":
            tok_stream.append(tok[i[j]])
        
print(tok_stream)