#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/JsmnUefiLib.h>
#include <Library/BaseMemoryLib.h>


static CHAR16 *JSON_STRING =
	  L"{\"user\": \"johndoe\", \"admin\": false, \"uid\": 1000,\n  "
	  "\"groups\": [\"users\", \"wheel\", \"audio\", \"video\"]}";

static int jsoneq(const CHAR16 *json, JSMNTOK_T *tok, const CHAR16 *s) {
	if (tok->Type == JSMN_STRING && (INT8) StrLen(s) == tok->End - tok->Start &&
		  CompareMem(json + tok->Start, s, tok->End - tok->Start) == 0) {
	  return 0;
	}

	return -1;
}

EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
	int i;
	int r;
	JSMN_PARSER p;
	JSMNTOK_T t[128]; /* We expect no more than 128 tokens */

	JsmnInit(&p);
	r = JsmnParser(&p, JSON_STRING, StrLen(JSON_STRING), t, sizeof(t)/sizeof(t[0]));
	if (r < 0) {
	  Print(L"Failed to parse JSON: %d\n", r);
	 return 1;
	}

	/* Assume the top-level element is an object */
	if (r < 1 || t[0].Type != JSMN_OBJECT) {
		Print(L"Object expected\n");
		return 1;
	}

	/* Loop over all keys of the root object */
	for (i = 1; i < r; i++) {
		if (jsoneq(JSON_STRING, &t[i], L"user") == 0) {
			/* We may use strndup() to fetch string value */
			Print(L"- User: %.*s\n", t[i+1].End-t[i+1].Start,
					JSON_STRING + t[i+1].Start);
			i++;
		} else if (jsoneq(JSON_STRING, &t[i], L"admin") == 0) {
			/* We may additionally check if the value is either "true" or "false" */
			Print(L"- Admin: %.*s\n", t[i+1].End-t[i+1].Start,
					JSON_STRING + t[i+1].Start);
			i++;
		} else if (jsoneq(JSON_STRING, &t[i], L"uid") == 0) {
			/* We may want to do strtol() here to get numeric value */
			Print(L"- UID: %.*s\n", t[i+1].End-t[i+1].Start,
					JSON_STRING + t[i+1].Start);
			i++;
		} else if (jsoneq(JSON_STRING, &t[i], L"groups") == 0) {
			int j;
			Print(L"- Groups:\n");
			if (t[i+1].Type != JSMN_ARRAY) {
				continue; /* We expect groups to be an array of strings */
			}
			for (j = 0; j < t[i+1].Size; j++) {
				JSMNTOK_T *g = &t[i+j+2];
				Print(L"  * %.*s\n", g->End - g->Start, JSON_STRING + g->Start);
			}
			i += t[i+1].Size + 1;
		} else {
			Print(L"Unexpected key: %.*s\n", t[i].End-t[i].Start,
					JSON_STRING + t[i].Start);
		}
	}

  return EFI_SUCCESS;
}
