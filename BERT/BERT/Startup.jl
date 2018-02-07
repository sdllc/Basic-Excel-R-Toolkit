
# =============================================================================
#
# create a module for scoped functions
#
# =============================================================================
module BERT

  struct ExcelType
    Application
  end

  EXCEL = nothing
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

  #---------------------------------------------------------------------------- 
  #---------------------------------------------------------------------------- 
  Autocomplete = function(buffer, position)
    try
      return Base.REPLCompletions.completions(buffer, position)[1];
    catch
    end
    return nothing;
  end

  SetCallbacks = function(com_callback::UInt64, callback::UInt64)
    global __callback_pointer = Ptr{UInt64}(callback)
    global __com_callback_pointer = Ptr{UInt64}(com_callback)
    nothing
  end

  Callback = function(command::String, arguments::Any = nothing)
    ccall(BERT.__callback_pointer, Void, (Cstring, Any), command, arguments)
  end

  FinalizeCOMPointer = function(x)
    Callback("release-pointer", x)
  end

  mutable struct FinalizablePointer 
    p::Ptr{UInt64}
    function FinalizablePointer(p)
      instance = new(p)
      finalizer(instance, FinalizeCOMPointer)
      return instance
    end
  end

  CallCOMFunction = function(p, name, call_type, index, args)
    # ccall( BERT.__callback_pointer, Void, (Any,), [args...])
    ccall(BERT.__com_callback_pointer, Any, (UInt64, Cstring, Cstring, UInt32, Any), 
      p, name, call_type, index, args)
  end

  #
  # creates a type representing a COM interface. this _creates_
  # types, it does not instantiate them; this should only be 
  # called once per type. they can be instaniated directly.
  # WIP
  #
  macro CreateCOMTypeInternal(struct_name, descriptor)
 
    local descriptor_ = eval(descriptor)
    local p = descriptor_[2]
    local functions = map( x -> function(args...)
      return CallCOMFunction(p, x[1], x[2], x[3], [args...])
      end, descriptor_[3] )

    local translate_name = function(x)
      if x[2] == "get"
        return string("get_", x[1])
      elseif x[2] == "put"    
        return string("put_", x[1])
      else
        return x[1]
      end
    end

    local struct_type = quote
      struct $struct_name 
        _pointer::UInt64
        $([Symbol(translate_name(x)) for x in descriptor_[3]]...)
        $struct_name(p) = new(p, $(functions...))
      end
    end

    ## print("Created type ", struct_name, "\n")
    return struct_type

  end

  CreateCOMType = function(descriptor)
    sym = Symbol("com_interface_", descriptor[1])
    if(!isdefined(BERT, sym))
      eval(:(@CreateCOMTypeInternal($sym, $descriptor)))
      eval(:(Base.show(io::IO, z::$(sym)) = print(string("COM interface ", $(descriptor[1]), " @", z._pointer))))
    end
    return eval(:( $(sym)($(descriptor[2]))))
  end

  InstallApplicationPointer = function(descriptor)
    global DX = descriptor
    application = CreateCOMType(descriptor)
    global EXCEL = ExcelType(application)
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
# banners: print julia banner from the REPL, then our banner
#
Base.banner();
BERT.Banner();
