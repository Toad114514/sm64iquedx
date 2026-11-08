#include "types.h"

extern bool gRomIsValid;
extern char gRomFilename[];

void legacy_folder_handler(void);

bool main_rom_handler(void);
void rom_on_drop_file(const char *path);

// True when the exact 神游 (iQue) Chinese rom dump is present in the game or
// user folder. See rom_checker.cpp for the accepted MD5.
bool rom_checker_ique_rom_present(void);
