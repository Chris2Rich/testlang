import argparse
# example syntax
# i64 dot_product(i64array a, i64array b) => {reduce<+> *}

parser = argparse.ArgumentParser()
parser.add_argument("input")
parser.add_argument("output")

args = parser.parse_args()
i_file = open(args.input, "r")
s = [j for i in i_file.readlines() for j in i]
i_file.close()

tok = ["eof",
  "identifier",
  "string_literal",
  "number_literal",
  
  "binding",
  "pop",
  "flip",
  "dupe",

  "add",
  "subtract",
  "multiply",
  "divide",
  "modulo",
  "equality",

  "right_arr",
  "left_arr",
  "right_bra",
  "left_bra",
  "right_cur",
  "left_cur"
  ]

tok = {tok[-i -1]: i for i in range(-1, -len(tok)-1, -1)}
  

tok_stream = []

class tokenizer:
  c = 0
  def Next(self):
    i = ""
    try:
      if c == len(s):
        yield tok.eof
      while s[c] == " ":
        c += 1
      if s[c] == "/":
        #skip comments
        if s[c+1] == "/":
          while s[c] != "\n":
            c += 1
        else:
          yield tok["divide"]
      if s[c] == "+":
        yield tok["add"]
      if s[c] == "-":
        yield tok["sutract"]
      if s[c] == "*":
        yield tok["multiply"]
      if s[c] == "<":
        yield tok["left_arr"]
      if s[c] == ">":
        yield tok ["right_arr"]
      if s[c] == "(":
        yield tok["left_bra"]
      if s[c] == ")":
        yield tok["right_bra"]
      if s[c] == "{":
        yield tok["right_cur"]
      if s[c] == "}":
        yield tok["left_cur"]
      if s[c] == "=":
        if s[c+1] == ">":
          c += 1
          yield tok["binding"]
        else:
          yield tok["equality"]
      else:
        while s[c] != " ":
          i += s[c]
          c += 1 
        yield i
      c += 1
    except:
      raise Exception("Error in tokenization - cursor position: " + c)

toker = tokenizer()
tok_stream.append(next(toker.Next(), None))
print(tok_stream)