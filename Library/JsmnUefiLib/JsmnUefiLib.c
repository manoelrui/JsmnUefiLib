#include <Uefi.h>
#include <Library/JsmnUefiLib.h>


/**
 * Allocates a fresh unused token from the token pull.
 */
static JSMNTOK_T *Jsmn_alloc_token(JSMN_PARSER *Parser,
		JSMNTOK_T *Tokens, UINTN NumTokens) {
	JSMNTOK_T *tok;
	if (Parser->Toknext >= NumTokens) {
		return NULL;
	}
	tok = &Tokens[Parser->Toknext++];
	tok->Start = tok->End = -1;
	tok->Size = 0;
#ifdef JSMN_PARENT_LINKS
	tok->Parent = -1;
#endif
	return tok;
}

/**
 * Fills token Type and boundaries.
 */
static void Jsmn_fill_token(JSMNTOK_T *token, JSMNTYPE_T Type,
                            int Start, int End) {
	token->Type = Type;
	token->Start = Start;
	token->End = End;
	token->Size = 0;
}

/**
 * Fills next available token with JSON primitive.
 */
static int Jsmn_parse_primitive(JSMN_PARSER *Parser, const CHAR16 *Js,
		UINTN Len, JSMNTOK_T *Tokens, UINTN NumTokens) {
	JSMNTOK_T *token;
	int Start;

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
	token = Jsmn_alloc_token(Parser, Tokens, NumTokens);
	if (token == NULL) {
		Parser->Pos = Start;
		return JSMN_ERROR_NOMEM;
	}
	Jsmn_fill_token(token, JSMN_PRIMITIVE, Start, Parser->Pos);
#ifdef JSMN_PARENT_LINKS
	token->Parent = Parser->Toksuper;
#endif
	Parser->Pos--;
	return 0;
}

/**
 * Fills next token with JSON string.
 */
static int Jsmn_parse_string(JSMN_PARSER *Parser, const CHAR16 *Js,
		UINTN Len, JSMNTOK_T *Tokens, UINTN NumTokens) {
	JSMNTOK_T *token;

	int Start = Parser->Pos;

	Parser->Pos++;

	/* Skip Starting quote */
	for (; Parser->Pos < Len && Js[Parser->Pos] != '\0'; Parser->Pos++) {
		char c = Js[Parser->Pos];

		/* Quote: End of string */
		if (c == '\"') {
			if (Tokens == NULL) {
				return 0;
			}
			token = Jsmn_alloc_token(Parser, Tokens, NumTokens);
			if (token == NULL) {
				Parser->Pos = Start;
				return JSMN_ERROR_NOMEM;
			}
			Jsmn_fill_token(token, JSMN_STRING, Start+1, Parser->Pos);
#ifdef JSMN_PARENT_LINKS
			token->Parent = Parser->Toksuper;
#endif
			return 0;
		}

		/* Backslash: Quoted symbol expected */
		if (c == '\\' && Parser->Pos + 1 < Len) {
			int i;
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
	int r;
	int i;
	JSMNTOK_T *token;
	int count = Parser->Toknext;

	for (; Parser->Pos < Len && Js[Parser->Pos] != '\0'; Parser->Pos++) {
		char c;
		JSMNTYPE_T Type;

		c = Js[Parser->Pos];
		switch (c) {
			case '{': case '[':
				count++;
				if (Tokens == NULL) {
					break;
				}
				token = Jsmn_alloc_token(Parser, Tokens, NumTokens);
				if (token == NULL)
					return JSMN_ERROR_NOMEM;
				if (Parser->Toksuper != -1) {
					Tokens[Parser->Toksuper].Size++;
#ifdef JSMN_PARENT_LINKS
					token->Parent = Parser->Toksuper;
#endif
				}
				token->Type = (c == '{' ? JSMN_OBJECT : JSMN_ARRAY);
				token->Start = Parser->Pos;
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
				token = &Tokens[Parser->Toknext - 1];
				for (;;) {
					if (token->Start != -1 && token->End == -1) {
						if (token->Type != Type) {
							return JSMN_ERROR_INVAL;
						}
						token->End = Parser->Pos + 1;
						Parser->Toksuper = token->Parent;
						break;
					}
					if (token->Parent == -1) {
						if(token->Type != Type || Parser->Toksuper == -1) {
							return JSMN_ERROR_INVAL;
						}
						break;
					}
					token = &Tokens[token->Parent];
				}
#else
				for (i = Parser->Toknext - 1; i >= 0; i--) {
					token = &Tokens[i];
					if (token->Start != -1 && token->End == -1) {
						if (token->Type != Type) {
							return JSMN_ERROR_INVAL;
						}
						Parser->Toksuper = -1;
						token->End = Parser->Pos + 1;
						break;
					}
				}
				/* Error if unmatched closing bracket */
				if (i == -1) return JSMN_ERROR_INVAL;
				for (; i >= 0; i--) {
					token = &Tokens[i];
					if (token->Start != -1 && token->End == -1) {
						Parser->Toksuper = i;
						break;
					}
				}
#endif
				break;
			case '\"':
				r = Jsmn_parse_string(Parser, Js, Len, Tokens, NumTokens);
				if (r < 0) return r;
				count++;
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
				r = Jsmn_parse_primitive(Parser, Js, Len, Tokens, NumTokens);
				if (r < 0) return r;
				count++;
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

	return count;
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

