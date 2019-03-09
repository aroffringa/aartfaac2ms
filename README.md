## aartfaac2ms and tools

`aartfaac2ms` is a pipeline that converts Aartfaac raw datasets to the Casacore measurement set format. It
phase rotates the data to a common phase centre, and (optionally) flags, averaged and compresses the data.

`aartfaac2ms` reuses a lot of my code from the MWA's Cotter pipeline. Please cite [the Cotter paper](https://arxiv.org/abs/1501.03946) if you use this software.

afedit is a tool to splice a raw Aartfaac set based on LST.

Both tools were written by André Offringa, lastname@gmail.com, ASTRON.

## afedit usage

Run `afedit` without parameters to get the list of parameters. This is an example run:

    $ afedit -lst-start 6.9 -lst-end 7 A12_SB072_1ch1s_10min.raw.vis lst-selected.raw.vis
    LST range of observation: 06h50m13.252s - 07h00m17.876s (in hours: 6.83701 - 7.00497).
    Selected timesteps from LST interval: 225 - 582

This writes a new file 'lst-selected.raw.vis'. The lst start and end parameters are local apparent
sidereal times in hours. So 7.5 is 7:30 LST. One can also give only a start or end or end time, and then the other
end of the interval is left open (so -lst-start 7 means all timesteps after lst 7). '24h wrapping' is
supported, so doing this:

    $ afedit -lst-start 10 -lst-end 9 A12_SB072_1ch1s_10min.raw.vis test.raw.vis
    LST range of observation: 06h50m13.252s - 07h00m17.876s (in hours: 6.83701 - 7.00497).
    Selected timesteps from LST interval: 0 - 600

indeed selects everything (as it means start at LST 10h, and select until LST 9h the next day). There's also
a `-show-lst` parameter to see the LST range of the observation. For example, for the input it shows this:

    $ afedit -show-lst A12_SB072_1ch1s_10min.raw.vis
    LST range of observation: 06h50m13.252s - 07h00m17.876s (in hours: 6.83701 - 7.00497).

And running it for the sliced dataset:

    $ afedit -show-lst lst-selected.raw.vis
    LST range of observation: 06h54m00.365s - 06h59m59.707s (in hours: 6.9001 - 6.99992).
