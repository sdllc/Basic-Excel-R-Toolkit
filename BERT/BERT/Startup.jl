
print("ZERP\n");

x = 100;

zed = function(a)
	a*a+a
	end

twoargs(arg1, arg2) = arg1 + arg2

goon() = 12

module BERT

  # 
  # this function gets a list of all functions in Main,
  # returning function name and list of argument names.
  # note that (at least for now) we don't support named
  # arguments; only ordinal arguments.
  #
  ListFunctions = function()
    function_list = []
    for name in names(Main) # these are symbols
      if string(name) != "ans"
        field = getfield(Main, name)
        if isa(field, Function)
          m = match( r"\(([^\(]*)\) in", string(methods(field)))
          if m != nothing 
            arguments = map( x -> strip(x), split(m[1], ",", keep=false))
            function_desc = append!([string(name)], arguments)
            push!(function_list, function_desc)
          end
        end
      end
    end
    function_list
  end

end
