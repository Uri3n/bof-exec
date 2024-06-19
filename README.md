# bof-exec
A small tool that loads and executes a Beacon Object File (BOF) and optionally passes arguments to it.

## How to use
Pass the path to the BOF as the first argument, and your arguments as the second (BOF arguments are optional, you may omit them).
Your provided arguments can be either strings (retrievable via BeaconDataExtract),
32-bit integers (retrievable via BeaconDataInt), or 16-bit integers (retrievable via BeaconDataShort).
Each argument should be comma delimited. For integer arguments, prefix them with either 'i' for an int32 or 's' for an int16 (short).
For example:

**bof-exec bof.o "string argument, i150, s50, i-13"**

This will pass arguments to the BOF's "go" function in the order you typed it. Notice how the last int has a '-' character
after the 'i'. This will pass the integer as a negative number to the BOF (although there's very little reason to do this).

![fdsf1231ss](https://github.com/Uri3n/bof-exec/assets/153572153/2f446ead-4dec-4519-b385-a0e7f3bb495c)
