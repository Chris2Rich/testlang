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
  
  #stack ops
  "pop",
  "flip",
  "dupe",

  #math ops
  "add",
  "subtract",
  "multiply",
  "divide",
  "modulo",
  "equality",

  #syntax
  "right_arr",
  "left_arr",
  "right_bra",
  "left_bra",
  "right_squ",
  "left_squ",
  "right_cur",
  "left_cur"
  ]

tok = {tok[-i -1]: tok[-i -1] for i in range(-1, -len(tok)-1, -1)}
  

tok_stream = []

class tokenizer:
  c = 0
  def Next(self):
    i = ""
    if self.c >= len(s):
      yield ("eof", tok["eof"])
    while s[self.c] == " ":
      self.c += 1
    if s[self.c] == "/":
      #skip self.comments
      if s[self.c+1] == "/":
        while s[self.c] != "\n":
          self.c += 1
      else:
        self.c += 1
        yield (s[self.c-1], tok["divide"])
    if s[self.c] == "+":
      self.c += 1
      yield (s[self.c-1], tok["add"])
    if s[self.c] == "-":
      self.c += 1
      yield (s[self.c-1], tok["subtract"])
    if s[self.c] == "*":
      self.c += 1
      yield (s[self.c-1], tok["multiply"])
    if s[self.c] == ";":
      self.c += 1
      yield (s[self.c-1], tok["pop"])
    if s[self.c] == ":":
      self.c += 1
      yield (s[self.c-1], tok["flip"])
    if s[self.c] == "#":
      self.c += 1
      yield (s[self.c-1], tok["dupe"])
    if s[self.c] == "<":
      self.c += 1
      yield (s[self.c-1], tok["left_arr"])
    if s[self.c] == ">":
      self.c += 1
      yield (s[self.c-1], tok ["right_arr"])
    if s[self.c] == "(":
      self.c += 1
      yield (s[self.c-1], tok["left_bra"])
    if s[self.c] == ")":
      self.c += 1
      yield (s[self.c-1], tok["right_bra"])
    if s[self.c] == "[":
      self.c += 1
      yield (s[self.c-1], tok["left_squ"])
    if s[self.c] == "]":
      self.c += 1
      yield (s[self.c-1], tok["right_squ"])
    if s[self.c] == "{":
      self.c += 1
      yield (s[self.c-1], tok["right_cur"])
    if s[self.c] == "}":
      self.c += 1
      yield (s[self.c-1], tok["left_cur"])
    if s[self.c] == "=":
      self.c += 1
      if s[self.c] == ">":
        self.c += 1
        yield (s[self.c-2] + s[self.c-1], tok["binding"])
      else:
        yield (s[self.c-1], tok["equality"])
    else:
      while not s[self.c] in " +=*/<>(){}[],;:#\"\'" and self.c != len(s):
        i += s[self.c]
        self.c += 1 
      yield (i, tok["identifier"])
      self.c += 1

toker = tokenizer()
t = next(toker.Next(), None)
while t != ("eof", tok["eof"]):
    tok_stream.append(t)
    t = next(toker.Next(), None)

print(tok_stream)