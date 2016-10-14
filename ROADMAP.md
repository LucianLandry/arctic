## Near-term goals

Continue refactoring Engine interface (probably use boost::asio instead of sockets; make move-at-goaltime internal)
Get a graphical user interface working (based off Juce or Qt)

## Out there (maybe never.  Project velocity is very slow)

Build a custom FICS interface ala Zippy.
xpgn-format save/restore.
Adjustable strength.
More aggressive use of transposition table (better draw handling)
Support heterogenous time control sessions (example 40 minutes/40 moves followed
    by 30 seconds/move)
Introduce UCI/CECP backends, which would enable Polyglot-like functionality.
Try internal iterative deepening for move ordering and possible razoring.
Implement some kind of easy-move scheme.
Conversely, try allow more search time if first move's eval goes down.  Finish the level, or at least try to find a move that doesn't go down.  Especially if first move evals under the loss threshold.
More efficient smp search: keep all threads busy through end of minimax compute
    (including last move); propagate alpha increases to other threads?
Get cross-platform support going.
Support for other chess (or even non-chess) variants.  fischer-random, crazyhouse and Die come to mind.  Also consider the FICS wild variants + Xboard variants.
Adaptive back-propagating opening book?
multiple-computer/clustering search.
    We will probably need to use a custom protocol for this.  The closest
    "standard" way to do what I want would be UCI, + alpha/beta constraint
    option to "go", plus a way to easily unmake/make moves on an internal board.
Find a way to advance the pawn in the endgame w/out it getting crowded out by
    other moves?  Do I give pawn rank a bonus?  Or make it a preferred move?
uiXboard: implement analyze mode.
Go directly from pondering to thinking on a ponderhit, just like any good UCI engine would do.  Not sure this is worth the complexity, assuming one is using a transposition table.
Fractional ply extension. (+ 1/numMoves)
Shrink search window (not null necessarily, but do shrink it)
Better thread utilization: keep all threads busy through end of minimax compute
    (including last move).

## Ummmm sure

Neural net evaluation.  Examples: neurochess, SAL, morph.