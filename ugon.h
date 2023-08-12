/*
	Uzerro's GON Parser
	Version Beta 1.4

	For detailed information and explanations on how the parser works, see the included documentation.
*/ 

/*
	Field Buffer Settings

	If GON_USING_DYNAMIC_BUFFER is defined, the parser will use dynamically allocated memory for the fields buffer.
	If you intend to use this buffer in a very memory-constrained setting, you can optionally undefine this macro and use a statically allocated buffer instead.
	When using static buffer mode, the maximum field count is defined with the macro GON_FIELD_BUFFER_SIZE.
	When using dynamic buffer mode, the default starting size for the buffer is defined with the macro GON_FIELD_BUFFER_DEFAULT_SIZE.
	You also have the option to define GON_REALLOC_ON_COMPLETE when in dynamic buffer mode. This will realloc the gon fields buffer once parsing has completed, reducing it to the smallest size necessary.
*/

#define GON_USING_DYNAMIC_BUFFER
#ifdef GON_USING_DYNAMIC_BUFFER
#define GON_FIELD_BUFFER_DEFAULT_SIZE 32
//#define GON_REALLOC_ON_COMPLETE
#else
#define GON_FIELD_BUFFER_SIZE 32
#endif

// Lookup table for whitespace characters
const unsigned char gon_lookup_whitespace[256] = {
	0,0,0,0,0,0,0,0,0,1,1,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

// Lookup table for text characters
// (basically whitespace, brackets/braces, and null)
const unsigned char gon_lookup_non_text[256] = {
	1,0,0,0,0,0,0,0,0,1,1,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,1,0,0,
};

// Field Value Types
typedef enum GonType {
	GON_TYPE_FIELD  = 1,
	GON_TYPE_OBJECT = 2,
	GON_TYPE_ARRAY  = 3
} GonType;

#define gon_type_check(gon, gontype) (gon != NULL && gon->type == gontype)

// Defines a single field in the gon file
typedef struct GonField {
	char *name;
	int   parent;
	int   type;
	union {
		char *value;
		struct { int size, count; };
	};
} GonField;

// Container for the information loaded from a gon file
typedef struct GonFile {
	char   *file;
	size_t  file_length;
#ifdef GON_USING_DYNAMIC_BUFFER
	GonField *fields;
	size_t    field_capacity;
#else
	GonField fields[GON_FIELD_BUFFER_SIZE];
#endif
} GonFile;

// Loads a text file into the GonFile struct
int gon_parse(GonFile* gon) {
	if (!gon->file) return 1;
	char* index = gon->file;
	char* last  = &gon->file[gon->file_length];

	#ifdef GON_USING_DYNAMIC_BUFFER
	if (!gon->fields) {
		gon->field_capacity = GON_FIELD_BUFFER_DEFAULT_SIZE;
		gon->fields = (GonField*)malloc(gon->field_capacity * sizeof(GonField));
	}
	#endif

	// create root field
	gon->fields[0].type   = GON_TYPE_OBJECT;
	gon->fields[0].parent = 0;
	gon->fields[0].count  = 0;
	gon->fields[0].name   = (char*)"root";

	int field_index = 1;
	int parent_index = 0;
	bool in_array = 0;

	// need to store an index so that we can defer null-ing it until we know what comes after
	// init'd to last so that we only overwrite existing null if none has been set yet.
	char* null_pos = last;

	// parse the file one field at a time
	while (true) {
		// skip whitespace and comments
		while (true) {
			while (gon_lookup_whitespace[*index]) index++;
			if (*index == '#') {
				while (*index != '\n') index++;
				continue;
			}
			break;
		}

		// break at EOF
		if (index >= last) {
			if (parent_index != 0) {
				puts("GON parse error: unexpected EOF.");
				return 1;
			}
			break;
		}

		// check if object ends
		if (*index == ']') {
			if (!in_array) goto L_Error;
			goto L_StepOutOfObject;
		}
		if (*index == '}'){
			if (in_array) goto L_Error;
			goto L_StepOutOfObject;
		}
		goto L_ReadName;

	L_StepOutOfObject:;
		if (parent_index == 0) {
			printf("GON parse error: encountered unexpected token %c at field %i.\n", *index, field_index);
			return 1;
		}
		gon->fields[parent_index].size = field_index - parent_index - 1;	// set parent size based on current index
		parent_index = gon->fields[parent_index].parent;					// set parent back to parent's parent
		in_array = (gon->fields[parent_index].type == GON_TYPE_ARRAY);		// in_array = true if new parent is array type
		*null_pos = 0;														// places null after previous field value
		index++;															// step over } or ]
		continue;

	L_ReadName:;
		*null_pos = 0;											// places null after previous field name/value
		gon->fields[parent_index].count++;						// increment parent object's field count
		memset(&gon->fields[field_index], 0, sizeof(GonField));	// init new field memory to zero
		gon->fields[field_index].parent = parent_index;			// set new field's parent index

		// get field / object name
		if (!in_array) {
			gon->fields[field_index].name = index;

			bool in_quotes = *index == '"';
			if (in_quotes) {
				(gon->fields[field_index].name)++;
				index++;
				while (*index != '"')
					index += 1 + (*index == '\\');
			}
			else while (!gon_lookup_non_text[*index]) index++;
			null_pos = index;
			// check that objects have a name
			if ((index - gon->fields[field_index].name) <= in_quotes) {
				printf("GON parse error: encountered unexpected token %c at field %i.\n", *index, field_index);
				return 1;
			}
			index += in_quotes;
		}

		// skip whitespace and comments
		while (true) {
			while (gon_lookup_whitespace[*index]) index++;
			if (*index == '#') {
				while (*index != '\n') index++;
				continue;
			}
			break;
		}

		// check if we need to realloc more space for the fields
		#ifdef GON_USING_DYNAMIC_BUFFER
		if (field_index >= gon->field_capacity - 1) {
			gon->field_capacity *= 2;
			GonField* fields_new = (GonField*)realloc(gon->fields, gon->field_capacity * sizeof(GonField));
			if (!fields_new) {
				puts("GON parse error: Unable to realloc gon fields buffer.");
				return 1;
			}
			gon->fields = fields_new;
		}
		#else
		if (field_index >= GON_FIELD_BUFFER_SIZE) {
			printf("GON parse error: encountered unexpected token %c at field %i.\n", *index, field_index);
			return 1;
		}
		#endif

		// step into object or array
		if (*index == '{')					  goto L_StepIntoObject;
		if (*index == '[') { in_array = true; goto L_StepIntoObject; }
		goto L_ReadValue;

	L_StepIntoObject:;
		gon->fields[field_index].type = GON_TYPE_OBJECT + in_array;	// set field type
		*null_pos = 0;					// can safely place null after name after reading in '{'
		index++;						// step over { or [
		parent_index = field_index;		// set new parent index
		field_index++;					// increment field index
		continue;

	L_ReadValue:;
		// read field value
		if (!gon_lookup_non_text[*index]) {
			*null_pos = 0;															// we can safely place null after field name now
			gon->fields[field_index].type = GON_TYPE_FIELD;							// set the gon type to field
			gon->fields[field_index].value = index;									// set the pointer to the start of the field's value string to the current index

			bool in_quotes = *index == '"';											// if the first character of a value string is " then the value string is enclosed in quotes
			if (in_quotes) {														// if the field's value is enclosed in quotes, we need to do some extra work so that we can allow characters which are typically not allowed in naked gon string values
				(gon->fields[field_index].value)++;									// move pointer for start of field value forward by one character so that we don't include the initial quotation mark
				index++;															// step over the initial quotation mark
				while (*index != '"')												// scan through string until we hit another (non-escaped) quotation mark
					index += 1 + (*index == '\\');									// increment index by one, or increment by 2 if we hit a backslash (this character escapes the following character)
			}
			else while (!gon_lookup_non_text[*index]) index++;						// for strings not in quotes, just scan forward until the next next non-text character
			null_pos = index;														// defer placing null after field value until either new field name or '}' or ']' is read
			index += in_quotes;														// step over the end quotation mark if applicable
			field_index++;															// increment the field index
			continue;																// go back to top of loop to parse the next field
		}

		// if token was not the start of an object, array, or valid field value then error
	L_Error:;
		printf("GON parse error: encountered unexpected token %c at field %i.\n", *index, field_index);
		return 1;
	}

	*null_pos = 0;							// place final null 
	gon->fields[0].size = field_index;		// set root object size

	#ifdef GON_REALLOC_ON_COMPLETE
	// realloc fields down to used size
	GonField* fields_new = realloc(gon->fields, (gon->field_capacity = gon->fields[0].size + 1) * sizeof(GonField));
	if (!fields_new) {
		puts("GON parse error: Unable to realloc gon fields buffer.");
		return 1;
	}
	gon->fields = fields_new;
	#endif

	return 0;
}

// Writes the contents of the GonFile to the buffer at dest
// Buffer must be pre-allocated by caller
void gon_serialize(GonFile* gon, char* dst, int tab_width) {
	int parent_index = 0;
	int field_index  = 1;
	int indent		 = 0;
	bool in_array	 = 0;

	while (1) {
		if (field_index >= gon->fields[0].size && parent_index == 0) break;

		// Check if object ends
		if (field_index - parent_index > gon->fields[parent_index].size) {
			indent -= tab_width;
			if (!in_array) memset(dst, ' ', indent), dst += indent; // add indentation
			*dst = in_array ? ']' : '}', dst++;

			parent_index = gon->fields[parent_index].parent;
			in_array = (gon->fields[parent_index].type == GON_TYPE_ARRAY);
			*dst = in_array ? ' ' : '\n'; dst++;
			continue;
		}

		// write field name
		if (!in_array) {
			if (!in_array) memset(dst, ' ', indent), dst += indent;	// add indentation

			// check whether to enclose name and/or value in quotes
			bool in_quotes = false;
			char* c = gon->fields[field_index].name;
			int len = 0;
			while (*c) {
				if (gon_lookup_non_text[*c]) in_quotes = true; 
				len++, c++;
			}

			if (in_quotes) *dst = '"', dst++;
			memcpy(dst, gon->fields[field_index].name, len);
			dst += len;
			if (in_quotes) *dst = '"', dst++;
			*dst = ' ', dst++;
		}

		// Write field value
		if (gon->fields[field_index].type == GON_TYPE_FIELD) {
			bool in_quotes = false;
			char* c = gon->fields[field_index].value;
			int len = 0;
			while (*c) {
				if (gon_lookup_non_text[*c]) in_quotes = true;
				len++, c++;
			}
			if (in_quotes) *dst = '"', dst++;
			memcpy(dst, gon->fields[field_index].value, len);
			dst += len;
			if (in_quotes) *dst = '"', dst++;
			*dst = in_array ? ' ' : '\n'; dst++;
			field_index++;
			continue;
		}

		// Step into object
		if (gon->fields[field_index].type == GON_TYPE_ARRAY) {
			in_array = 1;
			*dst = '[', dst++;
			*dst = ' ', dst++;
		}
		else {
			*dst = '{',  dst++;
			*dst = '\n', dst++;
		}
		indent += tab_width;
		parent_index = field_index;
		field_index++;
	}

	*dst = '\0'; // write null to end of file
}


void gon_serialize_file(GonFile* gon, FILE* fp, int tab_width) {
	int parent_index = 0;
	int field_index  = 1;
	int indent		 = 0;
	bool in_array	 = 0;

	while (1) {
		if (field_index >= gon->fields[0].size && parent_index == 0) break;

		// check if object ends
		if (field_index - parent_index > gon->fields[parent_index].size) {
			indent -= tab_width;										// decrease indent
			if (!in_array) for (int i = 0; i < indent; i++) fputc(' ', fp); // write indentation if not in array
			fputc(in_array ? ']' : '}', fp);								// write object / array end token
			parent_index = gon->fields[parent_index].parent;				// set parent index to grandparent
			in_array = (gon->fields[parent_index].type == GON_TYPE_ARRAY);	// determine if parent is an array
			fputc(in_array ? ' ' : '\n', fp);								// write newline on object/array end (or space if in array)
			continue;
		}

		// write field name
		if (!in_array) {
			if (!in_array) for (int i = 0; i < indent; i++) fputc(' ', fp); // write indentation if not in array
			bool in_quotes = false;
			{
				char* c = gon->fields[field_index].name;
				int len = 0;
				while (*c) {
					if (gon_lookup_non_text[*c]) in_quotes = true;
					len++, c++;
				}
			}
			if (in_quotes) fputc('"', fp);									// start quotes
			char* c = gon->fields[field_index].name;						// write name escaping characters as necessary
			while (*c) {
				if (*c == '\"' || *c == '\\') fputc('\\', fp);				// escape \ and "
				fputc(*c, fp);												// write next character
				c++;
			}
			if (in_quotes) fputc('"', fp);									// end quotes
			fputc(' ', fp);													// add a space after name
		}

		// Write field value
		if (gon->fields[field_index].type == GON_TYPE_FIELD) {
			bool in_quotes = false;
			{
				char* c = gon->fields[field_index].value;
				int len = 0;
				while (*c) {
					if (gon_lookup_non_text[*c]) in_quotes = true;
					len++, c++;
				}
			}
			if (in_quotes) fputc('"', fp);									// start quotes
			char* c = gon->fields[field_index].value;						// copy value escaping characters as necessary
			while (*c) {
				if (*c == '\"' || *c == '\\') fputc('\\', fp);				// escape \ and "
				fputc(*c, fp);												// write next character
				c++;
			}	
			if (in_quotes) fputc('"', fp);									// end quotes
			fputc(in_array ? ' ' : '\n', fp);								// write newline on field end (or space if in array)
			field_index++;
			continue;
		}

		// Step into object
		if (gon->fields[field_index].type == GON_TYPE_ARRAY) {
			in_array = 1;
			fputc('[', fp);
			fputc(' ', fp);
		}
		else {
			fputc('{', fp);
			fputc('\n', fp);
		}
		indent += tab_width;
		parent_index = field_index;
		field_index++;
	}

	fputc('\n', fp); // write null to end of file (probably not necessary, but its not hurting anything either)
}

// Gets the first child field with the given name if it exists, otherwise returns NULL
// Assumes that parent is not NULL
GonField* gon_get_field(GonField* parent, const char* name) {
	if (!parent) {
		printf("Tried to get field \"%s\" from null gon.", name);
		return NULL;
	} else if (parent->type != GON_TYPE_OBJECT) {
		char* parent_name = (char*)(parent->name ? parent->name : "NULL");
		printf("Tried to get field \"%s\" from non-object gon \"%s\".", name, parent_name);
		return NULL;
	}

	int count = parent->count;
	GonField* field = parent + 1;
	for (int i = 0; i < count; i++) {
		if (field->name && strcmp(field->name, name) == 0)
			return field;
		if (field->type != GON_TYPE_FIELD) // step over sub-fields of object and array types
			field += field->size;
		field++;
	}
	return NULL;
}

// Tries to get a string value from the named field
// If no field is found with matching name or field is wrong type, returns default value
char* gon_get_value(GonField* parent, const char* name, char* default_value) {
	GonField* field = gon_get_field(parent, name);
	if (gon_type_check(field, GON_TYPE_FIELD))
		return field->value;
	return default_value;
}

// Tries to get an integer value from the named field
// If no field is found with matching name or field is wrong type, returns default value
int gon_get_int(GonField* parent, const char* name, int default_value) {
	GonField* field = gon_get_field(parent, name);
	if (gon_type_check(field, GON_TYPE_FIELD))
		return atoi(field->value);
	return default_value;
}

// Tries to get a float value from the named field
// If no field is found with matching name or field is wrong type, returns default value
double gon_get_float(GonField* parent, const char* name, double default_value) {
	GonField* field = gon_get_field(parent, name);
	if (gon_type_check(field, GON_TYPE_FIELD))
		return atof(field->value);
	return default_value;
}

// Tries to get an boolean value from the named field (anything other than "true" returns false)
// If no field is found with matching name or field is wrong type, returns default value
bool gon_get_bool(GonField* parent, const char* name, bool default_value) {
	GonField* field = gon_get_field(parent, name);
	if (gon_type_check(field, GON_TYPE_FIELD))
		return (strcmp(field->value, "true") == 0);
	return default_value;
}

#define gon_iterate_array(gon_array, gon_it) \
GonField* gon_it##_last = gon_array + gon_array->size; \
for (GonField* gon_it = gon_array + 1; gon_it <= gon_it##_last; gon_it += (gon_it->type == GON_TYPE_FIELD ? 1 : gon_it->size + 1))

inline GonFile gon_create(void) {
	GonFile gon = { 0 };
	return gon;
}

void gon_free(GonFile* gon) {
	free(gon->file);
	#ifdef GON_USING_DYNAMIC_BUFFER
	free(gon->fields);
	#endif
}

/*

typedef struct GonFileBuilder {
GonFile file;
size_t index;
size_t parent;
} GonFileBuilder;

GonFileBuilder gon_builder_create(size_t expected_field_count) {
GonFileBuilder builder;
memset(&builder, 0, sizeof(builder));

expected_field_count = fmaxf(expected_field_count, 32);

builder.file.fields = malloc(expected_field_count * sizeof(GonField));
builder.file.field_capacity = expected_field_count;

builder.file.fields[0] = (GonField){
.type	= GON_TYPE_OBJECT,
.name	= "root",
.parent = 0,
};
builder.index = 1;

return builder;
}

// single function handles appending all types of fields
// if appending an object or array, just pass NULL for value, it is simply ignored
// passing a name when in an array will cause the name to just be ignored, so pass NULL for name too if you know you're in an array
int gon_builder_append(GonFileBuilder* builder, int gon_type, const char* name, const char* value) {
if (builder->index == builder->file.field_capacity) {
builder->file.field_capacity *= 2;
GonField* fields_new = realloc(builder->file.fields, builder->file.field_capacity * sizeof(GonField));
if (fields_new == NULL) {
puts("GON builder error: Unable to realloc gon fields buffer.");
return 1;
}
builder->file.fields = fields_new;
}

bool in_array = builder->file.fields[builder->parent].type == GON_TYPE_ARRAY;
builder->file.fields[builder->index] = (GonField) {
.type   = gon_type,
.name   = in_array ? NULL : name,
.parent = builder->parent,
};
if (gon_type == GON_TYPE_FIELD)
builder->file.fields[builder->index].value = value;

// check whether to enclose name and/or value in quotes
{
char* c;
c = name;
if (c)
while (*c) 
if (gon_lookup_non_text[*c++]) { 
builder->file.fields[builder->index].flags |= GON_FIELD_FLAG_NAME_IN_QUOTES; 
break; 
} 
c = value;
if (c)
while (*c)
if (gon_lookup_non_text[*c++]) { 
builder->file.fields[builder->index].flags |= GON_FIELD_FLAG_VALUE_IN_QUOTES;
break; 
} 
}

builder->file.fields[builder->parent].count++;
if (gon_type != GON_TYPE_FIELD) builder->parent = builder->index;
builder->index++;
return 0;
}

int gon_builder_step_out(GonFileBuilder* builder) {
if (builder->parent == 0 && builder->file.fields[0].size != 0) {
printf("GON builder error: cannot step out of root gon object.");
return 1;
}
builder->file.fields[builder->parent].size = builder->index - builder->parent - 1;
builder->parent = builder->file.fields[builder->parent].parent;
return 0;
}

*/


/*
For the time being, the GonFilePrinter is going to have a maximum object depth of 1024.
I think this is quite reasonable, as there's no reason I can think of to nest things so deeply.
Frankly, if this is a requirement of your program, you should just set the stack size to be deeper 
but keep it as a constant value so that everything stays on the stack instead of going to heap.
*/
#define GON_PRINTER_MAX_DEPTH 1024  

typedef struct GonFilePrinter {
	FILE* fp;
	unsigned char parent_type[GON_PRINTER_MAX_DEPTH];
	int depth;
	int tab_width;
	int indent;
} GonFilePrinter;

int gon_printer_append(GonFilePrinter* printer, int gon_type, const char* name, const char* value) {
	bool in_array = (printer->parent_type[printer->depth] == GON_TYPE_ARRAY);

	// write field name
	if (!in_array) {
		if (name == NULL) {
			printf("GON Printer error: All fields which are not in an array must have a name.");
			return 1;
		}
		if (!in_array) for (int i = 0; i < printer->indent; i++) fputc(' ', printer->fp); // write indentation if not in array

		char* c; // used in iterating over name

					// check whether to enclose name and/or value in quotes
		bool in_quotes = false;
		c = (char*)name;
		while (*c) if (gon_lookup_non_text[*c++]) { in_quotes = true; break; } 

		if (in_quotes) fputc('"', printer->fp);							// start quotes
		c = (char*)name;
		while (*c) {													// write name escaping characters as necessary
			if (*c == '\"' || *c == '\\') fputc('\\', printer->fp);		// escape \ and "
			fputc(*c, printer->fp);										// write next character
			c++;
		}
		if (in_quotes) fputc('"', printer->fp);							// end quotes
		fputc(' ', printer->fp);										// add a space after name
	}

	// Write field value
	if (gon_type == GON_TYPE_FIELD) {
		if (value == NULL) {
			printf("GON Printer error: All field types must have a value.");
			return 1;
		}

		char* c; // used in iterating over value string

		// check whether to enclose name and/or value in quotes
		bool in_quotes = false;
		c = (char*)value;
		while (*c) if (gon_lookup_non_text[*c++]) { in_quotes = true; break; } 

		if (in_quotes) fputc('"', printer->fp);								// start quotes
		c = (char*)value;															// copy value escaping characters as necessary
		while (*c) {
			if (*c == '\"' || *c == '\\') fputc('\\', printer->fp);			// escape \ and "
			fputc(*c, printer->fp);											// write next character
			c++;
		}	
		if (in_quotes) fputc('"', printer->fp);								// end quotes

		fputc(in_array ? ' ' : '\n', printer->fp);							// write newline on field end (or space if in array)
		return 0;
	}

	// Step into object
	if (gon_type == GON_TYPE_ARRAY) {
		in_array = 1;
		fputc('[', printer->fp);
		fputc(' ', printer->fp);
	}
	else {
		fputc('{', printer->fp);
		fputc('\n', printer->fp);
	}
	printer->indent += printer->tab_width;

	if (++printer->depth >= 1024) {
		puts("GON printer error: Exceeded maximum object depth.");
		return 1;
	}
	printer->parent_type[printer->depth] = gon_type;

	return 0;
}

int gon_printer_step_out(GonFilePrinter* printer) {
	if (printer->depth == 0) {
		printf("GON printer error: cannot step out of root gon object.");
		return 1;
	}
	fputc(printer->parent_type[printer->depth] == GON_TYPE_ARRAY ? ']' : '}', printer->fp);	// write object / array end token
	printer->depth--;
	fputc(printer->parent_type[printer->depth] == GON_TYPE_ARRAY ? ' ' : '\n', printer->fp);	// write newline on object/array end (or space if in array)
	printer->indent -= printer->tab_width;
	return 0;
}