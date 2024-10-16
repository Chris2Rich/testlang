import argparse
# example syntax
# (i64) dot_product(i64array a, i64array b) => (reduce<+> *) where (len a == len b)
# (ui5array) cypher(ui5array s, i5 n) => (- s / 2 n +)

parser = argparse.ArgumentParser()
parser.add_argument("input")
parser.add_argument("output")

args = parser.parse_args()
i_file = open(args.input, "r")
i_text = i_file.readlines()
i_file.close()

tokens = {
    tok_eof : -1,
    tok_identifier : -2,
    tok_number : -3,
}

id_str = str()
num_val = float()

print(i_text)