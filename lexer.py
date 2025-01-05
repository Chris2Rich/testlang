import argparse
import collections

parser = argparse.ArgumentParser()
# parser.add_argument("input")
# parser.add_argument("output")

args = parser.parse_args()
args.input = "source7.txt"
args.output = "out.txt"

i_file = open(args.input, "r")
s = [j for i in i_file.readlines() for j in i]
s.append("\n")
i_file.close()

tok = [
  "eof",
  "id",
  "str",
  "arr",
  "num",
  "bnd",
  "nl",
  
  #stack ops
  "pop",
  "flip",
  "dupe",

  #logic ops
  "not",

  #bit ops
  "bnt",
  "ban",
  "bor",
  "bxr",

  #binary ops
  "add",
  "sub",
  "mul",
  "div",
  "mod",
  "equ",
  "rsh",
  "lsh",

  #syntax
  "rarr",
  "larr",
  "rbra",
  "lbra",
  "rsqu",
  "lsqu",
  "rcur",
  "lcur"
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
          yield (s[self.c-1], tok["div"])

      if s[self.c] == "\n":
        self.c += 1
        yield (s[self.c-1], tok["nl"])

      if s[self.c] == "+":
        self.c += 1
        yield (s[self.c-1], tok["add"])
      if s[self.c] == "-":
        self.c += 1
        yield (s[self.c-1], tok["sub"])
      if s[self.c] == "*":
        self.c += 1
        yield (s[self.c-1], tok["mul"])

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
        yield (s[self.c-1], tok["ban"])
      if s[self.c] == "|":
        self.c += 1
        yield (s[self.c-1], tok["bor"])
      if s[self.c] == "^":
        self.c += 1
        yield (s[self.c-1], tok["bxr"])

      if s[self.c] == "~":
        self.c += 1
        yield (s[self.c-1], tok["bnt"])
      if s[self.c] == "<":
        self.c += 1
        if s[self.c] == "<":
          self.c += 1
          yield(s[self.c-2] + s[self.c-1], tok["lsh"])
        else:
          yield (s[self.c-1], tok["larr"])
      if s[self.c] == ">":
        self.c += 1
        if s[self.c] == ">":
          self.c += 1
          yield(s[self.c-2] + s[self.c-1], tok["rsh"])
        else:
          yield (s[self.c-1], tok["rarr"])
      if s[self.c] == "(":
        self.c += 1
        yield (s[self.c-1], tok["lbra"])
      if s[self.c] == ")":
        self.c += 1
        yield (s[self.c-1], tok["rbra"])
      if s[self.c] == "[":
        self.c += 1
        yield (s[self.c-1], tok["lsqu"])
      if s[self.c] == "]":
        self.c += 1
        yield (s[self.c-1], tok["rsqu"])
      if s[self.c] == "{":
        self.c += 1
        yield (s[self.c-1], tok["rcur"])
      if s[self.c] == "}":
        self.c += 1
        yield (s[self.c-1], tok["lcur"])

      if s[self.c] == "=":
        self.c += 1
        if s[self.c] == ">":
          self.c += 1
          yield (s[self.c-2] + s[self.c-1], tok["bnd"])
        else:
          yield (s[self.c-1], tok["equ"])
      else:
        while not s[self.c] in " \n+=*/<>(){}[],;:#!^&|" and self.c != len(s):
          i += s[self.c]
          self.c += 1
        if i.isnumeric() or (i.replace(".", "").isnumeric() and i.count(".") == 1):
          yield (i, tok["num"])
        elif (i[0] == "\"" and i[-1] == "\"") or (i[0] == "\'" and i[-1] == "\'"):
          yield (i, tok["str"])
        else:
          yield (i, tok["id"])
        self.c += 1
    except Exception as err:
      print(f"Error on character: {self.c} => \n {err}")
      raise

tokenizer = Tokenizer()
t = next(tokenizer.Next(), None)
while t != ("eof", tok["eof"]):
    tok_stream.append(t)
    t = next(tokenizer.Next(), None)

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
      if tok_stream[self.c][1] == "id":
        if self.c != len(tok_stream):
          if tok_stream[self.c + 1][1] == "bnd":
            root = tok_stream[self.c][0]
            self.identifiers.update({root: None})
            self.call_tree.update({root: []})
            sub = []
            temp = self.c + 2
            while temp != len(tok_stream) and tok_stream[temp][1] != "nl":
              if tok_stream[temp][1] == "id":
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

class ArrParser():
  c = 0
  shape = [[], []]
  real_shape = []

  def Next(self):
    if tok_stream[self.c][1] == tok["rsqu"]:
      tmp = self.c
      level = 0
      while tok_stream[tmp][1] == "rsqu":
        level += 1
        tmp += 1
      if level == self.shape[0][0]:
        ress = self.shape[1][self.shape[0].index(1)]
        resf = tok_stream.index(("]", tok["rsqu"]), ress)
        base = abs(ress - resf + 1)
        self.real_shape.append(base)

        for i in range(2, self.shape[0][0]+1):
          self.real_shape.append(self.shape[0].count(i-1) // self.shape[0].count(i))

        data = []
        for i in range(self.shape[1][0], self.c):
          if tok_stream[i][0] != "[" and tok_stream[i][0] != "]":
                data.append(tok_stream[i][0])

        del tok_stream[self.shape[1][0]: self.c + self.shape[0][0]]
        tok_stream.insert(self.shape[1][0], (self.real_shape[::-1], data, tok["arr"]))

        self.c = self.shape[1][0]
        self.shape = [[],[]]
        self.real_shape = []

    if tok_stream[self.c][1] == tok["lsqu"]:
      tmp = self.c
      level = 0
      while tok_stream[tmp][1] == tok["lsqu"]:
        level += 1
        tmp += 1
      self.shape[0].append(level)
      self.shape[1].append(self.c)
    if self.c == len(tok_stream) - 1:
      yield 0
    else:
      self.c += 1
      yield 1
            
arrparser = ArrParser()
i = 1
while i != 0:
  i = next(arrparser.Next(), None)

o_file = open(args.output, "w")
for i in tok_stream:
  o_file.write(str(i) + "\n")
o_file.close()