# lavaGeckoCodeTranslator
A tool for annotating raw hex Gecko codes with descriptive comments, converting code into GCTRM compatible syntax where appropriate. Importantly, this program does not seek to change or edit codes in any way; only to make it more apparent what they're actually doing.
Additionally, translations aim to be fully compilable via GCTRM, and are constructed with the aim of compiling as accurately as possible to the originally provided hex, though some constructions and instructions may compile slightly differently (see "Important Notes, Warnings, and Shortcomings" section below).
Lastly, for a look at what translated code output looks like, see the "Examples" folder [here](https://github.com/QuickLava/lavaGeckoCodeTranslator/tree/master/lavaGeckoCodeTranslator/Examples).

## Usage for Converting Individual Codes
To translate a code, paste it into a plain text file (eg. .txt, .asm, etc.), and either drag the file onto the program executable, or pass its file path as a command line argument to the program. The resulting conversion will be placed in the same location as the source file, with the same name, suffixed with "\_conv". If you find that the program is failing to produce a translation, it may be that either the specified input file doesn't exist, or that attempting to write the output file failed for one reason or another.

## Usage for Converting Hex Code Within Existing Source Files
The same rules as above apply for translating any raw Gecko code within existing files; simply drag them onto the program executable, or pass their file paths as command line arguments to the program. The program will go through the passed in files one by one, translating any raw hex it can find, leaving any non-hex code lines (eg. comments, GCTRM syntax lines, etc.) unchanged, replicating them in the translated output as they appeared in the source file.

Please note, though, that this program was designed around converting pure hex Gecko code, not the combination of pure hex and GCTRM syntax code often found in more modern source code files. As such, while the program's output is often correct when operating on mixed files like these, converting the code in such files to hex (eg. by compiling the file with GCTRM, and translating the resulting "codeset.txt" file instead) before attempting to translate them may grant more consistently accurate results.

## Using Dolphin Map Files to Improve Output
This program's translation output can actually be augmented a bit if you can supply a Dolphin-compatible .map file for it to reference!
Currently, the primary benefit to doing this is that it allows the program to specify what function each translated HOOK type code actually hooks, though in the future this functionality may expand to provide further benefit.

To do this, simply copy the relevant game's .map file into the same folder as the program executable, and rename it to "symbols.map". Afterwards, for as long as the file is there, any translated output will use that file as reference.
Alternatively, you can drag a .map file onto the executable along with any files you wish to translate (or specify a .map file as a command line argument), which will have the program translate those files with the provided .map instead, ignoring the "symbols.map" file in the program directory if one exists.

## Important Notes, Warnings, and Shortcomings
### Translation Output for Embedded Data
Some codes, along with their actual Gecko code, additionally embed within themselves blocks of raw hex data. While this program *can* successfully identify some common means of embedding data in this way (eg. skipping over embedded data via Gecko Goto statements), it is sometimes the case that the program will fail to identify blocks like this, interpretting them instead as blocks of PPC or Gecko code.
If you encounter situations like these, do please send them my way, and I'll see what I can do to improve the program's embed detection to account for these misses. Where these embed blocks do appear, you'll find them labeled "DATA_EMBED", with a note specifying how many bytes long the embed itself is.

### Translation Output for Gecko RAM Writes
Currently, the program doesn't use GCTRM styled syntax for Gecko RAM Write codes, instead opting just to annotate them with comments explaining what they do.
The reason for this is that Gecko RAM Write codes don't actually specifiy their destinations as absolute addresses, but rather as offsets relative to the Gecko Codehandler's Base Address (BA) or Pointer Offset (PO) values.

As such, in order to provide the absolute write addresses GCTRM expects, this program needs to keep track of those values internally, accounting for any changes to them, as well as any situations which would result in those values being uncertain (eg. after loading them from RAM).
Due to some problem scenarios where keeping track of those values can be a bit complicated, I've opted to stick to just annotating these types of codes for the time being.

I hope to improve this aspect of the program at a later date, though when that'll actually happen is unclear currently (hence releasing now, without it).

### Hex vs. Character Output in Gecko String Writes
In string writes specifically, a string's contents are only output as ASCII characters if every byte within the output is either a printable ASCII character, or 0x00 (the string null-terminator). Otherwise, each byte will be output as a hex number, regardless of whether it's a printable character or not.

Additionally, in the case that the data *is* all printable characters, the program will wrap each string in quotes (dividing the data into separate strings at null-terminators as necessary), and will provide a note that the final string in the data isn't null-terminated if it isn't.
Lastly, the output will always note how many bytes long the data is, and (in the event that more than one null-terminated string is found) will specify how many individual strings are contained in the data as well. 

### Line-final Comments on Gecko Hex Code
Currently, if the program translates a line of Gecko code which ends with a comment, the translated output will overwrite that comment with its own annotation for the line. This may be fixed at some point in the future, though there are no concrete plans to tackle this at the time of writing.

## Final Notes
While I have of course done my best to ensure that this program's translations are as accurate as possible, I certainly cannot guarantee that it will never make any mistakes.
If you come across a circumstance where you *do* believe that the program's output is incorrect, please contact me via Discord (QuickLava#6688) and we'll see about getting any issues sorted out.
Additionally, if you have any suggestions regarding any aspect of this program (eg. regarding information provided in translations, formatting suggestions, etc.), or otherwise have feedback you'd like to share, again, please feel free to contact me and we'll talk about it. Hopefully this is helpful to some!
