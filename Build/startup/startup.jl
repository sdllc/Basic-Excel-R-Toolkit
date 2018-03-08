#
# Copyright (c) 2017-2018 Structured Data, LLC
# 
# This file is part of BERT.
#
# BERT is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# BERT is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with BERT.  If not, see <http://www.gnu.org/licenses/>.
#

#
# NOTE: this changes in Julia > 0.6.2, which will come out soon. this 
# is a temporary solution only. 
#
# this is because the old version of with_output_color checked this 
# global flag -- more recent versions look up the value of :color in 
# the IOStream, which we can wrap via IOContext.
#

Base.eval(:(have_color=true));

# =============================================================================
#
# create a module for scoped functions
#
# =============================================================================
module BERT

  """
  EXCEL is the base module for the Excel scripting (COM) interface.
  In the EXCEL module, the `Application` object represents the running
  instance of Excel.

  The EXCEL module also includes enumerations provided by the Application
  object. These are usually used as parameters to functions.

  # Examples
  ```julia-repl
  > EXCEL.Application.get_ActiveSheet()
  COM interface _Worksheet Ptr{UInt64} @0x0000028f380ce350
  ```
  
  ```julia-repl
  > EXCEL.Application.get_ActiveSheet().get_Name()  
  "Sheet1"
  ```    

  ```julia-repl
  > EXCEL.XlBorderWeight.xlThick
  4
  ```
  """
  module EXCEL
    Application = nothing
  end

  # default to text/html. for GR, this generates SVGs. that probably 
  # stops working well for lots of data -- in that case, switch to png.

  DefaultPlotFormat = "text/html"

  import Base.Multimedia: Display, display, displayable

  #
  # this changes in >0.6.2, the base type becomes "AbstractDisplay"
  #
  struct ShellDisplay <: Display
  end

  #
  #
  #
  function display(d::BERT.ShellDisplay, mime::MIME"text/plain", x)
    io = STDOUT
    # write(io, answer_color(d.repl))
    show(IOContext(io, :limit => true), mime, x)
    println(io)
  end
  
  #
  #
  #
  function display(d::BERT.ShellDisplay, mime::MIME"text/markdown", x)
    io = STDOUT
    Base.Markdown.term(io, x)
    println(io) # this might not be necessary for MD
  end

  #
  # unify remote render
  #
  function remote_display(mime_type::String, x)
    buffer = IOBuffer()
    show(buffer, MIME(mime_type), x)
    BERT.Callback("render-mime", mime_type, buffer.data)
  end

  #
  # what to do about html? render inline, pass to console? (...)
  # NOTE: this function was here to support svg graphics, that's now inlined
  #
  #function display(d::BERT.ShellDisplay, mime::MIME"text/html", x)
  #  buffer = IOBuffer()
  #  show(buffer, MIME("text/html"), x)
  #  BERT.Callback("render-mime", "text/html", buffer.data)
  #end

  display(d::BERT.ShellDisplay, mime::MIME"image/png", x) = remote_display(mime(), x)
  display(d::BERT.ShellDisplay, mime::MIME"image/jpeg", x) = remote_display(mime(), x)
  display(d::BERT.ShellDisplay, mime::MIME"image/gif", x) = remote_display(mime(), x)
  display(d::BERT.ShellDisplay, mime::MIME"image/svg+xml", x) = remote_display(mime(), x)
  display(d::BERT.ShellDisplay, x::Base.Markdown.MD) = display(d, MIME("text/markdown"), x)

  #
  # 
  #
  function displayable(d::BERT.ShellDisplay, M::MIME)
    mime_string = string(M)
    return (Base.Multimedia.istextmime(M) 
      || string(M) == "image/png"
      || string(M) == "image/jpeg"
      || string(M) == "image/gif" )
  end

  #
  #
  #
  function display(d::BERT.ShellDisplay, x) 

    # my understanding is we can't switch on a type that's not loaded,
    # so this has to use strings. even in a try/catch block? although 
    # probably string matching is preferable to try/catch anyway.

    if(startswith(string(typeof(x)), "Plots.Plot"))
      # display(d, MIME(BERT.DefaultPlotFormat), x)  
      remote_display(BERT.DefaultPlotFormat, x)
    else
      display(d, MIME("text/plain"), x)  
    end
  end

  #
  #
  #
  pushdisplay(BERT.ShellDisplay());

  #----------------------------------------------------------------------------
  #
  # prints help documentation, as in the julia console. atm we're not 
  # changing prompts, but it works the same way: `?help`
  #
  #----------------------------------------------------------------------------
  function ShellHelp(str)

    # helpmode returns the docs (as markdown), but it also prints
    # some stuff directly (search, plus maybe something else); so
    # we want to evaluate it, then print the result. Markdown.term
    # also has a result, which we want to suppress.

    Markdown.term(STDOUT, Main.eval(Base.Docs.helpmode(str)));
    nothing

  end

  #---------------------------------------------------------------------------- 
  #
  #---------------------------------------------------------------------------- 
  function DisplayError(err)
    Base.with_output_color(print, Base.error_color(), STDERR, "ERROR: "; bold=true)
    showerror(IOContext(STDERR, :limit => true), err, [])
    println(STDERR)
  end

  #---------------------------------------------------------------------------- 
  #
  # read script file and (optionally) log to the console
  #
  #---------------------------------------------------------------------------- 
  function ReadScriptFile(file, notify=false)
    if(notify)
      print("Loading script file: $(Base.text_colors[:cyan])$(file)$(Base.text_colors[:normal])\n");
    end
    include(file)
    nothing
  end

  #---------------------------------------------------------------------------- 
  #
  # banner under regular julia banner
  #
  #---------------------------------------------------------------------------- 
  function Banner()

    version = ENV["BERT_VERSION"];

    print("""
BERT version $(version) (http://bert-toolkit.com).\n
$(Base.text_colors[:light_blue])This is not the default Julia REPL$(Base.text_colors[:normal]). Many things are similar, but some 
things are different. Please send feedback if you have questions or 
comments, and save your work often. \n\n""");


  end

  #---------------------------------------------------------------------------- 
  #
  # returns a list of all functions in Main, formatted as string
  # arrays: [function name, argument name 1, ...].  note that (at least 
  # for now) we don't support named arguments; only ordinal arguments.
  #
  #---------------------------------------------------------------------------- 
  ListFunctions = function()

    # watch out for the case where ans is a function

    function_list = filter(x -> (x != "ans" && getfield(Main, x) isa Function), names(Main)) 
    map(function(x)
      m = match( r"\(([^\(]*)\) in", string(methods(getfield(Main, x))))
      arguments = map(x -> strip(x), split(m[1], ",", keep=false))
      [string(x), arguments...]
    end, function_list )
  end

  #---------------------------------------------------------------------------- 
  #
  # AC function. FIXME: normalize AC between R, Julia (&c)
  #
  #---------------------------------------------------------------------------- 
  Autocomplete = function(buffer, position)
    try
      return Base.REPLCompletions.completions(buffer, position)[1];
    catch
    end
    return nothing;
  end

  #---------------------------------------------------------------------------- 
  #
  # sets callback pointers
  #
  #---------------------------------------------------------------------------- 
  SetCallbacks = function(com_callback::Ptr{Void}, callback::Ptr{Void})

    # clearly I don't understand how julia closures work

    global __callback_pointer = callback
    global __com_callback_pointer = com_callback
    nothing
  end

  #---------------------------------------------------------------------------- 
  #
  # runs code (not COM) callback
  #
  #---------------------------------------------------------------------------- 
  Callback = function(command::String, 
    argument1::Any = nothing, argument2::Any = nothing, argument3::Any = nothing)
    
    ccall(BERT.__callback_pointer, Any, (Cstring, Any, Any, Any), command, 
      argument1, argument2, argument3)
  end

  #---------------------------------------------------------------------------- 
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
  #---------------------------------------------------------------------------- 
  FinalizeCOMPointer = function(x)
    Callback("release-pointer", x.p)
  end

  #---------------------------------------------------------------------------- 
  #
  # NOTE: object with finalizer has to be mutable (?)
  #
  #---------------------------------------------------------------------------- 
  mutable struct FinalizablePointer 
    p::Ptr{UInt64}
    function FinalizablePointer(p)
      instance = new(p)
      finalizer(instance, FinalizeCOMPointer)
      return instance # necessary?
    end
  end

  #---------------------------------------------------------------------------- 
  #
  # creates a type representing a COM interface. this _creates_
  # types, it does not instantiate them; this should only be 
  # called once per type. after that they can be instantiated 
  # directly.
  #
  #---------------------------------------------------------------------------- 
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

  #---------------------------------------------------------------------------- 
  #
  # creates wrappers for COM pointers. types are generated on the fly
  # and stuffed into this namespace (glad that works, btw). subsequent
  # calls return instances. 
  #
  #---------------------------------------------------------------------------- 
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
  # single enum
  #
  macro CreateCOMEnumType(struct_name, descriptor)

    _descriptor = eval(descriptor)
    name, value_list = _descriptor

    sym = Symbol(struct_name)

    return quote
      struct $sym 
        $([Symbol(x[1]) for x in value_list]...)
        function $sym() 
          new($([x[2] for x in value_list]...))
        end
      end
    end

  end

  CreateCOMEnum = function(parent_object, descriptor)

    name, values = descriptor
    struct_name = string("com_enum_", parent_object, "_", name)

    # create the enum type
    if(!isdefined(Symbol(struct_name)))
      eval(:(@CreateCOMEnumType($struct_name, $descriptor)))
    end

    # create instance
    eval(:($(Symbol(struct_name))()))

  end

  #
  # composite/container type
  #
  macro CreateCOMEnumsType(struct_name, name_list, value_list)

    sym = Symbol(struct_name)

    return quote
      struct $sym 
        Application
        $([Symbol(x) for x in name_list]...)
        function $sym(application) 
          new(application, $(value_list...))
        end
      end
    end

  end

  #
  # this is * way * too slow to use. creating lots of structs is painful.
  # not sure if this was caused by defining the structs or by instantiating
  # them (NOTE: we tried both inner constructor and explicit constructor, 
  # both were bad).
  #
  # we have to do this another way.
  #
  CreateCOMEnums = function(parent_object, descriptor, pointer)

    struct_name = string("com_enums_", parent_object)

    # map(x -> x[1], descriptor)
    instance_list = map(x -> CreateCOMEnum(parent_object, x), descriptor)
    name_list = map(x -> x[1], descriptor)

    eval(:(@CreateCOMEnumsType($struct_name, $name_list, $instance_list)))
    eval(:($(Symbol(struct_name))($pointer)))

  end

  #---------------------------------------------------------------------------- 
  # 
  # creates/installs enums as sets of modules
  #
  # this is pretty fast, even if it's rewriting. it's night and day
  # vis a vis using structs. (also love that you can redefine modules).
  #
  # would like to remove the eval function from each module, though.
  #
  #---------------------------------------------------------------------------- 
  CreateEnumModules = function(mod, enums_list)

    # create all modules 
    names = [x[1] for x in enums_list]
    [mod.eval( Expr( :toplevel, :(module $(Symbol(name)); end)))
      for name in names]
    nothing

    # add values
    foreach(function(x)
      name, entries = x
      CreateEnumValues(mod, name, entries)
    end, enums_list)

  end

  #---------------------------------------------------------------------------- 
  #
  # installs values in a module enum
  #
  # also pretty fast. much better. could probably speed it up by
  # consolidating all the evals. (FIXME: maybe via quote?)
  #
  #---------------------------------------------------------------------------- 
  CreateEnumValues = function(parent_module, module_name, values)
    mod = getfield(parent_module, Symbol(module_name))
    [eval(mod, :($(Symbol(x[1])) = $(x[2]))) for x in values]
  end

  #---------------------------------------------------------------------------- 
  #
  # installs the root "Application" pointer in the EXCEL module
  #
  #---------------------------------------------------------------------------- 
  InstallApplicationPointer = function(descriptor)

    global ApplicationDescriptor = descriptor # for dev/debug
    local app = CreateCOMType(descriptor)

    # set pointer
    EXCEL.eval(:(Application = $(app)))

    # use module system
    CreateEnumModules(EXCEL, descriptor[4])
   
    nothing

  end

end # module BERT

#
# hoist into namespace
#
using BERT.EXCEL

#
# banners: print julia banner from the REPL, then our banner
#
Base.banner();
BERT.Banner();
