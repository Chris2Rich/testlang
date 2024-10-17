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

tok = [
  "eof",
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
    if self.c >= len(s):
      yield tok.eof
    while s[self.c] == " ":
      self.c += 1
    if s[self.c] == "/":
      #skip self.comments
      if s[self.c+1] == "/":
        while s[self.c] != "\n":
          self.c += 1
      else:
        self.c += 1
        yield tok["divide"]
    if s[self.c] == "+":
      self.c += 1
      yield tok["add"]
    if s[self.c] == "-":
      self.c += 1
      yield tok["sutraself.ct"]
    if s[self.c] == "*":
      self.c += 1
      yield tok["multiply"]
    if s[self.c] == "<":
      self.c += 1
      yield tok["left_arr"]
    if s[self.c] == ">":
      self.c += 1
      yield tok ["right_arr"]
    if s[self.c] == "(":
      self.c += 1
      yield tok["left_bra"]
    if s[self.c] == ")":
      self.c += 1
      yield tok["right_bra"]
    if s[self.c] == "{":
      self.c += 1
      yield tok["right_self.cur"]
    if s[self.c] == "}":
      self.c += 1
      yield tok["left_self.cur"]
    if s[self.c] == "=":
      self.c += 1
      if s[self.c+1] == ">":
        self.c += 1
        yield tok["binding"]
      else:
        yield tok["equality"]
    else:
      while not s[self.c] in " +=*/<>(){};:#\"\'" and self.c != len(s):
        i += s[self.c]
        self.c += 1 
      yield i
      self.c += 1

toker = tokenizer()
tok_stream.append(next(toker.Next(), None))
tok_stream.append(next(toker.Next(), None))
tok_stream.append(next(toker.Next(), None))
tok_stream.append(next(toker.Next(), None))
tok_stream.append(next(toker.Next(), None))
tok_stream.append(next(toker.Next(), None))
tok_stream.append(next(toker.Next(), None))
tok_stream.append(next(toker.Next(), None))

print(tok_stream)