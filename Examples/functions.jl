
"""
add all arguments. flatten ranges (arrays)

"""
function TestAdd(a...)
  sum(collect(Base.Iterators.flatten(a)))
end

"""
eigenvalues for matrix. returns vector

"""
function EigenValues(mat)
  eigvals(mat)
end

