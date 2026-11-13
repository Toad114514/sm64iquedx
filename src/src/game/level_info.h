#ifndef LEVEL_INFO_H
#define LEVEL_INFO_H

#include <PR/ultratypes.h>

void **get_course_name_table(void);
void **get_course_name_table_original(void);
void **get_act_name_table(void);
void **get_act_name_table_original(void);
u8 *convert_string_ascii_to_sm64(u8 *str64, const char *strAscii, bool menu);
char *convert_string_sm64_to_ascii(char *strAscii, const u8 *str64);
/* |description|
Convert one single-byte font code to the text it stands for, NUL-terminated.
The mapping without the "is this a CN buffer?" policy: used by the UTF-8 decoder
(cn_unicode.c), which is decoding bytes of a cartridge string and cannot call
convert_string_sm64_to_ascii() without recursing back into itself.
`out` must hold at least SM64_TO_ASCII_MAX_CHAR_SIZE + 1 bytes.
|descriptionEnd| */
char *sm64_char_to_ascii(char *out, u8 c);
/* |description|
Convert one printable ASCII character to the engine's single-byte dialog font
index (the value render_generic_char() and gDialogCharWidths[] expect).
Unknown characters fall back to the space glyph. Used by the UTF-8 text walker,
which sees plain ASCII and has to feed the vanilla renderer.
|descriptionEnd| */
u8 ascii_char_to_sm64(char c, bool menu);
/* |description|
Returns the name of the level corresponding to `courseNum`, `levelNum` and `areaIndex` as an ASCII (human readable) string.
Set `charCase` to 1 to capitalize or -1 to decapitalize the returned string
|descriptionEnd| */
const char *get_level_name_ascii(s16 courseNum, s16 levelNum, s16 areaIndex, s16 charCase);
/* |description|
Returns the name of the level corresponding to `courseNum`, `levelNum` and `areaIndex` as an SM64 encoded string.
This function should not be used in Lua mods.
Set `charCase` to 1 to capitalize or -1 to decapitalize the returned string
|descriptionEnd| */
const u8 *get_level_name_sm64(s16 courseNum, s16 levelNum, s16 areaIndex, s16 charCase);
/* |description|
Returns the name of the level corresponding to `courseNum`, `levelNum` and `areaIndex` as a decapitalized ASCII (human readable) string
|descriptionEnd| */
const char *get_level_name(s16 courseNum, s16 levelNum, s16 areaIndex);
/* |description|
Returns the name of the star corresponding to `courseNum` and `starNum` as an ASCII (human readable) string.
Set `charCase` to 1 to capitalize or -1 to decapitalize the returned string
|descriptionEnd| */
const char *get_star_name_ascii(s16 courseNum, s16 starNum, s16 charCase);
/* |description|
Returns the name of the star corresponding to `courseNum` and `starNum` as an SM64 encoded string.
This function should not be used in Lua mods.
Set `charCase` to 1 to capitalize or -1 to decapitalize the returned string
|descriptionEnd| */
const u8 *get_star_name_sm64(s16 courseNum, s16 starNum, s16 charCase);
/* |description|
Returns the name of the star corresponding to `courseNum` and `starNum` as a decapitalized ASCII (human readable) string
|descriptionEnd| */
const char *get_star_name(s16 courseNum, s16 starNum);

#endif // LEVEL_INFO_H
