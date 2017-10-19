#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/JsmnUefiLib.h>
#include <Library/BaseMemoryLib.h>


static CHAR16 *JSON_STRING =
	  L"{\"user\": \"johndoe\", \"admin\": false, \"uid\": 1000,\n  "
	  "\"groups\": [\"users\", \"wheel\", \"audio\", \"video\"]}";

static int jsoneq(const CHAR16 *json, jsmntok_t *tok, const CHAR16 *s) {
	if (tok->type == JSMN_STRING && (INT8) StrLen(s) == tok->end - tok->start &&
		  CompareMem(json + tok->start, s, tok->end - tok->start) == 0) {
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
	jsmn_parser p;
	jsmntok_t t[128]; /* We expect no more than 128 tokens */

	jsmn_init(&p);
	r = jsmn_parse(&p, JSON_STRING, StrLen(JSON_STRING), t, sizeof(t)/sizeof(t[0]));
	if (r < 0) {
	  Print(L"Failed to parse JSON: %d\n", r);
	 return 1;
	}

	/* Assume the top-level element is an object */
	if (r < 1 || t[0].type != JSMN_OBJECT) {
		Print(L"Object expected\n");
		return 1;
	}

	/* Loop over all keys of the root object */
	for (i = 1; i < r; i++) {
		if (jsoneq(JSON_STRING, &t[i], L"user") == 0) {
			/* We may use strndup() to fetch string value */
			Print(L"- User: %.*s\n", t[i+1].end-t[i+1].start,
					JSON_STRING + t[i+1].start);
			i++;
		} else if (jsoneq(JSON_STRING, &t[i], L"admin") == 0) {
			/* We may additionally check if the value is either "true" or "false" */
			Print(L"- Admin: %.*s\n", t[i+1].end-t[i+1].start,
					JSON_STRING + t[i+1].start);
			i++;
		} else if (jsoneq(JSON_STRING, &t[i], L"uid") == 0) {
			/* We may want to do strtol() here to get numeric value */
			Print(L"- UID: %.*s\n", t[i+1].end-t[i+1].start,
					JSON_STRING + t[i+1].start);
			i++;
		} else if (jsoneq(JSON_STRING, &t[i], L"groups") == 0) {
			int j;
			Print(L"- Groups:\n");
			if (t[i+1].type != JSMN_ARRAY) {
				continue; /* We expect groups to be an array of strings */
			}
			for (j = 0; j < t[i+1].size; j++) {
				jsmntok_t *g = &t[i+j+2];
				Print(L"  * %.*s\n", g->end - g->start, JSON_STRING + g->start);
			}
			i += t[i+1].size + 1;
		} else {
			Print(L"Unexpected key: %.*s\n", t[i].end-t[i].start,
					JSON_STRING + t[i].start);
		}
	}

  return EFI_SUCCESS;
}
