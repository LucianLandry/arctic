#include <conio.h>
#include "ref.h"
#include <time.h>

int main(void)
{    int show = 0;
	int col[2] = {LIGHTCYAN, LIGHTGRAY};
	long tmvalue, tmdummy;	/* timer vars. Should be updated. */
	int control[2] = {0, 0}; /* 0 if player controls; 1 if computer */
	struct brd board;
	char mvarray[912] = {
					FLAG,
					 8, FLAG,
					 9, 16, FLAG,
					10, 17, 24, FLAG,
					11, 18, 25, 32, FLAG,
					12, 19, 26, 33, 40, FLAG,
					13, 20, 27, 34, 41, 48, FLAG,
/* 0 dir */			14, 21, 28, 35, 42, 49, 56, FLAG,
					    22, 29, 36, 43, 50, 57, FLAG,
					        30, 37, 44, 51, 58, FLAG,
                                     38, 45, 52, 59, FLAG,
								 46, 53, 60, FLAG,
									54, 61, FLAG,
									    62, FLAG,
                              				   FLAG,

                          8, 16, 24, 32, 40, 48, 56, FLAG,
					 9, 17, 25, 33, 41, 49, 57, FLAG,
					10, 18, 26, 34, 42, 50, 58, FLAG,
/* 1 dir */			11, 19, 27, 35, 43, 51, 59, FLAG,
					12, 20, 28, 36, 44, 52, 60, FLAG,
					13, 21, 29, 37, 45, 53, 61, FLAG,
					14, 22, 30, 38, 46, 54, 62, FLAG,
					15, 23, 31, 39, 47, 55, 63, FLAG,



										   FLAG,
									    15, FLAG,
									14, 23, FLAG,
								 13, 22, 31, FLAG,
							  12, 21, 30, 39, FLAG,
						   11, 20, 29, 38, 47, FLAG,
					    10, 19, 28, 37, 46, 55, FLAG,
					 9, 18, 27, 36, 45, 54, 63, FLAG,
					17, 26, 35, 44, 53, 62, FLAG,
/* 2 dir */			25, 34, 43, 52, 61, FLAG,
					33, 42, 51, 60, FLAG,
					41, 50, 59, FLAG,
					49, 58, FLAG,
					57, FLAG,
					FLAG,






					 1,  2,  3,  4,  5,  6,  7, FLAG,
					 9, 10, 11, 12, 13, 14, 15, FLAG,
					17, 18, 19, 20, 21, 22, 23, FLAG,
					25, 26, 27, 28, 29, 30, 31, FLAG,
/* 3 dir */			33, 34, 35, 36, 37, 38, 39, FLAG,
					41, 42, 43, 44, 45, 46, 47, FLAG,
					49, 50, 51, 52, 53, 54, 55, FLAG,
					57, 58, 59, 60, 61, 62, 63, FLAG,


                              				   FLAG,
									    55, FLAG,
									54, 47, FLAG,
							      53, 46, 39, FLAG,
						       52, 45, 38, 31, FLAG,
					        51, 44, 37, 30, 23, FLAG,
					    50, 43, 36, 29, 22, 15, FLAG,
/* 4 dir */			49, 42, 35, 28, 21, 14,  7, FLAG,
					41, 34, 27, 20, 13,  6, FLAG,
					33, 26, 19, 12,  5, FLAG,
					25, 18, 11,  4, FLAG,
					17, 10,  3, FLAG,
					 9,  2, FLAG,
					 1, FLAG,
					FLAG,

					48, 40, 32, 24, 16,  8,  0, FLAG,
					49, 41, 33, 25, 17,  9,  1, FLAG,
					50, 42, 34, 26, 18, 10,  2, FLAG,
/* 5 dir */			51, 43, 35, 27, 19, 11,  3, FLAG,
					52, 44, 36, 28, 20, 12,  4, FLAG,
					53, 45, 37, 29, 21, 13,  5, FLAG,
					54, 46, 38, 30, 22, 14,  6, FLAG,
					55, 47, 39, 31, 23, 15,  7, FLAG,

					FLAG,
					48, FLAG,
					49, 40, FLAG,
					50, 41, 32, FLAG,
					51, 42, 33, 24, FLAG,
					52, 43, 34, 25, 16, FLAG,
					53, 44, 35, 26, 17,  8, FLAG,
/* 6 dir */			54, 45, 36, 27, 18,  9,  0, FLAG,
					    46, 37, 28, 19, 10,  1, FLAG,
						   38, 29, 20, 11,  2, FLAG,
							  30, 21, 12,  3, FLAG,
								 22, 13,  4, FLAG,
									14,  5, FLAG,
										6, FLAG,
										   FLAG,

					 6,  5,  4,  3,  2,  1,  0, FLAG,
					14, 13, 12, 11, 10,  9,  8, FLAG,
					22, 21, 20, 19, 18, 17, 16, FLAG,
/* 7 dir */			30, 29, 28, 27, 26, 25, 24, FLAG,
					38, 37, 36, 35, 34, 33, 32, FLAG,
					46, 45, 44, 43, 42, 41, 40, FLAG,
					54, 53, 52, 51, 50, 49, 48, FLAG,
					62, 61, 60, 59, 58, 57, 56, FLAG,

/* '8' dir :) */		17, 10, FLAG,
					16, 18, 11, FLAG,
					 8, 17, 19, 12, FLAG,
/* 1st rank */			 9, 18, 20, 13, FLAG,
					10, 19, 21, 14, FLAG,
					11, 20, 22, 15, FLAG,
					12, 21, 23, FLAG,
					13, 22, FLAG,

					25, 18,  2, FLAG,
					24, 26, 19,  3, FLAG,
					 0, 16, 25, 27, 20,  4, FLAG,
/* 2nd rank */			 1, 17, 26, 28, 21,  5, FLAG,
					 2, 18, 27, 29, 22,  6, FLAG,
					 3, 19, 28, 30, 23,  7, FLAG,
					 4, 20, 29, 31, FLAG,
					 5, 21, 30, FLAG,

					33, 26, 10,  1, FLAG,
					32, 34, 27, 11,  2,  0, FLAG,
					33, 35, 28, 12,  3,  1,  8, 24, FLAG,
/* 3rd rank */			34, 36, 29, 13,  4,  2,  9, 25, FLAG,
					35, 37, 30, 14,  5,  3, 10, 26, FLAG,
					36, 38, 31, 15,  6,  4, 11, 27, FLAG,
					37, 39,  7,  5, 12, 28, FLAG,
					38,  6, 13, 29, FLAG,

					41, 34, 18,  9, FLAG,
					40, 42, 35, 19, 10,  8, FLAG,
					41, 43, 36, 20, 11,  9, 16, 32, FLAG,
/* 4th rank */			42, 44, 37, 21, 12, 10, 17, 33, FLAG,
					43, 45, 38, 22, 13, 11, 18, 34, FLAG,
					44, 46, 39, 23, 14, 12, 19, 35, FLAG,
					45, 47, 15, 13, 20, 36, FLAG,
					46, 14, 21, 37, FLAG,


               		49, 42, 26, 17, FLAG,
					48, 50, 43, 27, 18, 16, FLAG,
					49, 51, 44, 28, 19, 17, 24, 40, FLAG,
/* 5th rank */			50, 52, 45, 29, 20, 18, 25, 41, FLAG,
					51, 53, 46, 30, 21, 19, 26, 42, FLAG,
					52, 54, 47, 31, 22, 20, 27, 43, FLAG,
					53, 55, 23, 21, 28, 44, FLAG,
					54, 22, 29, 45, FLAG,


					57, 50, 34, 25, FLAG,
					56, 58, 51, 35, 26, 24, FLAG,
					57, 59, 52, 36, 27, 25, 32, 48, FLAG,
/* 6th rank */			58, 60, 53, 37, 28, 26, 33, 49, FLAG,
					59, 61, 54, 38, 29, 27, 34, 50, FLAG,
					60, 62, 55, 39, 30, 28, 35, 51, FLAG,
					61, 63, 31, 29, 36, 52, FLAG,
					62, 30, 37, 53, FLAG,

					58, 42, 33, FLAG,
					59, 43, 34, 32, FLAG,
					56, 60, 44, 35, 33, 40, FLAG,
/* 7th rank */			57, 61, 45, 36, 34, 41, FLAG,
					58, 62, 46, 37, 35, 42, FLAG,
					59, 63, 47, 38, 36, 43, FLAG,
					60, 39, 37, 44, FLAG,
					38, 45, 61, FLAG,

					50, 41, FLAG,
					51, 42, 40, FLAG,
					52, 43, 41, 48, FLAG,
/* 8th rank. */		53, 44, 42, 49, FLAG,
					54, 45, 43, 50, FLAG,
					55, 46, 44, 51, FLAG,
					47, 45, 52, FLAG,
					46, 53, FLAG
					};

	textmode(C40);
	init(board.moves, mvarray, board.dir);
     clrscr();
	drawboard();
	drawticks();
	new(&board, col);
	board.depth = 0;
	board.hiswin = 1;	/* set for killer move heuristic */
	printstatus(&board, 0);
	for(;;)
	{	if (control[board.move & 1])
		{	tmvalue = time(&tmdummy);
			computermove(&board, &show, col);
			printstatus(&board, time(&tmdummy) - tmvalue);
			if (concheck(&board))
				printplaylist(&board);
		}
          tmvalue = time(&tmdummy);
		playermove(&board, &show, control, col);
		printstatus(&board, time(&tmdummy) - tmvalue);
		if (concheck(&board))
			printplaylist(&board);

	}
	/* because of anticipated view functions, update() shouldn't be done
		here */
}
