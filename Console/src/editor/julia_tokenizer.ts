/**
 * Copyright (c) 2017-2018 Structured Data, LLC
 * 
 * This file is part of BERT.
 *
 * BERT is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * BERT is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with BERT.  If not, see <http://www.gnu.org/licenses/>.
 */

/// <reference path="../../node_modules/monaco-editor/monaco.d.ts" />

/**
 * this is a language tokenizer for julia for the monaco editor.
 * still learning how monarch works (and learning julia syntax).
 * work in progress.
 */

import IRichLanguageConfiguration = monaco.languages.LanguageConfiguration;
import ILanguage = monaco.languages.IMonarchLanguage;

export const conf: IRichLanguageConfiguration = {
	wordPattern: /(-?\d*\.\d\w*)|([^\`\~\!\@\#%\^\&\*\(\)\=\$\-\+\[\{\]\}\\\|\;\:\'\"\,\.\<\>\/\?\s]+)/g,
	comments: {
		blockComment: ['#=', '=#'],
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

  control_flow: [
    'for', 'if', 'elseif', 'else', 'end', 'while', 'break', 'do',
    'try', 'catch', 'finally', 'throw'
  ],

  keywords: [
    'function', 'struct', 'global', 
    'switch', 'case', 'otherwise', 'try', 'catch', 'end', 'const', 'immutable',
    'import', 'importall', 'export', 'type', 'typealias', 'return', 'true', 
    'false', 'macro', 'quote', 'in', 'abstract', 'module', 'using', 'continue', 
    'ifelse', 'eval', 'let', 'nothing'
  ],

  typeKeywords: [
    'Array', 'String', 'Bool', 'Number', 'Int', 'Integer', 'Real', 'Complex', 
    'FloatingPoint', 'Float64', 'Float32', 'Int8', 'Int16', 'Int32', 'Int64', 
    'Rational', 'AbstractArray', 'Unsigned', 'Signed', 'UInt', 'UInt8', 'UInt16', 
    'UInt32', 'UInt64', 'Vector', 'AbstractVector', 'Matrix', 'AbstractMatrix', 
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

  namespaceFollows: [
    'module', 'using', 'import', 'importall'
  ],

  labelFollows: [
    'function',
  ],

  // The main tokenizer for our languages
  tokenizer: {
    root: [

      // [/[A-Z][\w]*(?=[\.\w]*(\s|\/\*!\*\/)+\w)/, 'type.identifier' ],

      // type.idenfier for function definitions. also uses the
      // @labelFollows block below.

      // 1. name = function(...)
      [/\w\w*(?=\s*=\s*function)/, { token: 'type.identifier'}],  

      // 2. name() = 
      [/\w\w*(?=\s*\(.*?\)\s*=)/, { token: 'type.identifier'}],  

      // instead of that, highlight function calls. this appears to be 
      // what the vs julia extension does. not in love with it.

      // [ /[a-z_A-Z]\w*(?:\s*\()/, { token: 'type.identifier' }],

      // identifiers and keywords
      [/[a-z_A-Z$][\w$]*/, { cases: { 
        '@namespaceFollows': { token: 'keyword', next: '@namespace' },
        '@labelFollows': { token: 'keyword', next: '@label' },
        '@control_flow': 'keyword.control', // this token is not defined by default; add to theme
        '@keywords': 'keyword',
        '@typeKeywords': 'type',
        '@default': 'identifier'
        } }],
     
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
      // [/@\s*[a-zA-Z_\$][\w\$]*/, { token: 'annotation', log: 'annotation token: $0' }],

      // numbers
      [/\d*\.\d+([eE][\-+]?\d+)?/, 'number.float'],
      [/0[xX][0-9a-fA-F]+/, 'number.hex'],
      [/\d+/, 'number'],

      // delimiter: after number because of .\d floats
      [/[;,.]/, 'delimiter'],

      // strings
      [/"([^"\\]|\\.)*$/, 'string.invalid' ],  // non-teminated string
      [/"""/, { token: 'string.quote', bracket: '@open', next: '@mstring' }],
      [/"/,  { token: 'string.quote', bracket: '@open', next: '@string' } ],

      // characters
      [/'[^\\']'/, 'string'],
      [/(')(@escapes)(')/, ['string','string.escape','string']],
      [/'/, 'string.invalid']
    ],

    comment: [
      [/[^=#]+/, 'comment' ],
      [/#=/,    'comment', '@push' ],    // nested comment [is that legal?]
      ["=#",    'comment', '@pop'  ],
      [/[#=]/,   'comment' ]
    ],
    
    label: [
      [ /[ \t]+$/, {token: 'white', next: '@pop'}],
      [ /[ \t]+/, {token: 'white'}],      
      [/\w\w*/, {token: 'type.identifier', next: '@pop'}],
      ['', {token: '', next: '@pop'}],
    ],

    // this is a little difficult because it can end 
    // either on a newline or at the end of another token;
    // we don't handle the (error?) case where the line ends
    // on an operator. requires testing for eol in whitespace, token

    namespace: [
      [ /[ \t]+$/, {token: 'white', next: '@pop'}],
      [ /[ \t]+/, 'white' ],      
      [ /[\:\.=,]/, '' ],
      [/\w\w*$/, {token: 'type.identifier', next: '@pop'}],
      [/\w\w*/, {token: 'type.identifier'}],
      ['','','@pop'],
    ],

    // this is my attempt to do string interpolation, highlighting
    // the outer $() set, and allowing inner parens for function calls.
    // not super familiar with the syntax, but seems to function OK:

    // there's probably a better way to do this with cases, if we can
    // somehow query the stack or use state vars

    // for nested paren sections
    interpolated_nested: [
      [/\)/, {token: 'interpolated', bracket: '@close', next: '@pop' }],
      [/\(/, {token: 'interpolated', bracket: '@open', next: 'interpolated_nested' }],
      { include: '@root' }
    ],

    // inside an interpolation block, until closing paren ")"
    interpolated: [
      [/\(/, {token: 'interpolated', bracket: '@open', next: 'interpolated_nested' }],
      [/\)/, {token: 'keyword', bracket: '@close', next: '@pop' }],
      { include: '@root' }
    ],

    // start of an interpolation block "$("
    interpolated_string: [
      [/\$\(/, {token: 'keyword', bracket: '@open', next: '@interpolated' }]
    ],

    // multiline string; only ends on three quotes (""")
    mstring: [
      { include: '@interpolated_string' },
      [/"""/,        { token: 'string.quote', bracket: '@close', next: '@pop' } ],
      [/./, 'string']
    ],
    
    // normal (single-line) string
    string: [
      { include: '@interpolated_string' }, // ?
//      [/[^\\"]+/,  'string'], // ?
      [/@escapes/, 'string.escape'],
      [/\\./,      'string.escape.invalid'],
      [/"/,        { token: 'string.quote', bracket: '@close', next: '@pop' } ],
      [/./, 'string']

    ],

    whitespace: [
      [/[ \t\r\n]+/, 'white'],
      [/#=/,       'comment', '@comment' ],
      [/#.*$/,    'comment'],
    ],
  },
};
