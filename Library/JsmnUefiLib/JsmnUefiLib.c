#include <Uefi.h>
#include <Library/JsmnUefiLib.h>


//
// Allocates a fresh unused Token from the Token pull.
//
STATIC
JSMNTOK_T*
JsmnAllocToken (
	IN JSMN_PARSER *Parser,
	IN JSMNTOK_T *Tokens,
	IN UINTN NumTokens
	)
{
	JSMNTOK_T *Tok;
	if (Parser->Toknext >= NumTokens) {
		return NULL;
	}
	Tok = &Tokens[Parser->Toknext++];
	Tok->Start = Tok->End = -1;
	Tok->Size = 0;
#ifdef JSMN_PARENT_LINKS
	Tok->Parent = -1;
#endif
	return Tok;
}

//
// Fills Token Type and boundaries.
//
STATIC
VOID
JsmnFillToken (
	OUT JSMNTOK_T *Token,
	IN JSMNTYPE_T Type,
    IN INT32 Start,
	IN INT32 End
	)
{
	Token->Type = Type;
	Token->Start = Start;
	Token->End = End;
	Token->Size = 0;
}

//
// Fills next available Token with JSON primitive.
//
STATIC
UINT32
JsmnParsePrimitive (
	IN JSMN_PARSER *Parser,
	IN CONST CHAR16 *Js,
	IN UINTN Len,
	OUT JSMNTOK_T *Tokens,
	IN UINTN NumTokens
	)
{
	JSMNTOK_T *Token;
	UINT32 Start;

	Start = Parser->Pos;

	for (; Parser->Pos < Len && Js[Parser->Pos] != '\0'; Parser->Pos++) {
		switch (Js[Parser->Pos]) {
#ifndef JSMN_STRICT
			/* In strict mode primitive must be followed by "," or "}" or "]" */
			case ':':
#endif
			case '\t' : case '\r' : case '\n' : case ' ' :
			case ','  : case ']'  : case '}' :
				goto found;
		}
		if (Js[Parser->Pos] < 32 || Js[Parser->Pos] >= 127) {
			Parser->Pos = Start;
			return JSMN_ERROR_INVAL;
		}
	}
#ifdef JSMN_STRICT
	/* In strict mode primitive must be followed by a comma/object/array */
	Parser->Pos = Start;
	return JSMN_ERROR_PART;
#endif

found:
	if (Tokens == NULL) {
		Parser->Pos--;
		return 0;
	}
	Token = JsmnAllocToken(Parser, Tokens, NumTokens);
	if (Token == NULL) {
		Parser->Pos = Start;
		return JSMN_ERROR_NOMEM;
	}
	JsmnFillToken(Token, JSMN_PRIMITIVE, Start, Parser->Pos);
#ifdef JSMN_PARENT_LINKS
	Token->Parent = Parser->Toksuper;
#endif
	Parser->Pos--;
	return 0;
}

//
// Fills next Token with JSON string.
//
STATIC
UINT32
JsmnParseString (
	IN JSMN_PARSER *Parser,
	IN CONST CHAR16 *Js,
	IN UINTN Len,
	OUT JSMNTOK_T *Tokens,
	IN UINTN NumTokens
	)
{
	JSMNTOK_T *Token;

	UINT32 Start = Parser->Pos;

	Parser->Pos++;

	/* Skip Starting quote */
	for (; Parser->Pos < Len && Js[Parser->Pos] != '\0'; Parser->Pos++) {
		CHAR16 c = Js[Parser->Pos];

		/* Quote: End of string */
		if (c == '\"') {
			if (Tokens == NULL) {
				return 0;
			}
			Token = JsmnAllocToken(Parser, Tokens, NumTokens);
			if (Token == NULL) {
				Parser->Pos = Start;
				return JSMN_ERROR_NOMEM;
			}
			JsmnFillToken(Token, JSMN_STRING, Start+1, Parser->Pos);
#ifdef JSMN_PARENT_LINKS
			Token->Parent = Parser->Toksuper;
#endif
			return 0;
		}

		/* Backslash: Quoted symbol expected */
		if (c == '\\' && Parser->Pos + 1 < Len) {
			UINT32 i;
			Parser->Pos++;
			switch (Js[Parser->Pos]) {
				/* Allowed escaped symbols */
				case '\"': case '/' : case '\\' : case 'b' :
				case 'f' : case 'r' : case 'n'  : case 't' :
					break;
				/* Allows escaped symbol \uXXXX */
				case 'u':
					Parser->Pos++;
					for(i = 0; i < 4 && Parser->Pos < Len && Js[Parser->Pos] != '\0'; i++) {
						/* If it isn't a hex character we have an error */
						if(!((Js[Parser->Pos] >= 48 && Js[Parser->Pos] <= 57) || /* 0-9 */
									(Js[Parser->Pos] >= 65 && Js[Parser->Pos] <= 70) || /* A-F */
									(Js[Parser->Pos] >= 97 && Js[Parser->Pos] <= 102))) { /* a-f */
							Parser->Pos = Start;
							return JSMN_ERROR_INVAL;
						}
						Parser->Pos++;
					}
					Parser->Pos--;
					break;
				/* Unexpected symbol */
				default:
					Parser->Pos = Start;
					return JSMN_ERROR_INVAL;
			}
		}
	}
	Parser->Pos = Start;
	return JSMN_ERROR_PART;
}

/**
	Run JSON Parser. It parses a JSON data string into and array of Tokens, each describing
	a single JSON object.

	This function has the responsibility to parse JSON string and fill Tokens.

	@param  Parser		A pointer to a object Parser containing an array of Tokens.
	@param  Js			A pointer to a Null-terminated unicode :q.
	@param  Len			The Length of input string to be parsed.
	@param  Tokens		A pointer to a an array of Tokens parsed from input string.
	@param  NumTokens	The maximum number of Tokens that is assumed to be parsed.

	@return The number of parsed Tokens or a Jsmn error while try to parse the input string.

**/
UINT32
EFIAPI
JsmnParser (
	IN JSMN_PARSER *Parser,
	IN CONST CHAR16 *Js,
	IN UINTN Len,
	OUT JSMNTOK_T *Tokens,
	IN UINT32 NumTokens
	)
{
	INT32 r;
	INT32 i;
	JSMNTOK_T *Token;
	INT32 Count = Parser->Toknext;

	for (; Parser->Pos < Len && Js[Parser->Pos] != '\0'; Parser->Pos++) {
		CHAR16 c;
		JSMNTYPE_T Type;

		c = Js[Parser->Pos];
		switch (c) {
			case '{': case '[':
				Count++;
				if (Tokens == NULL) {
					break;
				}
				Token = JsmnAllocToken(Parser, Tokens, NumTokens);
				if (Token == NULL)
					return JSMN_ERROR_NOMEM;
				if (Parser->Toksuper != -1) {
					Tokens[Parser->Toksuper].Size++;
#ifdef JSMN_PARENT_LINKS
					Token->Parent = Parser->Toksuper;
#endif
				}
				Token->Type = (c == '{' ? JSMN_OBJECT : JSMN_ARRAY);
				Token->Start = Parser->Pos;
				Parser->Toksuper = Parser->Toknext - 1;
				break;
			case '}': case ']':
				if (Tokens == NULL)
					break;
				Type = (c == '}' ? JSMN_OBJECT : JSMN_ARRAY);
#ifdef JSMN_PARENT_LINKS
				if (Parser->Toknext < 1) {
					return JSMN_ERROR_INVAL;
				}
				Token = &Tokens[Parser->Toknext - 1];
				for (;;) {
					if (Token->Start != -1 && Token->End == -1) {
						if (Token->Type != Type) {
							return JSMN_ERROR_INVAL;
						}
						Token->End = Parser->Pos + 1;
						Parser->Toksuper = Token->Parent;
						break;
					}
					if (Token->Parent == -1) {
						if(Token->Type != Type || Parser->Toksuper == -1) {
							return JSMN_ERROR_INVAL;
						}
						break;
					}
					Token = &Tokens[Token->Parent];
				}
#else
				for (i = Parser->Toknext - 1; i >= 0; i--) {
					Token = &Tokens[i];
					if (Token->Start != -1 && Token->End == -1) {
						if (Token->Type != Type) {
							return JSMN_ERROR_INVAL;
						}
						Parser->Toksuper = -1;
						Token->End = Parser->Pos + 1;
						break;
					}
				}
				/* Error if unmatched closing bracket */
				if (i == -1) return JSMN_ERROR_INVAL;
				for (; i >= 0; i--) {
					Token = &Tokens[i];
					if (Token->Start != -1 && Token->End == -1) {
						Parser->Toksuper = i;
						break;
					}
				}
#endif
				break;
			case '\"':
				r = JsmnParseString(Parser, Js, Len, Tokens, NumTokens);
				if (r < 0) return r;
				Count++;
				if (Parser->Toksuper != -1 && Tokens != NULL)
					Tokens[Parser->Toksuper].Size++;
				break;
			case '\t' : case '\r' : case '\n' : case ' ':
				break;
			case ':':
				Parser->Toksuper = Parser->Toknext - 1;
				break;
			case ',':
				if (Tokens != NULL && Parser->Toksuper != -1 &&
						Tokens[Parser->Toksuper].Type != JSMN_ARRAY &&
						Tokens[Parser->Toksuper].Type != JSMN_OBJECT) {
#ifdef JSMN_PARENT_LINKS
					Parser->Toksuper = Tokens[Parser->Toksuper].Parent;
#else
					for (i = Parser->Toknext - 1; i >= 0; i--) {
						if (Tokens[i].Type == JSMN_ARRAY || Tokens[i].Type == JSMN_OBJECT) {
							if (Tokens[i].Start != -1 && Tokens[i].End == -1) {
								Parser->Toksuper = i;
								break;
							}
						}
					}
#endif
				}
				break;
#ifdef JSMN_STRICT
			/* In strict mode primitives are: numbers and booleans */
			case '-': case '0': case '1' : case '2': case '3' : case '4':
			case '5': case '6': case '7' : case '8': case '9':
			case 't': case 'f': case 'n' :
				/* And they must not be keys of the object */
				if (Tokens != NULL && Parser->Toksuper != -1) {
					JSMNTOK_T *t = &Tokens[Parser->Toksuper];
					if (t->Type == JSMN_OBJECT ||
							(t->Type == JSMN_STRING && t->Size != 0)) {
						return JSMN_ERROR_INVAL;
					}
				}
#else
			/* In non-strict mode every unquoted value is a primitive */
			default:
#endif
				r = JsmnParsePrimitive(Parser, Js, Len, Tokens, NumTokens);
				if (r < 0) return r;
				Count++;
				if (Parser->Toksuper != -1 && Tokens != NULL)
					Tokens[Parser->Toksuper].Size++;
				break;

#ifdef JSMN_STRICT
			/* Unexpected char in strict mode */
			default:
				return JSMN_ERROR_INVAL;
#endif
		}
	}

	if (Tokens != NULL) {
		for (i = Parser->Toknext - 1; i >= 0; i--) {
			/* Unmatched opened object or array */
			if (Tokens[i].Start != -1 && Tokens[i].End == -1) {
				return JSMN_ERROR_PART;
			}
		}
	}

	return Count;
}

/**
	Create JSON Parser over an array of Tokens.

	@param  Parser	A pointer to a object Parser containing an array of Tokens.

**/
VOID
EFIAPI
JsmnInit (
	OUT JSMN_PARSER *Parser
	)
{
	Parser->Pos = 0;
	Parser->Toknext = 0;
	Parser->Toksuper = -1;
}
