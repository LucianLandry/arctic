//--------------------------------------------------------------------------
//                   Eval.h - (position) evaluation class.
//                           -------------------
//  copyright            : (C) 2016 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#ifndef EVAL_H
#define EVAL_H

const int kMaxEvalStringLen = 26;

class Eval
{
public:
    // A 'royal' piece in this sense is any piece that loses the game if
    //  captured.  They are in effect invaluable.  Making this '0' lets us
    //  implement multiple royal pieces on one side w/out screwing up the
    //  evaluation.
    static const int Royal  = 0;
    static const int King   = Royal;
    static const int Pawn   = 100;
    static const int Bishop = 300;
    static const int Knight = 300;
    static const int Rook   = 500;
    static const int Queen  = 900;
    static const int Win    = 100000; // For chess, this is a checkmate.
    static const int Loss   = -Win;

    // For win/loss detection in x plies.  Here, x can be 100 plies.
    static const int WinThreshold  = Win - 100;
    static const int LossThreshold = -WinThreshold;

    // Constructors.
    Eval() = default; // FIXME: do I really want a non-initialized eval?
    Eval(const Eval &other) = default;
    explicit inline Eval(int exactVal);
    inline Eval(int lowBound, int highBound);

    Eval &operator=(const Eval &other) = default;

    // These operators are strict -- that is, if the answer is indeterminate,
    //  they will return false.  (If necessary, we could use a tribool here.)
    inline bool operator<(int val) const;
    inline bool operator<=(int val) const;
    inline bool operator>(int val) const;
    inline bool operator>=(int val) const;
    
    inline bool IsExactVal() const;
    // Returns the bounds difference.  An exact evaluation has a Range() of 0.
    inline int  Range() const;
    // Is this above the win threshold or below the loss threshold?
    inline bool DetectedWin() const;
    inline bool DetectedLoss() const;
    inline bool DetectedWinOrLoss() const;
    // Returns -1 if we are not in a win/lose situation.
    inline int  MovesToWinOrLoss() const;
    // Accessor.
    inline int LowBound() const;
    inline int HighBound() const;

    inline Eval Inverted() const;
    
    // These operations can be chained.
    inline Eval &Invert();
    inline Eval &Set(int lowBound, int highBound);
    inline Eval &Set(int exactVal);
    inline Eval &BumpTo(Eval other);
    inline Eval &BumpHighBoundTo(int highBound);
    inline Eval &BumpHighBoundToWin();

    // Iff the evaluation is above 'threshold', decrements it.  Iff the
    //  evaluation is below '-threshold', increments it.
    // This enables calculation of plies to win/loss by tweaking the eval.
    // Slightly hacky.  This is not the inverse of BumpTo()!
    inline Eval &DecayTo(int threshold);
    // This is sort of the opposite of 'DecayTo()'.
    inline Eval &RipenFrom(int threshold);
    
    // Writes to and returns 'result', which is suitable for logging, and
    //  which is assumed to be at least kMaxEvalStringLen chars long.
    // FIXME: most other classes use Log(); we should get our story straight.
    char *ToLogString(char *result) const;
private:
    int lowBound;
    int highBound;
};

// Convenience constant used for initialization.
const Eval EvalLoss(Eval::Loss, Eval::Loss);

inline Eval::Eval(int exactVal) : lowBound(exactVal), highBound(exactVal) {}

inline Eval::Eval(int lowBound, int highBound) :
            lowBound(lowBound), highBound(highBound) {}

inline bool Eval::operator<(int val) const
{
    return highBound < val;
}

inline bool Eval::operator<=(int val) const
{
    return highBound <= val;
}

inline bool Eval::operator>(int val) const
{
    return lowBound > val;
}

inline bool Eval::operator>=(int val) const
{
    return lowBound >= val;
}

inline bool Eval::IsExactVal() const
{
    return lowBound == highBound;
}

inline int Eval::Range() const
{
    return highBound - lowBound;
}

inline bool Eval::DetectedWin() const
{
    return lowBound >= Eval::WinThreshold;
}

inline bool Eval::DetectedLoss() const
{
    return highBound <= Eval::LossThreshold;
}

inline bool Eval::DetectedWinOrLoss() const
{
    return DetectedWin() || DetectedLoss();
}

inline int  Eval::MovesToWinOrLoss() const
{
    if (DetectedWin())
        return (Eval::Win - lowBound + 1) / 2;
    if (DetectedLoss())
        return (highBound - Eval::Loss + 1) / 2;
    return -1;
}

inline int Eval::LowBound() const
{
    return lowBound;
}

inline int Eval::HighBound() const
{
    return highBound;
}

inline Eval Eval::Inverted() const
{
    return Eval(-highBound, -lowBound);
}

inline Eval &Eval::Invert()
{
    int tmpVal = lowBound;
    lowBound = -highBound;
    highBound = -tmpVal;
    return *this;
}

inline Eval &Eval::Set(int lowBound, int highBound)
{
    this->lowBound = lowBound;
    this->highBound = highBound;
    return *this;
}

inline Eval &Eval::Set(int exactVal)
{
    this->lowBound = exactVal;
    this->highBound = exactVal;
    return *this;
}

inline Eval &Eval::BumpTo(Eval other)
{
    if (lowBound < other.lowBound)
        lowBound = other.lowBound;
    if (highBound < other.highBound)
        highBound = other.highBound;
    return *this;
}

inline Eval &Eval::BumpHighBoundTo(int highBound)
{
    if (this->highBound < highBound)
        this->highBound = highBound;
    return *this;
}

inline Eval &Eval::BumpHighBoundToWin()
{
    this->highBound = Eval::Win;
    return *this;
}

inline Eval &Eval::DecayTo(int threshold)
{
    if (lowBound > threshold)
        lowBound--;
    else if (lowBound < -threshold)
        lowBound++;

    // It makes a twisted kind of sense for the highBound to decay as well as
    //  the lowBound.
    if (highBound > threshold)
        highBound--;
    else if (highBound < -threshold)
        highBound++;

    return *this;
}

inline Eval &Eval::RipenFrom(int threshold)
{
    if (lowBound > threshold && lowBound < Eval::Win)
        lowBound++;
    else if (lowBound < -threshold && lowBound > Eval::Loss)
        lowBound--;

    if (highBound > threshold && highBound < Eval::Win)
        highBound++;
    else if (highBound < -threshold && highBound > Eval::Loss)
        highBound--;

    return *this;
}

#endif // EVAL_H
