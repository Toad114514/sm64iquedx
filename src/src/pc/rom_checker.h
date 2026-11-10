#include "types.h"

extern bool gRomIsValid;
extern char gRomFilename[];

void legacy_folder_handler(void);

bool main_rom_handler(void);
void rom_on_drop_file(const char *path);

// True when the exact 神游 (iQue) Chinese rom dump is present in the game or
// user folder. See rom_checker.cpp for the accepted MD5.
bool rom_checker_ique_rom_present(void);

// Path to that rom, or NULL when it is not there. The Chinese text pack is
// *read out of this file at runtime* rather than compiled in, so the path is
// not merely a signal any more -- without it there is no glyph data at all.
// The returned pointer stays valid for the rest of the session.
const char *rom_checker_ique_rom_path(void);
