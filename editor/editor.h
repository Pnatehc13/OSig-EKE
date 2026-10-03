#ifndef EDITOR_H
#define EDITOR_H

#ifdef __cplusplus
extern "C" {
#endif

// Launches the full-screen on-OS text editor on the given file path
void editor_open(const char* filepath);

#ifdef __cplusplus
}
#endif

#endif // EDITOR_H
