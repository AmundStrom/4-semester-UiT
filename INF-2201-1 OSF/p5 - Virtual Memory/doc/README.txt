If swapping does not work, it means the swap-space overlaps with the rest of the image.

To fix this, there is 2 steps:

1. Go into createimage.c line 141, and increase the value, should originally be 320. (must be sector aligned!)

2. Go into memory.h line 52, and set it as the same value as the previous step.