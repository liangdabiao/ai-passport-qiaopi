// tests/test_szj_text_util.c —— 经文数据的完整性与边界。
#include <assert.h>
#include <string.h>

#include "szj_text.h"
#include "szj_text_util.h"

int main(void)
{
    assert(szj_line(-1) == NULL);
    assert(szj_line(SZJ_LINE_COUNT) == NULL);
    assert(strcmp(szj_line(0), "人之初") == 0);

    // 每一句都必须是 3 个汉字(9 个 UTF-8 字节),且没有空项,
    // 否则界面上的「田字格」会错位。
    for (int i = 0; i < SZJ_LINE_COUNT; i++) {
        const char *line = szj_line(i);
        assert(line != NULL);
        assert(strlen(line) == 9);
    }

    // 每课 4 句。
    assert(szj_stanza_of_line(-1) == -1);
    assert(szj_stanza_of_line(SZJ_LINE_COUNT) == -1);
    assert(szj_stanza_of_line(0) == 0);
    assert(szj_stanza_of_line(3) == 0);
    assert(szj_stanza_of_line(4) == 1);
    assert(szj_stanza_of_line(SZJ_LINE_COUNT - 1) == SZJ_STANZA_COUNT - 1);

    char text[SZJ_STANZA_TEXT_CAPACITY];
    const size_t written = szj_stanza_text(0, text, sizeof(text));
    assert(written == 36);
    assert(strcmp(text, "人之初性本善性相近习相远") == 0);

    // 每一课都能拼出 12 字(36 字节);越界与容量不足必须安全失败并清空输出。
    for (int stanza = 0; stanza < SZJ_STANZA_COUNT; stanza++) {
        assert(szj_stanza_text(stanza, text, sizeof(text)) == 36);
        assert(strlen(text) == 36);
    }

    assert(szj_stanza_text(0, text, 8) == 0);
    assert(text[0] == '\0');
    assert(szj_stanza_text(-1, text, sizeof(text)) == 0);
    assert(szj_stanza_text(SZJ_STANZA_COUNT, text, sizeof(text)) == 0);
    assert(szj_stanza_text(0, NULL, sizeof(text)) == 0);
    assert(szj_stanza_text(0, text, 0) == 0);

    // 常量之间必须自洽,免得界面按错的行数分课。
    assert(SZJ_LINES_PER_STANZA == 4);
    assert(SZJ_LINE_COUNT % SZJ_LINES_PER_STANZA == 0);
    assert(SZJ_STANZA_COUNT * SZJ_LINES_PER_STANZA == SZJ_LINE_COUNT);
    assert(SZJ_LINE_BYTES == 10);
    return 0;
}
