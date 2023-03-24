// Uzerro's GON Parser
// Version Beta 1.3

// Field Buffer Settings
#define GON_USING_DYNAMIC_BUFFER
#ifdef GON_USING_DYNAMIC_BUFFER
// Default buffer size
#define GON_FIELD_BUFFER_DEFAULT_SIZE 32
// If this is defined, the parser will realloc the fields array to the minimum size needed to contain the data
//#define GON_REALLOC_ON_COMPLETE
#else
// Field Buffer Size
#define GON_FIELD_BUFFER_SIZE 32
#endif

#define GON_MAX_DEPTH 255
#define GON_TAB_WIDTH 2

// Lookup table for whitespace characters
const int gon_lookup_whitespace[256] = {
	0,0,0,0,0,0,0,0,0,1,1,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

// Lookup table for text characters
// (basically whitespace, brackets/braces, and null)
const int gon_lookup_non_text[256] = {
	1,0,0,0,0,0,0,0,0,1,1,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,1,0,0,
};

// Field Value Types
#define GON_VALUE_TYPE_FIELD  1
#define GON_VALUE_TYPE_OBJECT 2
#define GON_VALUE_TYPE_ARRAY  3

// Field Flags
#define GON_FIELD_FLAG_NAME_IN_QUOTES  (1 << 0)
#define GON_FIELD_FLAG_VALUE_IN_QUOTES (1 << 1)

// Defines a single field in the gon file
typedef struct GonField {
	char* name;
	unsigned short type;
	unsigned short flags;
	union {				// saves ~8 bytes probably
		char* value;	// value only needed by fields
		struct {		// size and count only needed by objects/arrays
			unsigned short size;
			unsigned short count;
		};
	};
} GonField;

// Container for the information loaded from a gon file
typedef struct GonFile {
	#ifdef GON_USING_DYNAMIC_BUFFER
	GonField* fields;
	size_t field_capacity;
	#else
	GonField fields[GON_FIELD_BUFFER_SIZE];
	#endif
	char* file;
	size_t file_length;
} GonFile;

// Loads a text file into the GonFile struct
int gon_parse(GonFile* gon) {
	if (!gon->file) return 1;
	char* index = gon->file;
	char* last = &gon->file[gon->file_length];

	#ifdef GON_USING_DYNAMIC_BUFFER
	if (!gon->fields) {
		gon->field_capacity = GON_FIELD_BUFFER_DEFAULT_SIZE;
		gon->fields = (GonField*)malloc(gon->field_capacity * sizeof(GonField));
	}
	#endif

	// create root field
	gon->fields[0].type = GON_VALUE_TYPE_OBJECT;

	unsigned int field_index = 1;
	unsigned int depth = 0;
	unsigned int parent_index[GON_MAX_DEPTH];
	bool in_array = 0;

	parent_index[0] = 0;
	// need to store an index so that we can defer null-ing it until we know what comes after
	// init'd to last so that we only overwrite existing null if none has been set yet.
	char* null_pos = last;

	// Parse the file one field at a time
	while (true) {
		// Skip whitespace and comments
		while (gon_lookup_whitespace[*index]) index++;
		if (*index == '#') {
			while (*index != '\n') index++;
			while (gon_lookup_whitespace[*index]) index++;
		}

		// Break at EOF
		if (index >= last) {
			if (parent_index[depth]) {
				puts("GON parse error: unexpected EOF.");
				return 1;
			}
			break;
		}

		// Check if object ends
		if (*index == '}' || *index == ']') {
			if (depth == 0) {
				printf("GON parse error: encountered unexpected token %c at field %i.\n", *index, field_index);
				return 1;
			}
			gon->fields[parent_index[depth]].size = field_index - parent_index[depth] - 1;
			depth--;
			in_array = (gon->fields[parent_index[depth]].type == GON_VALUE_TYPE_ARRAY); // in_array = true if new parent is array type
			*null_pos = 0; // places null after previous field value
			index++; // step over } or ]
			continue;
		}

		*null_pos = 0; // places null after previous field value
		gon->fields[parent_index[depth]].count++;
		gon->fields[parent_index[depth]].flags = 0;
		// Get field / object name
		if (!in_array) {
			gon->fields[field_index].name = index;
			// Do stuff if the string is in quotes
			bool in_quotes = *index == '"';
			if (in_quotes) {
				gon->fields[field_index].flags |= GON_FIELD_FLAG_NAME_IN_QUOTES;
				(gon->fields[field_index].name)++;
				index++;
				while (*index != '"')
					index += 1 + (*index == '\\');
			}
			else while (!gon_lookup_non_text[*index]) index++;
			null_pos = index;
			//nulls[null_count++] = index;
			// Check that objects have a name
			if ((index - gon->fields[field_index].name) <= in_quotes) {
				printf("GON parse error: encountered unexpected token %c at field %i.\n", *index, field_index);
				return 1;
			}
			index += in_quotes;
		}

		// Skip whitespace and comments
		while (gon_lookup_whitespace[*index]) index++;
		if (*index == '#') {
			while (*index != '\n') index++;
			while (gon_lookup_whitespace[*index]) index++;
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
			printf("reallocated gon fields buffer to %u\n", gon->field_capacity);
			printf("field index %i\n", field_index);
		}
		#else
		if (field_index >= GON_FIELD_BUFFER_SIZE) {
			printf("GON parse error: encountered unexpected token %c at field %i.\n", *index, field_index);
			return 1;
		}
		#endif

		// Read field value
		if (!gon_lookup_non_text[*index]) {
			*null_pos = 0; // can safely place null after field name now
			gon->fields[field_index].type = GON_VALUE_TYPE_FIELD;
			gon->fields[field_index].value = index;

			// Do stuff if the string is in quotes
			bool in_quotes = *index == '"';
			if (in_quotes) {
				gon->fields[field_index].flags |= GON_FIELD_FLAG_VALUE_IN_QUOTES;
				(gon->fields[field_index].value)++;
				index++;
				while (*index != '"')
					index += 1 + (*index == '\\');
			}
			else while (!gon_lookup_non_text[*index]) index++;
			null_pos = index; // defer placing null after field value until either new field name or '}' or ']' is read
			//if (*(gon->fields[FI].Char) == 0) throw "cannot have a field with no value";
			index += in_quotes;
			field_index++;
			continue;
		}

		// This check is only necessary to enforce correctness in a minor way. ( technically the user can begin an object with } or ] without the check )
		// Would be nice if we could get rid of it, but it's probably best to keep it for now.
		// This method seems to be slightly faster than using a switch/case for object, array, and error, since the only possible mispredict is the error case
		if (*index != '{' && *index != '[') {
			printf("GON parse error: encountered unexpected token %c at field %i.\n", *index, field_index);
			return 1;
		}

		// Step into object or array
		in_array = (*index == '[');
		gon->fields[field_index].type = GON_VALUE_TYPE_OBJECT + in_array;

		*null_pos = 0; // can safely place null after name after reading in '{'
		index++; // step over { or [
		depth++;
		if (depth >= GON_MAX_DEPTH) {
			printf("GON parse error: exceeded maximum object depth at field %i\n", field_index);
			return 1;
		}
		parent_index[depth] = field_index;
		field_index++;
	}

	// Set root object size
	gon->fields[0].size = field_index;

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
void gon_serialize(GonFile* gon, char* dst) {
	unsigned int field_index = 1;
	unsigned int depth = 0;
	unsigned int parent_index[GON_MAX_DEPTH];
	unsigned int indent = 0;
	bool in_array = 0;
	parent_index[0] = 0;

	while (1) {
		if (field_index >= gon->fields[0].size && depth == 0) break;

		// Check if object ends
		if (field_index - parent_index[depth] > gon->fields[parent_index[depth]].size) {
			indent -= GON_TAB_WIDTH;
			if (!in_array) memset(dst, ' ', indent), dst += indent; // add indentation
			*dst = in_array ? ']' : '}', dst++;

			depth--;
			in_array = (gon->fields[parent_index[depth]].type == GON_VALUE_TYPE_ARRAY);
			*dst = in_array ? ' ' : '\n'; dst++;
			continue;
		}

		// write field name
		if (!in_array) {
			if (!in_array) memset(dst, ' ', indent), dst += indent;	// add indentation
			bool in_quotes = gon->fields[field_index].flags & GON_FIELD_FLAG_NAME_IN_QUOTES;
			if (in_quotes) *dst = '"', dst++;
			int size = strlen(gon->fields[field_index].name);
			memcpy(dst, gon->fields[field_index].name, size);
			dst += size;
			if (in_quotes) *dst = '"', dst++;
			*dst = ' ', dst++;
		}

		// Write field value
		if (gon->fields[field_index].type == GON_VALUE_TYPE_FIELD) {
			bool in_quotes = gon->fields[field_index].flags & GON_FIELD_FLAG_VALUE_IN_QUOTES;
			if (in_quotes) *dst = '"', dst++;
			int size = strlen(gon->fields[field_index].value);
			memcpy(dst, gon->fields[field_index].value, size);
			dst += size;
			if (in_quotes) *dst = '"', dst++;
			*dst = in_array ? ' ' : '\n'; dst++;
			field_index++;
			continue;
		}

		// Step into object
		if (gon->fields[field_index].type == GON_VALUE_TYPE_ARRAY) {
			in_array = 1;
			*dst = '[', dst++;
			*dst = ' ', dst++;
		}
		else {
			*dst = '{', dst++;
			*dst = '\n', dst++;
		}
		indent += GON_TAB_WIDTH;
		depth++;
		parent_index[depth] = field_index;
		field_index++;
	}

	*dst = '\0'; // write null to end of file
}

// Gets the first child field with the given name if it exists, otherwise returns NULL
// Assumes that parent is not NULL
GonField* gon_get_field(GonField* parent, const char* name) {
	int count = parent->count;
	GonField* field = parent + 1;
	for (int i = 0; i < count; i++) {
		if (field->name && strcmp(field->name, name) == 0)
			return field;
		if (field->type != GON_VALUE_TYPE_FIELD) // step over sub-fields of object and array types
			field += field->size;
		field++;
	}
	return NULL;
}

inline GonFile gon_create(void) {
	GonFile gon;
	memset(&gon, 0, sizeof(GonFile));
	return gon;
}

void gon_free(GonFile* gon) {
	free(gon->file);
	#ifdef GON_USING_DYNAMIC_BUFFER
	free(gon->fields);
	#endif
}
