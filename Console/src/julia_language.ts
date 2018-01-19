/// <reference path="../node_modules/monaco-editor/monaco.d.ts" />

import IRichLanguageConfiguration = monaco.languages.LanguageConfiguration;
import ILanguage = monaco.languages.IMonarchLanguage;

export const conf: IRichLanguageConfiguration = {
	wordPattern: /(-?\d*\.\d\w*)|([^\`\~\!\@\#%\^\&\*\(\)\=\$\-\+\[\{\]\}\\\|\;\:\'\"\,\.\<\>\/\?\s]+)/g,
	comments: {
		blockComment: ['###', '###'],
		lineComment: '#'
	},
	brackets: [
		['{', '}'],
		['[', ']'],
		['(', ')']
	],
	autoClosingPairs: [
		{ open: '{', close: '}' },
		{ open: '[', close: ']' },
		{ open: '(', close: ')' },
		{ open: '"', close: '"' },
		{ open: '\'', close: '\'' },
	],
	surroundingPairs: [
		{ open: '{', close: '}' },
		{ open: '[', close: ']' },
		{ open: '(', close: ')' },
		{ open: '"', close: '"' },
		{ open: '\'', close: '\'' },
	]
};

export const language = <ILanguage>{

  defaultToken: '',
	ignoreCase: false,
  tokenPostfix: '.jl',
    
  // Set defaultToken to invalid to see what you do not tokenize yet
  // defaultToken: 'invalid',

  // thanks to https://groups.google.com/forum/#!topic/julia-users/tisM_9sKPCc

  keywords: [
    'function', 'global', 'for', 'end', 'while', 'if', 'else', 'elseif', 'break',
    'switch', 'case', 'otherwise', 'try', 'catch', 'end', 'const', 'immutable',
    'import', 'importall', 'export', 'type', 'typealias', 'return', 'true', 
    'false', 'macro', 'quote', 'in', 'abstract', 'module', 'using', 'continue', 
    'ifelse', 'do', 'eval', 'let', 'finally', 'throw'
  ],

  typeKeywords: [
    'Array', 'String', 'Bool', 'Number', 'Int', 'Integer', 'Real', 'Complex', 
    'FloatingPoint', 'Float64', 'Float32', 'Int8', 'Int16', 'Int32', 'Int64', 
    'Rational', 'AbstractArray', 'Unsigned', 'Signed', 'Uint', 'Uint8', 'Uint16', 
    'Uint32', 'Uint64', 'Vector', 'AbstractVector', 'Matrix', 'AbstractMatrix', 
    'Type', 'IO', 'Any', 'ASCIIString', 'Union', 'Dict', 'Function', 'SubArray', 
    'Range', 'Range1', 'Symbol', 'Expr'
  ],

  keywords2: [
    'cell', 'collect', 'filter', 'merge', 'divrem', 'hex', 'dec', 'oct', 'base', 
    'int', 'round', 'cmp', 'float', 'linspace', 'fill',     'start', 'done', 'tuple', 
    'minimum', 'maximum', 'count', 'index', 'append', 'push', 'pop', 'shift', 
    'unshift', 'insert', 'splice', 'reverse', 'sort', 'zip', 'length', 'delete', 
    'copy', 'haskey', 'keys', 'values', 'get', 'getkey', 'Set', 'isa', 'issubset', 
    'intersect', 'setdiff', 'symdiff', 'complement', 'print', 'printf', 'println', 
    'sprintf', 'join', 'utf8', 'char', 'search', 'rsearch', 'beginswith', 'endswith',
    'replace', 'lowercase', 'uppercase', 'ucfirst', 'lcfirst', 
    'split', 'rsplit', 'chop', 'chomp', 'lpad', 'rpad', 'lstrip', 'rstrip', 
    'strip', 'isalnum', 'isalpha', 'isascii', 'isblank', 'iscntrl', 'isdigit', 
    'isgraph', 'islower', 'isprint', 'ispunct', 'isspace', 'isupper', 'isxdigit', 
    'match', 'captures', 'offset', 'offsets', 'matchall', 'eachmatch', 'hcat', 
    'vcat', 'hvcat', 'reshape', 'deepcopy', 'similar', 'reinterpret', 'map', 
    'reduce', 'mapreduce', 'DataArray', 'DataFrame', 'removeNA', 'replaceNA', 
    'colnames', 'head', 'tail', 'describe', 'join', 'groupby', 'by', 'stack', 
    'readtable', 'readcsv', 'readdlm', 'writetable', 'writecsv', 'writedlm', 
    'require', 'reload', 'include', 'evalfile', 'cd', 'open', 'write', 'close', 
    'position', 'seek', 'seekstart', 'seekend', 'skip', 'isopen', 'eof', 
    'isreadonly', 'ltoh', 'htol', 'serialize', 'deserialize', 'download'
  ],

  operators: [
    '=', '>', '<', '!', '~', '?', ':', '==', '<=', '>=', '!=',
    '&&', '||', '++', '--', '+', '-', '*', '/', '&', '|', '^', '%',
    '<<', '>>', '>>>', '+=', '-=', '*=', '/=', '&=', '|=', '^=',
    '%=', '<<=', '>>=', '>>>='
  ],

  // we include these common regular expressions
  symbols:  /[=><!~?:&|+\-*\/\^%]+/,

  // C# style strings
  escapes: /\\(?:[abfnrtv\\"']|x[0-9A-Fa-f]{1,4}|u[0-9A-Fa-f]{4}|U[0-9A-Fa-f]{8})/,

  // The main tokenizer for our languages
  tokenizer: {
    root: [
      // identifiers and keywords
      [/[a-z_$][\w$]*/, { cases: { 
        '@keywords': 'keyword',
        '@typeKeywords': 'type',
        '@default': 'identifier'
        } }],
      [/[A-Z][\w\$]*/, 'type.identifier' ],  // to show class names nicely

      // whitespace
      { include: '@whitespace' },

      // delimiters and operators
      [/[{}()\[\]]/, '@brackets'],
      [/[<>](?!@symbols)/, '@brackets'],
      [/@symbols/, { cases: { '@operators': 'operator',
                              '@default'  : '' } } ],

      // @ annotations.
      // As an example, we emit a debugging log message on these tokens.
      // Note: message are supressed during the first load -- change some lines to see them.
      [/@\s*[a-zA-Z_\$][\w\$]*/, { token: 'annotation', log: 'annotation token: $0' }],

      // numbers
      [/\d*\.\d+([eE][\-+]?\d+)?/, 'number.float'],
      [/0[xX][0-9a-fA-F]+/, 'number.hex'],
      [/\d+/, 'number'],

      // delimiter: after number because of .\d floats
      [/[;,.]/, 'delimiter'],

      // strings
      [/"([^"\\]|\\.)*$/, 'string.invalid' ],  // non-teminated string
      [/"/,  { token: 'string.quote', bracket: '@open', next: '@string' } ],

      // characters
      [/'[^\\']'/, 'string'],
      [/(')(@escapes)(')/, ['string','string.escape','string']],
      [/'/, 'string.invalid']
    ],

    comment: [
      [/[^=#]+/, 'comment' ],
      [/#=/,    'comment', '@push' ],    // nested comment
      ["=#",    'comment', '@pop'  ],
      [/[#=]/,   'comment' ]
    ],

    string: [
      [/[^\\"]+/,  'string'],
      [/@escapes/, 'string.escape'],
      [/\\./,      'string.escape.invalid'],
      [/"/,        { token: 'string.quote', bracket: '@close', next: '@pop' } ]
    ],

    whitespace: [
      [/[ \t\r\n]+/, 'white'],
      [/#=/,       'comment', '@comment' ],
      [/#.*$/,    'comment'],
    ],
  },
};
