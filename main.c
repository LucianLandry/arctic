#include <time.h>
#include "ref.h"

int main(void)
{
    int show = 0;
    long tmvalue, tmdummy;	/* timer vars. Should be updated. */
    int control[2] = {0, 0}; /* 0 if player controls; 1 if computer */
    BoardT board;

    UIInit();
    init(board.moves, board.dir);
    UIBoardDraw();
    UITicksDraw();
    newgame(&board);
    board.depth = 0;
    board.hiswin = 1;	/* set for killer move heuristic */
    printstatus(&board, 0);

    for(;;)
    {
	if (control[board.ply & 1])
	{
	    tmvalue = time(&tmdummy);
	    computermove(&board, &show);
	    printstatus(&board, time(&tmdummy) - tmvalue);
	    if (concheck(&board))
		printplaylist(&board);
	}
	tmvalue = time(&tmdummy);
	playermove(&board, &show, control);
	printstatus(&board, time(&tmdummy) - tmvalue);
	if (concheck(&board))
	    printplaylist(&board);
	
    }
    /* because of anticipated view functions, update() shouldn't be done
       here */
}
