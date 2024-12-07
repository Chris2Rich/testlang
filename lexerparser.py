import argparse
import collections

parser = argparse.ArgumentParser()
# parser.add_argument("input")
# parser.add_argument("output")

args = parser.parse_args()
args.input = "source1.txt"
args.output = "out.txt"

i_file = open(args.input, "r")
s = [j for i in i_file.readlines() for j in i]
s.append("\n")
i_file.close()

tok = [
  "eof",
  "identifier",
  "string_literal",
  "number_literal",
  "binding",
  "newline",
  
  #stack ops
  "pop",
  "flip",
  "dupe",

  #logic ops
  "not",

  #bit ops
  "bit_not",
  "bit_and",
  "bit_or",
  "bit_xor",

  #binary ops
  "add",
  "subtract",
  "multiply",
  "divide",
  "modulo",
  "equality",
  "right_shift",
  "left_shift",

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

class Tokenizer:
  c = 0
  def Next(self):
    try:
      i = ""
      if self.c >= len(s):
        yield ("eof", tok["eof"])
      while s[self.c] == " " or s[self.c] == ",":
        self.c += 1
      if s[self.c] == "/":
        #skip comments
        if s[self.c+1] == "/":
          while s[self.c] != "\n":
            self.c += 1
        else:
          self.c += 1
          yield (s[self.c-1], tok["divide"])

      if s[self.c] == "\n":
        self.c += 1
        yield (s[self.c-1], tok["newline"])

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

      if s[self.c] == "!":
        self.c += 1
        yield (s[self.c-1], tok["not"])
      if s[self.c] == "&":
        self.c += 1
        yield (s[self.c-1], tok["bit_and"])
      if s[self.c] == "|":
        self.c += 1
        yield (s[self.c-1], tok["bit_or"])
      if s[self.c] == "^":
        self.c += 1
        yield (s[self.c-1], tok["bit_xor"])

      if s[self.c] == "~":
        self.c += 1
        yield (s[self.c-1], tok["bit_not"])
      if s[self.c] == "<":
        self.c += 1
        if s[self.c] == "<":
          self.c += 1
          yield(s[self.c-2] + s[self.c-1], tok["left_shift"])
        else:
          yield (s[self.c-1], tok["left_arr"])
      if s[self.c] == ">":
        self.c += 1
        if s[self.c] == ">":
          self.c += 1
          yield(s[self.c-2] + s[self.c-1], tok["right_shift"])
        else:
          yield (s[self.c-1], tok["right_arr"])
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
        while not s[self.c] in " \n+=*/<>(){}[],;:#!^&|" and self.c != len(s):
          i += s[self.c]
          self.c += 1
        if i.isnumeric() or (i.replace(".", "").isnumeric() and i.count(".") == 1):
          yield (i, tok["number_literal"])
        elif (i[0] == "\"" and i[-1] == "\"") or (i[0] == "\'" and i[-1] == "\'"):
          yield (i, tok["string_literal"])
        else:
          yield (i, tok["identifier"])
        self.c += 1
    except Exception as err:
      print(f"Error on character: {self.c} => \n {err}")
      raise

tokenizer = Tokenizer()
t = next(tokenizer.Next(), None)
while t != ("eof", tok["eof"]):
    tok_stream.append(t)
    t = next(tokenizer.Next(), None)

class Node:
  def __init__(self, data):
    self.children = []
    self.data = data

stack = []

#inlines all bindings to make program easier to run
class Inliner:
  c = 0

  identifiers = {}
  call_tree = {}

  def explore_call_tree(self, t):
    #BFS always finds first loop
    visited=[]
    queue = []
    for i in self.call_tree[t]:
      queue.append(i)
    while len(queue) > 0:
      if t in queue:
        return queue
      else:
        a = queue.pop()
        if a not in self.call_tree:
          pass
        else:
          for i in self.call_tree[a]:
            queue.append(i)
    return False

  def Next(self):
    try:
      if tok_stream[self.c][1] == "identifier":
        if self.c != len(tok_stream):
          if tok_stream[self.c + 1][1] == "binding":
            root = tok_stream[self.c][0]
            self.identifiers.update({root: None})
            self.call_tree.update({root: []})
            sub = []
            temp = self.c + 2
            while temp != len(tok_stream) and tok_stream[temp][1] != "newline":
              if tok_stream[temp][1] == "identifier":
                self.call_tree[root].append(tok_stream[temp][0])
              
              sub.append(tok_stream[temp])
              temp += 1
            if self.explore_call_tree(tok_stream[self.c][0]):
              raise Exception(f"Error: function \"{root}\" is recursive")

            self.identifiers.update({tok_stream[self.c][0]: sub})
            del tok_stream[self.c:temp+1]
            self.c = -1
          else:
            if tok_stream[self.c][0] in self.identifiers:
              id = tok_stream[self.c][0]
              for i in self.identifiers[id][::-1]:
                tok_stream.insert(self.c, i)
              self.c += len(self.identifiers[id])
              del tok_stream[self.c]
              self.c -= len(self.identifiers[id]) + 1
      if self.c == len(tok_stream) - 1:
        yield 0
      else:
        self.c += 1
        yield 1
                      
    except Exception as err:
      print(f"Error on token: {self.c} => \n {err}")
      raise

inliner = Inliner()
i = 1
while i != 0:
  i = next(inliner.Next(), None)
print(tok_stream)