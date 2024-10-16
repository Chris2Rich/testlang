import argparse
# example syntax
# i64 dot_product(i64array a, i64array b) => (reduce<+> *) where (len a == len b)
# ui5array cypher(ui5array s, i5 n) => (- s / 2 n +)

parser = argparse.ArgumentParser()
parser.add_argument("input")
parser.add_argument("output")

args = parser.parse_args()
i_file = open(args.input, "r")
i_text = [j for i in i_file.readlines() for j in i]
i_file.close()

tok = {
    "eof" : -1,
    "identifier" : -2,
    "literal" : -3,
    "binding" : -4,
    "equality": -5,
    "hi_ord_f": -6,
    
    "+" : -7,
    "-" : -8,
    "*" : -9,
    "/" : -10,
    "%" : -11,
    ">" : -12,
    "<" : -13
}

tok_stream = []

class tokenizer:
    cursor = 0
    def Next():
        cursor += 1
        
print(i_text)