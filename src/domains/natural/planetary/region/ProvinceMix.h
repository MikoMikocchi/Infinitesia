#pragma once

namespace Infinitesia
{
// A mixed contact zone has rocks from both provinces. The primary province is
// the side of the contact, `secondaryWeight` says how much of the column is
// borrowed from the other side (0..0.5, 0.5 exactly on the contact). The
// decision is a small deterministic 3D noise, so the borrowed rock forms
// lenses and pods instead of a one-block speckle. The same function is used by
// the game and by the snapshot tools, so they agree.
bool UseSecondaryRock(double secondaryWeight, int eastMeters, int northMeters, int elevationMeters) noexcept;
} // namespace Infinitesia
