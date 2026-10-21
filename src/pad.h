// Gamepad state used by the controls: read from GInput when it is loaded (the
// game's own pad layer on the Deck), from XInput otherwise.
#pragma once

namespace gmc {
namespace pad {

struct State {
    // Sticks in -255..255, positive right / up, dead zone already applied.
    int leftX;
    int leftY;
    int rightX;
    int rightY;
    // Analog triggers in 0..255.
    int l2;
    int r2;
    // Face buttons, PlayStation names (Deck: cross = A, square = X,
    // triangle = Y, circle = B).
    bool cross;
    bool square;
    bool triangle;
    bool circle;
    bool l1;
    bool r1;
    bool l3;
    bool r3;
    bool dpadUp;
    bool dpadDown;
    bool dpadLeft;
    bool dpadRight;
    bool start;
    bool select;
};

// Must not be called from DllMain (may load XInput). Returns false when no
// pad is available.
bool Read(State& out);

} // namespace pad
} // namespace gmc
