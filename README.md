Alectardy98's fork of Jaga's Goomba Color fork

A fork of JaGoomba Color with the goal of allowing for proper saving and loading while running an overclocked GBA using a crystal oscillator mod.  Based on the 2019-05-04 source.

While using the EZ FLASH OMEGA DE, I found that while overclocked, save games would not correctly be read or loaded from SRAM, despite the SRAM injection being good. I determined that the way save games were compressed in Jagoomba color was causing issues while overclocking. In this modified fork, compression of save games has been removed, and now the RAW save game is used. This does in fact allow for overclocking and proper save loading, however, now there is not enough available space for most titles to have save states. This could be added in if bank-switching code was implemented to utilize the full capacity of SRAM on the EZ Flash Omega DE, but I currently have no plans to implement this. 

I also have not modified the code to require you to open up the emulator menu to inject the save file into SRAM. I honestly am not super sure how to allow for live save injection, and maybe this is something that I will look into in the future. 

Because save games are RAW, I am unsure if previous save games made with goomba will load properly with this modified code. You may want to start a new game. Please use at your own risk, and make backups of all your saved games before installing this.


THE REST OF THIS IF IS FROM JAGOOMBA COLOR:

Some notable hacks and games that have had issues fixed:
- Donkey Kong Land: New Colors Mode, https://www.romhacking.net/hacks/6076/ (file select menu accessible)
- Faceball 2000 (menu accessible)
- Kirby's Dream Land DX Service Repair, https://www.romhacking.net/hacks/6224/ (level 2 palette issues fixed)
- Konami GB Collections 2 and 4 (boots)
- Metal Gear Solid: Ghost Babel (elevator crash fixed)
- Pokemon Crystal (graphical corruption fixed)
- Wario Land DX, https://www.romhacking.net/hacks/6683/ (boots)

To build:
- Install the latest DevkitPro GBA tools
- Navigate Msys2 to this directory
- make
- Rename font.lz77.o to font.o and fontpal.bin.o to fontpal.o
- make

To test, I build a ROM with the resulting jagoombacolor.gba and the game I'm testing using goombafront.exe, then run it in mGBA.  You can find goombafront.exe as part of the Goomba Color releases.  For helpful debug symbols, take jagoombacolor.elf, put it in the same directory as the built ROM, and rename it to (ROM name).elf.  (Thanks to Endrift for the tip.)
Also included is a simple .bat file that will use gdb to dump debug symbols to a text file.

Thanks to:
- Dwedit for the Goomba Color emulator, which you can find at https://www.dwedit.org/gba/goombacolor.php.  If you'd like to incorporate my changes into Goomba Color, you're more than welcome to.
- FluBBa for the Goomba emulator before that: http://goomba.webpersona.com/
- Minucce for help with ASM and pointing me in the right direction.
- Sterophonick for code tweaks and featuring Jagoomba in the excellent Simple kernel for the EZ-Flash Omega carts: https://gbatemp.net/threads/new-theme-for-ez-flash-omega.520665/
- EZ-Flash for releasing the source to their modified Goomba Color builds, which hopefully allows this to support the Omega Definitive Edition's rumble features
- Nuvie for the code that saves the desired Game Boy type per game.
- Radimerry for the MGS:Ghost Babel elevator fix, Faceball menu fix, and SMLDX SRAM fix.
- Therealteamplayer for the default-to-grayscale code for GB games if no SGB palette is found.
