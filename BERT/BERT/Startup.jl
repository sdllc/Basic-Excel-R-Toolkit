
# =============================================================================
#
# create a module for scoped functions
#
# =============================================================================
module BERT

  #---------------------------------------------------------------------------- 
  # this function gets a list of all functions in Main,
  # returning function name and list of argument names.
  # note that (at least for now) we don't support named
  # arguments; only ordinal arguments.
  #---------------------------------------------------------------------------- 
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

  #---------------------------------------------------------------------------- 
  # testing. this is a patched-up version of the socket repl
  # function from julia base
  #---------------------------------------------------------------------------- 
  RunSocketREPL = function()
	  server = listen(9999)
	  client = TCPSocket()
	  while isopen(server)
		  err = Base.accept_nonblock(server, client)
		  if err == 0
			  Base.REPL.run_repl(client)
			  client = TCPSocket()
		  elseif err != Base.UV_EAGAIN
			  uv_error("accept", err)
		  else
			  Base.stream_wait(server, server.connectnotify)
		  end
	  end
  end

end

