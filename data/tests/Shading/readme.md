# Shading model tests

## NoShading.nandrad

Reference, all 4 windows and constructions are unshaded.

## ControlledShading.nandrad

- Window 1001 has constant shading factor, global radiation loads are reduced to 60% of nominal loads
- Windows 1002 and 1003 are intensity controlled, using a global horizontal sensor and the same control model
- Window 1004 is intensity controlled, using radiation onto the window itself as control value

This model has two shading control model instances.

## NoShadingButWithController.nandrad

This model has the same controllers as `ControlledShading.nandrad`, yet with a reduction factor of 1. This means,
there is no reduction effect.

## PrecomputedShading.nandrad

One window has a pre-computed shading applied.
