## ONE HEADER TO RULE THEM ALL!!!
### Binary
Get it here [[Windows x64 static binary]](https://github.com/rpvelloso/oneheader/raw/master/oneheader-mingw64.zip)

### Problem:
Let's say you are writing a library, and you got all your
headers spread all over your project's folder tree. If
you want to distribute it as a single binary (DLL) with a 
single header, how do you combine all those headers into a 
single one?

### How does it works?
1. Your project's folder is scanned for C/C++ headers 
   and a list of headers found is created;
1. For every header in the list it analyzes its #include 
   directives and assemble a dependency graph in the 
   following way:
   
   1. If the included header *is not* located inside 
      the project's folder then it is ignored (e.g., 
      if it is a system header);
      
   1. If the included header *is* located inside the 
       project's folder then an edge is create in the 
       dependency graph, linking the included header 
       to the current header being analyzed;
       
1. The dependency graph is topologically sorted to determine
   the correct order to concatenate the headers into a single
   file. If a cycle is found in the graph, the process is 
   interrupted (i.e., if it is not a DAG);

### Limitations:
  1. It does not handles headers with the same name in different paths;
  2. It only gives you a correct order to combine all the headers, you 
     still need to concatenate them (maybe you want remove or modify some of them prior to merging).

### Compiling:
  g++ -Wall -ggdb -std=c++1y -lstdc++fs oneheader.cpp -o oneheader[.exe]

### Usage:
  ./oneheader[.exe] project_folder/ > file_sequence.txt
