#!/bin/sh

PROTOC=../../protoc-3.5.0-win32/bin/protoc.exe 

$PROTOC --cpp_out=. variable.proto
$PROTOC --js_out=import_style=commonjs,binary:../Console/generated/ variable.proto

