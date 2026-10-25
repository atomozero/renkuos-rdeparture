# R Departures

**English** | [日本語](README.ja.md) | [한국어](README.ko.md)

An airport departures board for Haiku and RenkuOS, full screen and without a
window border. Every character is a split flap: when something changes the
flaps rattle through other letters and settle, row after row, and the airline
column is one flap per airline in its colours.

![Narita, in Japanese](docs/narita.png)

## Install with pkgman

| Haiku | Add the repository |
| --- | --- |
| 32-bit x86 (x86_gcc2) | `pkgman add-repo https://pkgman.rainygirl.com/x86_gcc2` |
| x86_64 | `pkgman add-repo https://pkgman.rainygirl.com/x86_64` |
| arm64 | `pkgman add-repo http://pkgman.rainygirl.com/arm64` |

```sh
pkgman install rdepartures
```

Then start **R Departures** from **Deskbar -> Applications**. The package also
installs the flap font, TeX Gyre Heros Condensed Bold.

If `pkgman add-repo` fails with `Operation not supported`, the network kit of
that image has no TLS; use `http://` instead of `https://`.

## Airports

Seoul Incheon (ICN), Tokyo Narita (NRT), Hong Kong (HKG), London Heathrow
(LHR), Paris Charles de Gaulle (CDG), Milan Malpensa (MXP), New York JFK (JFK) and San Francisco
(SFO). Pick one with the gear in the top right corner; the board remembers it.

Each board shows its own local time, summer time included, and switches
language every eight seconds:

1. the language of the airport's country,
2. English, where that is not English already,
3. each destination in its own language -- Seoul as 서울, Taipei as 臺北,
   Athens as ΑΘΗΝΑ -- with its remarks in that language where the board has
   the words.

![Narita, each destination in its own language](docs/narita-destinations.png)
![Hong Kong](docs/hongkong.png)
![Paris](docs/paris.png)

The flights are real routes with plausible flight numbers, gates and times;
the timetable is generated, the same every day, with a few delays and
cancellations decided by the date. It is not live data. Remarks follow the
clock: check-in, gate open, boarding, final call, gate closed, departed.

![Flaps changing](docs/flipping.png)

## Keys

| Key | |
| --- | --- |
| Esc, Q | quit |
| Right arrow, space | next airport |
| Left arrow | previous airport |

The mouse pointer hides after three seconds.

## Build from source

On Haiku:

```sh
make install
```

builds `RDepartures` and copies it to `~/config/non-packaged/apps/R Departures`,
with the board font (TeX Gyre Heros Condensed Bold) in
`~/config/non-packaged/data/fonts/otfonts`; the font is picked up after the
next restart. The code is C++98, so the default `g++` builds it on a 32-bit
x86 (gcc2) hybrid as well.

On a workstation with Docker, `tools/cross-build.sh all` cross-builds
`dist/x86/` (gcc2), `dist/x86_64/` and `dist/arm64/`.

`RDepartures --snapshot board.png --size 1600x768 --airport CDG` renders the
board off-screen and saves it, for checking it without a screen. Quit a
running board first: a second launch is handed to it instead.

## Not affiliated

Airline names and colours are used to make the board look like one; no logos
are included, and nothing here is endorsed by any airline or airport.

## AI disclosure

R Departures was developed with the assistance of AI coding tools (Anthropic's
Claude).

## License

MIT for the code. `fonts/texgyreheroscn-bold.otf` (TeX Gyre Heros, by
B. Jackowski and J. M. Nowacki) is under the GUST Font License
(`fonts/GUST-FONT-LICENSE.txt`).
