
Cant find old notes file atm, so making a new one to jot down a few ideas.


much of the information required to define a field is contextual, that is, it depends on the position of the field relative to other fields in the file.

## About

The uGON parser is simple implementation of a file parser for the GON (Glaiel Object Notation) file format. This format is similar to JSON, but it is much more terse. The original implementation of this format was created by Tyler Glaiel, and if you want to learn more about the format itself, you should refer to this link to the original repository.

Purpose:
My goal with this implementation of the format was to create a much faster, more efficient parser which will greatly cut down on load times and simplify the data access model. After several revisions, I believe this goal has largely been acheived. My uGON parser is roughly 100x faster than the original GON implementation, and significantly faster than rapidxml for a file containing roughly equivalent infromation. Currently, on my machine, the runtime of the parser is on average equivalent to around 3x the runtime of the strlen function.

How:
This parser gets its speed primarily from its simplicity. The entire uGON parser is available as a single C header file, and it is only ~330 lines total. Nearly the entire parsing process is done in a single 120-line loop, with no function calls or recursion. This loop reads each name/value pair which defines a GON field, with minimal control branching to handle object and array field types. 

It avoids many of the pitfalls of other parsers simply by doing things the old fashioned way, keeping in mind the basic of how computers really function. Many similar parsers for generalized, text-based data files (such as XML, JSON, etc.) construct a tree structure of nodes or objects which represent the data in the file (similar to a DOM). These individual nodes are often allocated one by one on the heap, and then linked to one another through pointers. This results in slow data access because of indirection and the need to constantly allocate small chunks of memory. By contrast, the uGON parser stores the data for each field sequentially in a flat array, were the structure can be derived from the field size and count attributes. This enables traversal of the parsed file in a tree-like manner, but without the performance penalties of a true, pointer-based tree structure.

The parser is "in-situ", meaning that tokens are parsed in-place. This allows us to skip all of the costly string manipulation operations required by other parsing methods. Instead of copying substrings to a separate locaiton in memory, we simply null-terminate each needed substring and store a pointer to the start location of the substring.

There are plans to add further functionality, such as a basic hash-based lookup which should accelerate second-stage parsing. This has not been added quite yet since it is not strictly essential and I've been more invested in other projects.

Why?:
My earlier claim that this parser is 100x faster than the original implementation may come off as an apparent exaggeration, but this is not the case. I have performed timed comparisons between my parser and other parser over millions of iterations in order to get the most accurate benchmarks that I could. *insert table of benchmarks here*

I should also note that this parser really is not heavily optimized. It may be possible that the format could be parsed much faster through the use of SIMD instructions or a multi-threaded solution. However, my use case for this file format does not make this level of optimization necessary. I think one would be hard-pressed to find a parallelized solution which is faster for the file sizes in the small-to-medium range, due to the necessary setup these methods require.



## in more depth

Overview of the parsing process of the original GON parser:
1. The file is loaded into a buffer
2. The file is tokenized
  - all tokens are copied into an array of strings
3. The GonObject tree is constructed from the tokens.
  - structural tokens determine control flow, start/end of objects/arrays.
  - string tokens may represent a name or a value
    - if it is a name, it is simply stored in the GonObject's name field.
    - if it is a 


In the case of the original GON parser, tokenization is done in a separate step from the construction of the tree, meaning that the extracted strings are copied more than once.
An attempt is then made to convert the token into each of the primitive data types (int, float, boolean), and the successful conversions for all of these formats are stored in the GonObject.

Often, the tree is then left in memory as the canonical storage for the data that was read from the file, and the tree is traversed each time data from the file is accessed. This is incredibly inefficient, and for no concrete benefit.

In the uGON parser, data representing the structure of the file is generated in a single pass, and the data for each individual "node" is stored sequentially in a flat array. This also does away with the need to store pointers to child nodes, since we can simply store the count of child nodes, and we know that these will be accesed in the subsequent array indices.

While it is true that the flat array structure prevents the easy insertion or removal of objects, that is outside of the intended use case for this parser.

It's a sad sign of the state of modern software development that major applications are reliant on data file formats such as XML and JSON, and the available tools are exceedingly over-engineered in the worst possible way.

Programming "best practices": superstitions and 
Why does the original GON parser do tokenization separately from the construction of the tree? IDK, proabbyl just because if you look up how to write a parser, that's what people say you should do. And in this case, perhaps  reasonably so. For parsing more complex grammars, tokenization is the perfect place to start. However, for such a simple file format, there is really no reason to "tokenize" separately. I mean really, the whole process of loading this file is just tokenization. Even once you've created the AST from the tokens, you haven't really gotten the data into a usable format. 

Some people might say "Well this is not maintainable, because it's too hard to read."
The difficult part is 120 lines. And with an adequate understanding of the file format, it is not exceptionally hard to read, even without any comments. I could easily comment every single line in the main loop, but I do not think it would serve to benefit the program.


## general notes:
- using flags to denote when a field name or value is in quotes is probably not great.
  - when creating a gon file from user input, the strings assigned to field names/values may contain spaces, in which case they will need to be in quotes. But we don't want the user to have to specify this or need to remember to check the input and set the in_quotes flag in every place where this is relevant.

- could it be better to use recursion instead of doing the manual stacks?
  - the ideal would be to push and pop from the stack manually, so that I could push/pop the parent index manually when entering/exiting objects/arrays
    - tried to do this through hacky methods - taking a pointer to an int at the top of the stack and then using that as i wished, but C runs some code to check for corruption around the stack when exitting the funciton, and they weren't so happy with that, even if i set the values back to zero when I'm done.
    - may just store the index of each field's parent in the GonField struct. Then I don't have to worry about this.

- child_count, is it really needed? why did I not just increment the count field of the parent directly?
  - seems not, just implemented it as such and everything appears to work, but will need to test further to verify.

- nulls? why do i do them all at the end again? makes me wish i had my old notes.
  - I assume there was some reason, such as not wanting to modify the input file string until the end..
    - but I don't see why this is necessary now, since I never move backwards.
    - ok, figured it out. the reason is because there is not always whitespace between the end of one token and the next, because of {} and [].
      - new potential solution is just to null all whitespace, although this may slow us down...
    - a better solution?
      - need to place nulls:
        - after each field name
          - may be terminated with '{' or '[' rather than whitespace
        - after each field value
          - may be terminated with '}' or ']' rather than whitespace

- may not *really* need count field for objects, but it doesn't hurt to keep.

- now that we are in C and not C++, the memory ownership around gonfile fields is *slightly* more complicated.
- if the user is using dyanmic buffer mode, then they need free the buffer before the gonfile goes out of scope, and before calling parse on a previously used gonFile


## Optimizing Further

Need to perform speed test comparisons against strlen on larger or more complicated GON files.

Parallelized parsing will probably only yeild a speed increase on certain types of data, or very large files


SIMD?
- could be used to generate bitmasks over the whole input to find start and end of tokens
- but then we still have to iterate over the SIMD output, not to mention allocating space for it...
The potential speed increase would be in iterating more quickly over the data by using 64 bit registers (equivalent to 8 chars)

what do we want to know on first SIMD pass?
- position of start/end of field names/values
  - dependent on positions of quotes
- position of special characters {}[]

I just took a look at the code for simdjson and it looks like way more crap than I want to involve myself with. I'm very skeptical that it even parses faster than my parser for an similar set of data.

Threads?
- if we knew the size of the file upfront, then the job could be split up into multiple threads where the starting index of each thread is set by creating equal subdivisions of the string. Then each thread will properly start parsing after first encountering an object or array close token, so that we know we are starting now on a field name and not a value.
- we would know to end after encountering our first null character, as this will either be the end of the file, or the first null placed by the next thread forward (in which case we ignore the last field name read).
- the fields parsed by each thread would have to be stored separately, and then recombined at the end to create one cohesive gon file.
- would have issues counting fields though, because of nesting objects...

State transition table?
not really applicable here, as the few branches that we do have are necessary.
if we knew where all objects/arrays began and end, it may be possible to parallelize the parsing of each one by 

It is unlikely that I will be able to further optimize the parser given the extreme simplicity of the format and the terseness of the data stored within.
If it were common to have very long string data stored in the file, then perhaps SIMD/threading would be worth the extra overhead, but as it is it would only slow down the parse time.

## QOL Features and Post-Processing

add function for converting GON file to hash map
- would prevent having to run strcmp when searching for fields by name
  - makes reading the gonFile marginally faster, but may not be worth the overhead
  - for my uses, I won't be retreiving the data from the GonFile more than once, as I will just store the data into a proper struct for internal use
- each field would need to store a hash of the the field name
  - this is a simple enough change, but adds slightly to the memory footprint
- is it faster to hash every field name or just do strcmp for the fields we actually access?
  - I will probably access every field in a given file when loading
  - which means doing repeat strcmps for every single field

add functionality to generate and edit gon files?
- easier to implement generating gon files, as fields can simply be added sequentially
- insertions will be difficult since we will either have to move data in the array or move to less of a flat structure.


Could save time on realloc if instead of doing realloc, we just alloc a new buffer for the next however-many fields and store a pointer to that location in the last gon field index available.

Should actually stop storing parent index in GonField and go back to old method of creating a local stack for the parent indices. May also just return to limiting max object depth to 255 or something.


## Change Log:

- removed parent index and child count arrays from parsing, as well as depth tracking. Now every GonField contains the index of its parent field, and object count is incremented each time a field is added.
- adding nulls at the end of name and value strings has now been reworked. The null are no longer collected and added all at once at the end of parsing, nor are they set immediately after reaching the end of a name or value string. Rather, the index of the null will be stored until it is safe to set the character to null, which is essentially just whenever we read the next valid token (which the null may have otherwise overwritten).
- went back to using malloc to allocate fields in stead of calloc
- added gon_create()
- added gon_free()







API Notes:

printing arrays of objects is currently messed up

The UGON parser is designed with the philosophy that the user will be able to understand how the parser generally works and how to interface with it. Although the main parsing loop is a bit complicated, everything surrounding it is very simple.

Additionally, it should be understood that the parser's output is not intended to be stored and used as a data structure which the user will repeatedly traverse. There are no insertion operations, and data access if intended to be handled directly by the user. Once the gon file has been parsed and the GonFile fields information has been generated, the user should traverse the fields and extract the data from the original file, converting from strings into whatever data format they wish. Essentially, this parser only generates the tokens for you and places them in an array which can be queried in a tree-like manner.

The intended use case is:
- read the file into buffer
- run parser, creating GonFile to provide structure for next step of processing
- read all data in the file into internal represntation (structs)
- dispose of the GonFile

How to use the ugon parser
1. create a GonFile using the gon_create() function
  - you could create it manually if you really want to, you just need to remember to memset the struct to 0's
1. set the file and file_length fields of the GonFile
   - in order to allow the user to implement their file loading however they want, I don't provide a function to do this in ugon.h
   - So the only requirement here is that before you call gon_parse(), you need to have loaded a text file as a char buffer into memory, and set that char* in the GonFile struct's file field.
   - at this point, treat the input file's char buffer as though it is owned by the GonFile. It will be modified by the GonFile, and the buffer will be freed when gon_free() is called.
2. Now the input buffer has been modified such that all substrings of field names and values have been null-terminated, and the GonFields buffer has been filled with the structural information required to query the fields of the file.
3. get data from the GonFile using gon_get_field();
   - if you want to query the root object, pass gon.fields as parent
   - returns null if no matching object found
4. once you have a GonField* that you want to extract data from, simply access the value field and convert the string to whatever data type you need. (atof, atoi, strcpy)
  

Example usage:

GonFile gon = gon_create();
read_text_file("test.gon", &gon.file, &gon.file_length);
gon_parse(&gon);

// do stuff with the gon file data


gon_free(&gon);

