<h1 style="text-align: center;">The uGON Parser</h1>

<h2>Overview</h2>
<p>
  The uGON parser is simple implementation of a file parser for the GON (Glaiel Object Notation) file format. 
  This format is similar to JSON, but it is much more terse. 
  The original implementation of this format was created by Tyler Glaiel, and if you want to learn more about the format itself, you should refer to <a href="https://github.com/TylerGlaiel/GON">this link</a> to the original repository.
</p>

<h2>Motivation</h2>
<p>
  I became familiar with the GON format because it is used heavily for storing game data in The End is Nigh, a game by Tyler Glaiel and Edmund McMillen.
  Through modding this game, I have used the format extensively and have had an affinity for its extreme simplicity compared to many other text-based data file formats (such as JSON or XML).
  During development of my <a href="https://stuart-mouse.github.io/randomizer.html">The End is Nigh Randomizer</a>, I developed a very straightforward C# port of the original parser, which helped familiarize me with how it functioned.
  After butting heads with the limitations of the TEiN game engine for long enough, I set out to create a new engine from scratch, and as part of that I wanted to write a GON parser from scratch to address some of the inadequacies I recognized in the original version.
</p>
<p>
  My goal with this implementation of the format was to create a much faster, more efficient parser which will greatly cut down on load times and simplify the data access model.
  After several revisions, I believe this goal has largely been acheived.
  My uGON parser is roughly 150x faster (yes, 150 times, not percent, faster) than the original GON implementation, and significantly faster than rapidxml for a file containing roughly equivalent infromation.
  Currently, on my machine, the runtime of the parser is on average equivalent to around 3x the runtime of the strlen function on the same input file.
</p>

<h2>High-Level Design</h2>
<p>
  This parser gets its speed primarily from its simplicity. 
  The entire uGON parser is available as a single C header file, and it is only ~330 lines total. 
  Nearly the entire parsing process is done in a single 120-line loop, with no function calls or recursion. 
  The main data structure of the parser is the GonField. This represents each field in the file, which may be of type field, object, or array.
  This struct is treated as a tagged union, reducing the memory footprint as much as possible without compromising speed.
  The main loop reads each name/value pair which defines a GonField, with minimal control branching to handle object and array field types. 
  In place of using recursion for parsing nested objects, a simple stack is used to push and pop parent object indices.
</p>
<p>
  uGON avoids many of the pitfalls of other parsers simply by doing things the old fashioned way, keeping in mind the basics of how computers really function. 
  Many parsers for similar formats construct a tree structure of nodes or objects which represent the data in the file (similar to a DOM). 
  These individual nodes are often allocated one by one on the heap, and then linked to one another through pointers. 
  This results in slow data access because of indirection and the need to constantly allocate small chunks of memory. 
  By contrast, the uGON parser stores the data for each field sequentially in a flat array, were the structure can be derived from the field size and count attributes. 
  This enables traversal of the parsed file in a tree-like manner, but without the performance penalties of a true, pointer-based tree structure.
</p>
<p>
  This has still been quite a cursory explaination of the program, but hopefully it has communicated some basic elements of the parser's structure. More detailed documentation is yet to come.
</p>

<h2>To-Do</h2>
<p>
  This parser is not quite completely ready to see use in your next big project, but it should be seeing some useful updates in the weeks and months to come. 
  I plan to finalize a proper API for interfacing with the parser soon, as well as adding better methods for accessing data, including a simple hash table to enable fast data retreivals.
  I may look into options for further optimization using mutli-threading or SIMD instructions, though I anticipate that such solutions may have very diminishing returns.
</p>