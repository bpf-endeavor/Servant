# List of things to do

## Missing Features

1. Get the size of yield chain from the Elf file (uBPF knows it) and remove --num-prog flag
2. uBPF does not suppor the ".rodata.str" it would be nice to have it
3. Enable programs to define the next program to run after they yield. Right now it moves in fixed stages.

## Performance Worries

1. We have an indirect call to the eBPF function. Use a static patching for it.
