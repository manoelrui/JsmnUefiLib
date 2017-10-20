#ifndef __JSMN_UEFI_LIB_H_
#define __JSMN_UEFI_LIB_H_


//
// JSON type identifier. Basic types are:
// 	o Object
// 	o Array
// 	o String
// 	o Other primitive: number, boolean (true/false) or null
//
typedef enum {
	JSMN_UNDEFINED = 0,
	JSMN_OBJECT = 1,
	JSMN_ARRAY = 2,
	JSMN_STRING = 3,
	JSMN_PRIMITIVE = 4
} JSMNTYPE_T;

//
// Jsmn error definitions
//
enum Jsmnerr {
	// Not enough tokens were provided
	JSMN_ERROR_NOMEM = -1,

	// Invalid character inside JSON string
	JSMN_ERROR_INVAL = -2,

	// The string is not a full JSON packet, more bytes expected
	JSMN_ERROR_PART = -3
};

//
// JSON token description.
// type		type (object, array, string etc.)
// start	start position in JSON data string
// end		end position in JSON data string
 //
typedef struct {
	JSMNTYPE_T Type;
	INT32 Start;
	INT32 End;
	INT32 Size;
#ifdef JSMN_PARENT_LINKS
	int Parent;
#endif
} JSMNTOK_T;

//
// JSON parser. Contains an array of token blocks available. Also stores
// the string being parsed now and current position in that string
//
typedef struct {
	UINT32 Pos; 		// offset in the JSON string
	UINT32 Toknext; 	// next token to allocate
	INT32 Toksuper; 	// superior token node, e.g parent object or array
} JSMN_PARSER;


/**
	Create JSON parser over an array of tokens.

	@param  Parser	A pointer to a object parser containing an array of tokens.

**/
VOID
EFIAPI
JsmnInit (
	OUT JSMN_PARSER *Parser
);

/**
	Run JSON parser. It parses a JSON data string into and array of tokens, each describing
	a single JSON object.

	This function has the responsibility to parse JSON string and fill tokens.

	@param  parser		A pointer to a object parser containing an array of tokens.
	@param  js			A pointer to a Null-terminated unicode :q.
	@param  len			The length of input string to be parsed.
	@param  tokens		A pointer to a an array of tokens parsed from input string.
	@param  num_tokens	The maximum number of tokens that is assumed to be parsed.

	@return The number of parsed tokens or a jsmn error while try to parse the input string.

**/
UINT32
EFIAPI
JsmnParser (
	IN JSMN_PARSER *Parser,
	IN CONST CHAR16 *Js,
	IN UINTN Len,
	OUT JSMNTOK_T *Tokens,
	IN UINT32 NumTokens
);


#endif
