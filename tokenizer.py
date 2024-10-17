import argparse
# example syntax
# i64 dot_product(i64array a, i64array b) => (reduce<+> *) where (len a == len b)
# ui5array cypher(ui5array s, i5 n) => (- s / 2 n +)

parser = argparse.ArgumentParser()
parser.add_argument("input")
parser.add_argument("output")

args = parser.parse_args()
i_file = open(args.input, "r")
s = [j for i in i_file.readlines() for j in i]
i_file.close()

tok = ["eof",
  "identifier",
  "literal",
  "binding",
  "equality",
  
  "add",
  "subtract",
  "multiply",
  "divide",
  "modulo",

  "right_arr",
  "left_arr"]

tok = {tok[-i -1]: i for i in range(-1, -len(tok)-1, -1)}
  

tok_stream = []

class tokenizer:
  c = 0
  i = ""
  def Next():
    try:
      if c == len(s):
        return tok.eof
      while s[c] == " ":
        c += 1
      if s[c] == "/":
        #skip comments
        if s[c+1] == "/":
          while s[c] != "\n":
            c += 1
        else:
          return tok["divide"]
      c += 1
    except:
      raise Exception("Error in tokenization - cursor position: " + c)
    
print(tok)