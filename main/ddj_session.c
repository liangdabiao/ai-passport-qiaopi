// main/ddj_session.c —— 见 ddj_session.h。
#include "ddj_session.h"

static int clamp(int value, int low, int high)
{
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static int at_least_one(int count)
{
    return count < 1 ? 1 : count;
}

void ddj_session_begin(ddj_session_t *session, int chapter, ddj_slot_t previous)
{
    if (!session) return;
    session->chapter = chapter;
    session->stage = DDJ_STAGE_READ;
    session->passage = 0;
    session->point = 0;
    session->cursor = 0;
    session->previous = previous;
}

ddj_slot_t ddj_session_slot(const ddj_session_t *session)
{
    if (!session) return DDJ_SLOT_NONE;
    const int slot = (int)DDJ_SLOT_LANDED + session->cursor;
    if (slot <= DDJ_SLOT_NONE || slot >= DDJ_SLOT_COUNT) return DDJ_SLOT_NONE;
    return (ddj_slot_t)slot;
}

ddj_act_t ddj_session_key(ddj_session_t *session, ddj_key_t key,
                          int passage_count, int point_count, int option_count)
{
    if (!session) return DDJ_ACT_CANCEL;

    const int passages = at_least_one(passage_count);
    const int points = at_least_one(point_count);
    const int options = at_least_one(option_count);

    // 存档之后只剩「结束」这一个动作，两个键都算看完。
    if (session->stage == DDJ_STAGE_DONE) {
        return (key == DDJ_KEY_OK || key == DDJ_KEY_BACK) ? DDJ_ACT_FINISHED : DDJ_ACT_NONE;
    }

    if (session->stage == DDJ_STAGE_READ) {
        switch (key) {
            case DDJ_KEY_UP:
                session->passage = clamp(session->passage - 1, 0, passages - 1);
                return DDJ_ACT_NONE;
            case DDJ_KEY_DOWN:
                session->passage = clamp(session->passage + 1, 0, passages - 1);
                return DDJ_ACT_NONE;
            case DDJ_KEY_OK:
                if (session->passage < passages - 1) {
                    session->passage++;
                } else {
                    session->stage = DDJ_STAGE_POINT;
                    session->point = 0;
                }
                return DDJ_ACT_NONE;
            default: /* DDJ_KEY_BACK */
                if (session->passage > 0) {
                    session->passage--;
                    return DDJ_ACT_NONE;
                }
                // 第一屏就按返回：这一章没读完，什么都不记。
                return DDJ_ACT_CANCEL;
        }
    }

    if (session->stage == DDJ_STAGE_POINT) {
        switch (key) {
            case DDJ_KEY_UP:
                session->point = clamp(session->point - 1, 0, points - 1);
                return DDJ_ACT_NONE;
            case DDJ_KEY_DOWN:
                session->point = clamp(session->point + 1, 0, points - 1);
                return DDJ_ACT_NONE;
            case DDJ_KEY_OK:
                if (session->point < points - 1) {
                    session->point++;
                } else {
                    session->stage = DDJ_STAGE_PONDER;
                    session->cursor = 0;
                }
                return DDJ_ACT_NONE;
            default: /* DDJ_KEY_BACK */
                if (session->point > 0) {
                    session->point--;
                } else {
                    // 退回原文，并停在最后一句上，读起来是连续的。
                    session->stage = DDJ_STAGE_READ;
                    session->passage = passages - 1;
                }
                return DDJ_ACT_NONE;
        }
    }

    // DDJ_STAGE_PONDER
    switch (key) {
        case DDJ_KEY_UP:
            session->cursor = clamp(session->cursor - 1, 0, options - 1);
            return DDJ_ACT_NONE;
        case DDJ_KEY_DOWN:
            session->cursor = clamp(session->cursor + 1, 0, options - 1);
            return DDJ_ACT_NONE;
        case DDJ_KEY_OK:
            // 选中即表态。到这里才可能被记下来。
            session->stage = DDJ_STAGE_DONE;
            return DDJ_ACT_NONE;
        default: /* DDJ_KEY_BACK */
            session->stage = DDJ_STAGE_POINT;
            session->point = points - 1;
            return DDJ_ACT_NONE;
    }
}
