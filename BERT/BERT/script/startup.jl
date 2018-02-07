
# =============================================================================
#
# create a module for scoped functions
#
# =============================================================================
module BERT

  EXCEL = nothing

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

$(colors[:green])BERT$(colors[:normal]) Julia shell version 0.1 BETA. $(colors[:reverse])This is not the default Julia shell$(colors[:normal]). Many
things are similar, but some things are different. Please send feedback if you
have questions or comments, and save your work often. 


""");

  end

  #
  # this function gets a list of all functions in Main, returning function 
  # name and list of argument names. note that (at least for now) we don't
  # support named arguments; only ordinal arguments.
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

  #
  #
  #
  Autocomplete = function(buffer, position)
    try
      return Base.REPLCompletions.completions(buffer, position)[1];
    catch
    end
    return nothing;
  end

  #
  #
  #
  SetCallbacks = function(com_callback::Ptr{Void}, callback::Ptr{Void})

    # clearly I don't understand how julia closures work

    global __callback_pointer = callback
    global __com_callback_pointer = com_callback
    nothing
  end

  #
  #
  #
  Callback = function(command::String, arguments::Any = nothing)
    ccall(BERT.__callback_pointer, Any, (Cstring, Any), command, arguments)
  end

  #
  # calls release on a COM pointer. 
  #
  # FIXME: we are not really locking down these pointers, so they might 
  # get copied (you can certainly do that expressly if you want). we might
  # think about adding a callback in the ctor so we're aware of extra 
  # copies (possibly calling addref, or maybe using a second-order refcount
  # on top).
  # 
  # it's probably not possible to perfectly lock these, but we might do a 
  # better job of hiding. 
  #
  FinalizeCOMPointer = function(x)
    Callback("release-pointer", x.p)
  end

  #
  # NOTE: object with finalizer has to be mutable (?)
  #
  mutable struct FinalizablePointer 
    p::Ptr{UInt64}
    function FinalizablePointer(p)
      instance = new(p)
      finalizer(instance, FinalizeCOMPointer)
      return instance
    end
  end

  #
  # creates a type representing a COM interface. this _creates_
  # types, it does not instantiate them; this should only be 
  # called once per type. after that they can be instantiated 
  # directly.
  #
  macro CreateCOMTypeInternal(struct_name, descriptor)

    local descriptor_ = eval(descriptor)
    local functions_list = descriptor_[3]

    local translate_name = function(x)
      name, call_type = x
      if call_type == "get"
        return Symbol("get_", name)
      elseif call_type == "put"    
        return Symbol("put_", name)
      else
        return Symbol(name)
      end
    end

    local struct_type = quote
      struct $struct_name 
        _pointer
        $([translate_name(x) for x in functions_list]...)
      end
    end

    return struct_type

  end

  #
  # creates wrappers for COM pointers. types are generated on the fly
  # and stuffed into this namespace (glad that works, btw). subsequent
  # calls return instances. 
  #
  CreateCOMType = function(descriptor)

    name, pointer, functions_list = descriptor
    sym = Symbol("com_interface_", name)
    if(!isdefined(BERT, sym))
      eval(:(@CreateCOMTypeInternal($sym, $descriptor)))
      eval(:(Base.show(io::IO, object::$(sym)) = print(string("COM interface ", $(name), " ", object._pointer.p))))
    end

    local functions = map(x -> function(args...)
      return eval(:( ccall(BERT.__com_callback_pointer, Any, (UInt64, Cstring, Cstring, UInt32, Any), 
        $(pointer), $(x[1]), $(x[2]), $(x[3]), [$(args)...])))
    end, functions_list)
    
    return eval(:( $(sym)(FinalizablePointer($(pointer)), $(functions...))))

  end

  #
  # type to match R 
  #
  struct ExcelType
    Application
  end

  #
  # installs the root "Application" pointer and creates the EXCEL 
  # wrapper object. 
  # 
  # FIXME: enums
  #
  InstallApplicationPointer = function(descriptor)
    global ApplicationDescriptor = descriptor # for dev/debug
    global EXCEL = ExcelType(CreateCOMType(descriptor))
    nothing
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
