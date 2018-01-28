
# =============================================================================
#
# create a module for scoped functions
#
# =============================================================================
module BERT

  EXCEL = 102.3
  PX = nothing
  DX = nothing

  #---------------------------------------------------------------------------- 
  # banner under regular julia banner
  #---------------------------------------------------------------------------- 

  Banner = function()

    const colors = Base.AnyDict(
      :black         => "\033[30m",
      :red           => "\033[31m",
      :green         => "\033[32m",
      :yellow        => "\033[33m",
      :blue          => "\033[34m",
      :magenta       => "\033[35m",
      :cyan          => "\033[36m",
      :white         => "\033[37m",
      :light_black   => "\033[90m", # gray
      :light_red     => "\033[91m",
      :light_green   => "\033[92m",
      :light_yellow  => "\033[93m",
      :light_blue    => "\033[94m",
      :light_magenta => "\033[95m",
      :light_cyan    => "\033[96m",
      :red_bg        => "\033[41m",
      :green_bg      => "\033[42m",
      :yellow_bg     => "\033[43m",
      :blue_bg       => "\033[44m",
      :normal        => "\033[0m",
      :default       => "\033[39m",
      :bold          => "\033[1m",
      :underline     => "\033[4m",
      :blink         => "\033[5m",
      :reverse       => "\033[7m",
      :hidden        => "\033[8m",
      :nothing       => "",
    );

    print("""

BERT Julia shell version 0.1 BETA. $(colors[:reverse])This is not the default Julia shell$(colors[:normal]). Many
things are similar, but some things are different. Please send feedback if you
have questions or comments, and save your work often. 

""");

  end

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

  InstallApplicationPointer = function(key, descriptor)
    
    len = length(descriptor)
    for i = 1:len
      if descriptor[i].parameters[1] == "interface"
        print("iface: ", descriptor[i].parameters[2], "\n");
      elseif descriptor[i].parameters[1] == "functions"
        print(length(descriptor[i].parameters[2]), " functions in interface\n" )
      elseif descriptor[i].parameters[1] == "enums"
        print(length(descriptor[i].parameters[2]), " enums in interface\n" )
      end

    end

    print("key ", key)
    global PX = key
    global DX = descriptor
    nothing
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

#
# hoist into namespace
#
using BERT.EXCEL

#
# banners
#
Base.banner();
BERT.Banner();
