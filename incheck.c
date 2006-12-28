#include "ref.h"

int nopose(BoardT *board, int src, int dest, int hole)
/* checks to see if there are any occupied squares between src and dest.
   returns: 0 if blocked, 1 if nopose.  Note:  doesn't check if dir == DIRFLAG
   (none) or 8 (knight attack), so shouldn't be called in that case. */
{
    int dir = board->dir[src] [dest];
    uint8 *to = board->moves[src] [dir];
    while (*to != dest)
    {
	if (board->coord[*to] && *to != hole)
	    /* hole is used to extend attacks along checking ray in
	       attacked().  In this case it's our friendly kcoord.
	       Usually, it should be FLAG. */
	    return 0;	/* some sq on the way to dest is occ'd. */
	to++;
    }
    return 1;	/* notice we always hit dest before we hit end o' list. */
}


#if 0 /* bldbg: not needed, is it old code? */
int discheck(BoardT *board, int src, int turn, struct srclist *poplist)
/* Sees if turn's move from src puts !turn in check.
   returns:  the coord of the checking piece (FLAG if none).
   It is assumed that the moving piece is not moving
   on the checking ray, but that it originated from a checking ray.
   poplist is the possible pin list; that is, list of all sliding pieces
   on checking rays of the enemy king. */
{
    int i;
    int ekcoord = board->playlist['K' | (!turn << 5)].list[0];
    int dir = board->dir[ekcoord] [src];
    if (poplist->lgh > 0 && nopose(board, ekcoord, src, FLAG))
	/* have pos pins, and path so far is free */
    {
	for (i = 0; i < poplist->lgh; i++)
	    if (board->dir[src] [poplist->list[i]] == dir &&
		nopose(board, src, poplist->list[i], FLAG))
		return poplist->list[i]; /* won't find any more. */
    }
    return FLAG;
}
#endif
