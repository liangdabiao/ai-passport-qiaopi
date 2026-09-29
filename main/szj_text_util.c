// main/szj_text_util.c —— 见 szj_text_util.h。
#include "szj_text_util.h"

#include <string.h>

#include "szj_text.h"

const char *szj_line(int index) {
    if (index < 0 || index >= SZJ_LINE_COUNT) return NULL;
    return szj_lines[index];
}

int szj_stanza_of_line(int line) {
    if (line < 0 || line >= SZJ_LINE_COUNT) return -1;
    return line / SZJ_LINES_PER_STANZA;
}

size_t szj_stanza_text(int stanza, char *out, size_t capacity) {
    if (!out || capacity == 0) return 0;
    out[0] = '\0';
    if (stanza < 0 || stanza >= SZJ_STANZA_COUNT) return 0;

    size_t written = 0;
    const int first = stanza * SZJ_LINES_PER_STANZA;
    for (int i = 0; i < SZJ_LINES_PER_STANZA; i++) {
        const char *line = szj_line(first + i);
        if (!line) return 0;
        const size_t length = strlen(line);
        if (written + length + 1 > capacity) {
            out[0] = '\0';
            return 0;
        }
        memcpy(out + written, line, length);
        written += length;
        out[written] = '\0';
    }
    return written;
}
