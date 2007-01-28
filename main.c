#include <time.h>
#include "ref.h"

int main(void)
{
    long tmvalue, tmdummy;	/* timer vars. Should be updated. */
    int control[2] = {0, 0}; /* 0 if player controls; 1 if computer */
    int autopass = 0;
    BoardT board;

    UIInit();
    initPreCalc();
    UIBoardDraw();
    UITicksDraw();

    board.level = 0;
    board.hiswin = 1;	/* set for killer move heuristic */
    newgame(&board);

    printstatus(&board, 0);

    for(;;)
    {
	if (control[board.ply & 1])
	{
	    tmvalue = time(&tmdummy);
	    computermove(&board);
	    printstatus(&board, time(&tmdummy) - tmvalue);
	    concheck(&board, "main1");
	}
	tmvalue = time(&tmdummy);
	if (!autopass)
	{
	    playermove(&board, &autopass, control);
	    printstatus(&board, time(&tmdummy) - tmvalue);
	}
	concheck(&board, "main2");
    }
    return 0;
}
