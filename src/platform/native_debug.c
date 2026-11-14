#include "WL_DEF.H"

void PicturePause(void)
{
    IN_StartAck();
    while (!IN_CheckAck()) {
        VL_WaitVBL(1);
    }
}

int DebugKeys(void)
{
    return 0;
}
