import argparse
# example syntax
# (i64) dot_product()

parser = argparse.ArgumentParser()
parser.add_argument("input")
parser.add_argument("output")

args = parser.parse_args()
i_file = open(args.input, "r")
i_text = i_file.readlines()
i_file.close()

print(i_text)