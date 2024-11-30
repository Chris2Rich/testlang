all functions are permeable 
eg 
- \* [1,2,3,4] 2
\# -> [2,4,6,8]

as * is a dyadic function, each element in the array was multiplied by 2

- pow [1,2,3,4,5] 2
\# -> [1,4,9,16,25]

- pow 2 [1,2,3,4,5]
\# -> [2,4,8,16,25]

in case 1, as the array is the first operator, pow (i, 2) is applied where i is an element in the array
in case 2, as the array is the second operator, pow(2, i) is applied which creates a new array of shape (5)

# What about more complex permeation?

as a general rule, when an nadic function that applies to scalars is applied to n arrays of the same dimension, then the result will be a matrix of shape n where each element is the result of the function being applied itemwise

eg
- \* [1,2,3] [4,5,6]
\# -> [4,10,18]

- \* [1,2] [4,5,6]
\# -> error: bad shape (cannot permeate dyadic "*" over 1array shape "2" and 1array shape "3")

when the previous rule but work but the dimension of one array is higher, the result is if the smaller array was repeated until the shape aligned in the missing dimension. this is also true for scalars

eg
- \+ [[1,2,3,4], [5,6,7,8]] [1,2,3,4]
\# -> [[2,4,6,8], [6,8,10,12]]