// tests/test_ddj_session.c —— 四层推进：原文 → 点拨 → 参究 → 存档。
#include <assert.h>

#include "ddj_chapter.h"
#include "ddj_session.h"
#include "ddj_text.h"

int main(void)
{
    const ddj_chapter_t *chapter = ddj_chapter_at(0);
    assert(chapter != NULL);
    const int passages = chapter->passage_count;
    const int points = chapter->point_count;

    ddj_session_t s;

    // 进层就停在原文第一屏，并且记得上一次的表态。
    ddj_session_begin(&s, 0, DDJ_SLOT_CHEWING);
    assert(s.chapter == 0);
    assert(s.stage == DDJ_STAGE_READ);
    assert(s.passage == 0 && s.point == 0 && s.cursor == 0);
    assert(s.previous == DDJ_SLOT_CHEWING);
    assert(ddj_session_slot(&s) == DDJ_SLOT_LANDED);

    // 第一屏按返回等于「今天不读了」：什么都不记。
    assert(ddj_session_key(&s, DDJ_KEY_BACK, passages, points, 3) == DDJ_ACT_CANCEL);

    // 上/下在第一屏与最后一屏都不会越界。
    assert(ddj_session_key(&s, DDJ_KEY_UP, passages, points, 3) == DDJ_ACT_NONE);
    assert(s.passage == 0);
    for (int i = 0; i < passages + 3; i++) {
        ddj_session_key(&s, DDJ_KEY_DOWN, passages, points, 3);
    }
    assert(s.passage == passages - 1);

    // 回退一屏，再读到最后一屏：补回退掉的那一屏，再一次确定才换层。
    assert(ddj_session_key(&s, DDJ_KEY_BACK, passages, points, 3) == DDJ_ACT_NONE);
    assert(s.passage == passages - 2);
    assert(ddj_session_key(&s, DDJ_KEY_OK, passages, points, 3) == DDJ_ACT_NONE);
    assert(s.passage == passages - 1);
    assert(s.stage == DDJ_STAGE_READ);
    assert(ddj_session_key(&s, DDJ_KEY_OK, passages, points, 3) == DDJ_ACT_NONE);
    assert(s.stage == DDJ_STAGE_POINT);
    assert(s.point == 0);

    // 在第一条点拨按返回：退回原文并停在最后一句，读起来是连续的。
    assert(ddj_session_key(&s, DDJ_KEY_BACK, passages, points, 3) == DDJ_ACT_NONE);
    assert(s.stage == DDJ_STAGE_READ);
    assert(s.passage == passages - 1);

    // 再从原文最后一屏按一次确定，就回到点拨第一条。
    assert(ddj_session_key(&s, DDJ_KEY_OK, passages, points, 3) == DDJ_ACT_NONE);
    assert(s.stage == DDJ_STAGE_POINT);
    assert(s.point == 0);

    // 点拨条数按真实内容走：上下夹紧，最后一条按确定才进参究。
    for (int i = 0; i < points + 3; i++) {
        ddj_session_key(&s, DDJ_KEY_DOWN, passages, points, 3);
    }
    assert(s.point == points - 1);
    assert(ddj_session_key(&s, DDJ_KEY_OK, passages, points, 3) == DDJ_ACT_NONE);
    assert(s.stage == DDJ_STAGE_PONDER);
    assert(s.cursor == 0);

    // 参究：光标在三个选项之间夹紧，光标决定槽位。
    assert(ddj_session_slot(&s) == DDJ_SLOT_LANDED);
    ddj_session_key(&s, DDJ_KEY_DOWN, passages, points, 3);
    assert(s.cursor == 1 && ddj_session_slot(&s) == DDJ_SLOT_CHEWING);
    ddj_session_key(&s, DDJ_KEY_DOWN, passages, points, 3);
    assert(s.cursor == 2 && ddj_session_slot(&s) == DDJ_SLOT_MISSED);
    ddj_session_key(&s, DDJ_KEY_DOWN, passages, points, 3);
    assert(s.cursor == 2);
    ddj_session_key(&s, DDJ_KEY_UP, passages, points, 3);
    assert(s.cursor == 1);
    ddj_session_key(&s, DDJ_KEY_UP, passages, points, 3);
    ddj_session_key(&s, DDJ_KEY_UP, passages, points, 3);
    assert(s.cursor == 0);

    // 在参究按返回退回点拨，并停在最后一条。
    assert(ddj_session_key(&s, DDJ_KEY_BACK, passages, points, 3) == DDJ_ACT_NONE);
    assert(s.stage == DDJ_STAGE_POINT);
    assert(s.point == points - 1);

    // 回到参究，选中第二项（槽 1）。
    ddj_session_key(&s, DDJ_KEY_OK, passages, points, 3);
    ddj_session_key(&s, DDJ_KEY_DOWN, passages, points, 3);
    assert(s.stage == DDJ_STAGE_PONDER && s.cursor == 1);
    assert(ddj_session_key(&s, DDJ_KEY_OK, passages, points, 3) == DDJ_ACT_NONE);
    assert(s.stage == DDJ_STAGE_DONE);
    assert(ddj_session_slot(&s) == DDJ_SLOT_CHEWING);

    // 存档后两个键都算看完：上下已经没有意义。
    assert(ddj_session_key(&s, DDJ_KEY_UP, passages, points, 3) == DDJ_ACT_NONE);
    assert(ddj_session_key(&s, DDJ_KEY_DOWN, passages, points, 3) == DDJ_ACT_NONE);
    assert(s.stage == DDJ_STAGE_DONE);
    assert(ddj_session_key(&s, DDJ_KEY_OK, passages, points, 3) == DDJ_ACT_FINISHED);

    // 存档层按返回也算看完，不能把人卡在一张死页上。
    ddj_session_begin(&s, 0, DDJ_SLOT_NONE);
    for (int i = 0; i < passages + points + 2; i++) {
        ddj_session_key(&s, DDJ_KEY_OK, passages, points, 3);
    }
    assert(s.stage == DDJ_STAGE_DONE);
    assert(ddj_session_key(&s, DDJ_KEY_BACK, passages, points, 3) == DDJ_ACT_FINISHED);

    // 屏数/条数传 0 或负数时夹到 1，不会算出负下标。
    ddj_session_begin(&s, 0, DDJ_SLOT_NONE);
    assert(ddj_session_key(&s, DDJ_KEY_OK, 0, 0, 0) == DDJ_ACT_NONE);
    assert(s.stage == DDJ_STAGE_POINT);
    assert(s.cursor == 0);
    assert(ddj_session_slot(&s) == DDJ_SLOT_LANDED);
    assert(ddj_session_key(&s, DDJ_KEY_OK, 0, 0, 0) == DDJ_ACT_NONE);
    assert(s.stage == DDJ_STAGE_PONDER);
    assert(ddj_session_key(&s, DDJ_KEY_OK, 0, 0, 0) == DDJ_ACT_NONE);
    assert(s.stage == DDJ_STAGE_DONE);

    // NULL 容错：当取消处理，不崩。
    assert(ddj_session_key(NULL, DDJ_KEY_OK, passages, points, 3) == DDJ_ACT_CANCEL);
    assert(ddj_session_slot(NULL) == DDJ_SLOT_NONE);
    ddj_session_begin(NULL, 0, DDJ_SLOT_NONE);

    // 一章能完整走通：从进层到结束，按键次数是可预期的。
    ddj_session_begin(&s, 0, DDJ_SLOT_NONE);
    int keys = 0;
    while (ddj_session_key(&s, DDJ_KEY_OK, passages, points, 3) == DDJ_ACT_NONE) {
        keys++;
        assert(keys < 100);
    }
    assert(s.stage == DDJ_STAGE_DONE);
    // 原文换层 1 次 + 走完剩余 5 屏 + 点拨换层 1 次 + 走完剩余 4 条 + 参究选中 1 次。
    assert(keys == 12);
    return 0;
}
