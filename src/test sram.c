#include "includes.h"

static const int savestate_size_estimate = 0xC800;
void cleanup_ewram();
int FindStateByIndex(int index, int type, stateheader **stateptr);

extern int SaveState(u8 *dest);
extern int LoadState(u8 *source, int maxLength);

int loadstate2(int romNumber, stateheader *sh);
int savestate2(void);

#if !CARTSRAM
#if !MOVIEPLAYER
int get_saved_sram(void)
{
    return 0;
}
#endif
#else

// In your configuration file, ensure you have:
#define SRAM_SIZE 128
#define CARTSRAM 1
// and any other needed macros (e.g. USETRIM)

// --- Macro Definitions ---
#define SAVE_START_32K 0x6000
#define SAVE_START_64K 0xE000

#if SRAM_SIZE==32
    #define SAVE_START SAVE_START_32K
#else
    #define SAVE_START SAVE_START_64K
#endif

#if SRAM_SIZE == 128
    // For 128KB, reserve:
    // - First 64KB for battery (in-game) saves (XGB_SRAM)
    // - Second 64KB for emulator state saves & config (starting at SAVE_START)
    #undef SAVE_START
    #define SAVE_START    0x10000  // Emulator state area begins at 64KB offset.
    #define XGB_SRAM      MEM_SRAM  // Battery saves reside at physical SRAM start.
#else
    #if SRAM_SIZE==32
        #define SAVE_START 0x6000
    #else
        #define SAVE_START 0xE000
    #endif
    #define XGB_SRAM MEM_SRAM
#endif

// --- Global Variables ---
// Remove duplicate declarations.
EWRAM_BSS u32 sram_owner = 0;
EWRAM_BSS u8 *sram_copy = NULL;    // Now directly points to physical SRAM.
EWRAM_BSS int totalstatesize;      // Total bytes used for emulator state saves.
EWRAM_BSS u32 save_start;          // Starting offset of emulator state region.

EWRAM_BSS u8 *lzo_workspace = NULL;
EWRAM_BSS u8 *uncompressed_save = NULL;
EWRAM_BSS u8 *compressed_save = NULL;
EWRAM_BSS stateheader *current_save_file = NULL;
EWRAM_BSS bool doNotLoadSram;

// --- Some Constants ---
#define STATEID  0x57a731d8
#define STATEID2 0x57a731d9

#define STATESAVE  0
#define SRAMSAVE   1
#define CONFIGSAVE 2
#define MBC_SAV    2

/*
#if LITTLESOUNDDJ
u8 *M3_SRAM_BUFFER =(u8*)0x9FE0000;
void *M3_COMPRESS_BUFFER = (u32*)0x9FD0000;
#endif
*/


/*
extern u8 Image$$RO$$Limit;
extern u8 g_cartflags;	//(from GB header)
extern int bcolor;		//Border Color
extern int palettebank;	//Palette for DMG games
extern u8 gammavalue;	//from lcd.s
//extern u8 gbadetect;	//from gb-z80.s
extern u8 stime;		//from ui.c
extern u8 autostate;	//from ui.c
extern u8 *textstart;	//from main.c

extern char pogoshell;	//main.c

//-------------------
u8 *findrom(int);
void cls(int);		//main.c
void drawtext(int,char*,int);
void setdarkness(int dark);
void scrolll(int f);
void scrollr(void);
void waitframe(void);
u32 getmenuinput(int);
void writeconfig(void);
void setup_sram_after_loadstate(void);
void no_sram_owner();
void register_sram_owner();

extern int roms;		//main.c
extern int selected;	//ui.c
extern char pogoshell_romname[32];	//main.c
//----asm stuff------
int savestate(void*);		//cart.s
void loadstate(int,void*);		//cart.s

extern u8 *romstart;	//from cart.s
extern u32 romnum;	//from cart.s
extern u32 frametotal;	//from gb-z80.s
//-------------------

typedef struct {
	u16 size;	//header+data
	u16 type;	//=STATESAVE or SRAMSAVE
	u32 uncompressed_size;
	u32 framecount;
	u32 checksum;
	char title[32];
} stateheader;

typedef struct {		//(modified stateheader)
	u16 size;
	u16 type;	//=CONFIGSAVE
	char bordercolor;
	char palettebank;
	char misc;
	char reserved3;
	u32 sram_checksum;	//checksum of rom using SRAM e000-ffff
	u32 zero;	//=0
	char reserved4[32];  //="CFG"
} configdata;
*/

/*
void bytecopy(u8 *dst,u8 *src,int count)
{
	do
	{
		*dst++ = *src++;
	} while (--count);
}
*/

/*
void debug_(u32 n,int line);
void errmsg(char *s) {
	int i;

	drawtext(32+9,s,0);
	for(i=30;i;--i)
		waitframe();
	drawtext(32+9,"                     ",0);
}*/

void flush_end_sram(void)
{
    u8* sram = MEM_SRAM;
    int i;
    int save_end = save_start + 0x2000;
    for (i = save_start; i < save_end; i++)
    {
        sram[i] = 0;
    }
}




void probe_sram_size(void) {
    #if SRAM_SIZE == 128
        // With 128KB, force the state region to start at 0x10000.
        save_start = SAVE_START;
    #else
        // Original probing code for 32KB/64KB:
        vu8* sram = MEM_SRAM;
        vu8* sram2 = MEM_SRAM + 0x8000;
        u32 val1 = sram[0] | (sram[1] << 8) | (sram[2] << 16) | (sram[3] << 24);
        u32 val2 = sram2[0] | (sram2[1] << 8) | (sram2[2] << 16) | (sram2[3] << 24);
        if (val2 == val1) {
            if (val1 == STATEID || val1 == STATEID2) {
                sram[0] = (val1 ^ (STATEID ^ STATEID2)) & 0xFF;
                u32 newval2 = sram2[0] | (sram2[1] << 8) | (sram2[2] << 16) | (sram2[3] << 24);
                if (newval2 != val2)
                    save_start = 0x6000;
                else
                    save_start = 0xE000;
                sram[0] = STATEID & 0xFF;
            } else {
                sram[0] ^= 0xFF;
                if (sram2[0] == sram[0])
                    save_start = 0x6000;
                else
                    save_start = 0xE000;
                sram[0] ^= 0xFF;
            }
        } else {
            save_start = 0xE000;
        }
    #endif
}


/*
void flush_xgb_sram()
{
	u8* sram=(u8*)XGB_SRAM;
	int i;
	for (i=0x0;i<0x8000;i++)
	{
		sram[i]=0;
	}
}
*/



void getsram(void) {
    // Directly point to physical SRAM.
    sram_copy = MEM_SRAM;
}

#if USETRIM
//quick & dirty rom checksum
u32 checksum_this()
{
	u8 *p = romstart;
	u32 sum=0;
	int i;
//	u32 addthis;
	
	u8* end = (u8*)INSTANT_PAGES[1];
	
	u8 endchar = end[-1];
	for (i = 0; i < 128; i++)
	{
		if (p < end)
		{
			sum += *p | (*(p + 1) << 8) | (*(p + 2) << 16) | (*(p + 3) << 24);
		}
		else
		{
			sum += endchar | (endchar << 8) | (endchar << 16) | (endchar << 24);
		}
		p += 128;
	}
	return sum;
}

u32 checksum_mem(u8 *p)
{
	u32 sum=0;
	int i;
	for (i = 0; i < 128; i++)
	{
		sum += *p | (*(p + 1) << 8) | (*(p + 2) << 16) | (*(p + 3) << 24);
		p += 128;
	}
	return sum;
}

u32 checksum_romnum(int romNumber)
{
	u8 *romBase = findrom2(romNumber);
	u32 *rom32 = (u32*)romBase;
	if (*rom32 == TRIM)
	{
		u8 *page0_start = romBase + rom32[2];
		u8 *page0_end = romBase + rom32[3];
		u8 *p = page0_start;

		u32 sum=0;
		int i;
		
		u8 endchar=page0_end[-1];
		for (i = 0; i < 128; i++)
		{
			if (p < page0_end)
			{
				sum += *p | (*(p + 1) << 8) | (*(p + 2) << 16) | (*(p + 3) << 24);
			}
			else
			{
				sum += endchar | (endchar << 8) | (endchar << 16) | (endchar << 24);
			}
			p+=128;
		}
		return sum;
	}
	else
	{
		return checksum_mem(romBase);
	}
}

#else
//quick & dirty rom checksum
u32 checksum(u8 *p) {
	u32 sum=0;
	int i;
	for(i=0;i<128;i++) {
		sum+=*p|(*(p+1)<<8)|(*(p+2)<<16)|(*(p+3)<<24);
		p+=128;
	}
	return sum;
}
#endif

void writeerror(void) {
    int i;
    cls_secondary();
    drawtext_secondary(9, "  Write error! Memory full.", 0);
    drawtext_secondary(10, "     Delete some saves.", 0);
    scrolll(0);
    for (i = 90; i; --i)
        waitframe();
    scrollr(0);
}


/*
void memset8(u8 *p, int value, int size)
{
	while (size > 0)
	{
		*p++ = (u8)value;
		size--;
	}
}
*/

//(sram_copy=copy of GBA SRAM, current_save_file=new data)
//overwrite:  index=state#, erase=0
//new:  index=big number (anything >=total saves), erase=0
//erase:  index=state#, erase=1
//returns TRUE if successful
//IMPORTANT!!! totalstatesize is assumed to be current
//need to check this
int updatestates(int index, int erase, int type)
{
    if (sram_copy == NULL)
        getsram();
    
    stateheader *newdata = current_save_file;
    stateheader *foundState;
    int stateFound = FindStateByIndex(index, type, &foundState);
    int oldSize = 0;
    int newSize = (erase || newdata == NULL) ? 0 : newdata->size;
    
    // The emulator state area is assumed to begin at sram_copy+4 and span 'save_start' bytes.
    u8 *saveEnd = sram_copy + 4 + totalstatesize;
    u8 *dest = stateFound ? (u8*)foundState : saveEnd;
    if (stateFound)
        oldSize = foundState->size;
    
    u8 *newSaveEnd = saveEnd + newSize - oldSize;
    
    if ((newSaveEnd - sram_copy) + 8 > save_start)
        return 0;
    
    u8 *src = dest + oldSize;
    int copyCount = saveEnd - src;
    if (copyCount > 0 && (dest + newSize != src))
        memmove(dest + newSize, src, copyCount);
    
    if (newSize > 0)
        memcpy32(dest, (void*)newdata, newSize);
    
    {
        u32 *terminator = (u32*)newSaveEnd;
        terminator[0] = 0;
        terminator[1] = 0xFFFFFFFF;
    }
    
    totalstatesize = newSaveEnd - sram_copy;
    memset8(MEM_SRAM + totalstatesize + 8, 0, save_start - (totalstatesize + 8));
    return 1;
}
	
	u8 *src = dest + oldSize;
	u8 *copySrc = src;
	u8 *copyDest = dest + newSize;
	int copyCount = saveEnd - src;
	if (copyCount > 0 && copyDest != copySrc)
	{
		memmove(copyDest, copySrc, copyCount);
	}
	if (newSize > 0)
	{
		memcpy32(dest, (void*)newdata, newSize);
	}
	
	u32 *terminator = (u32*)newSaveEnd;
	terminator[0] = 0;
	terminator[1] = 0xFFFFFFFF;
	
	totalstatesize = newSaveEnd - sram_copy;
	bytecopy(MEM_SRAM, sram_copy, totalstatesize + 8);
	memset8(MEM_SRAM + totalstatesize + 8, 0, save_start - (totalstatesize + 8));
	return 1;
}

//more dumb stuff so we don't waste space by using sprintf
int twodigits(int n,char *s) {
	int mod=n%10;
	n=n/10;
	*(s++)=(n+'0');
	*s=(mod+'0');
	return n;
}

char *number_at(char *dest, unsigned int n)
{
	unsigned int n2=n;
	int digits=0;
	char *retval;
	do
	{
		n2/=10;
		digits++;
	} while (n2);
	dest+=digits;
	retval=dest;
	*(dest--)='\0';
	do
	{
		*(dest--)=(n%10)+'0';
		n/=10;
	} while (n);
	return retval;
}

void number_cat(char *str, unsigned int n)
{
	number_at(str + strlen(str), n);
}


void getstatetimeandsize(char *s,int time,u32 size,u32 freespace)
{
#if 0
	strcpy(s,"00:00:00 - 00/00k");
	twodigits(time/216000,s);
	s+=3;
	twodigits((time/3600)%60,s);
	s+=3;
	twodigits((time/60)%60,s);
	s+=5;
	twodigits(size/1024,s);
	s+=3;
	twodigits(totalsize/1024,s);
#else
	/////////012345678901234567890123456789
	//        12:34:56 - 65535, free 65535
	strcpy(s,"00:00:00 - ");
	
	twodigits(time/216000,s);
	s+=3;
	twodigits((time/3600)%60,s);
	s+=3;
	twodigits((time/60)%60,s);
	s+=5;
	s=number_at(s,size);
	strcat(s,", free ");
	s=number_at(s+7,freespace);
#endif
}

#define LOADMENU 0
#define SAVEMENU 1
#define SRAMMENU 2
#define DELETEMENU 3
#define FIRSTLINE 2
#define LASTLINE 16

//sram_copy holds copy of SRAM
//draw save/loadstate menu and update global totalstatesize
//returns a pointer to current selected state
//update *states on exit
stateheader* drawstates(int menutype,int *menuitems,int *menuoffset, int needed_size)
{
	if (sram_copy == NULL)
	{
		getsram();
	}
	
	int type;
	int offset=*menuoffset;
	int sel=selected;
	int startline;
	int size;
	int statecount;
	int total;
	int freespace;
	char *s=str;
	//char s[30];
	stateheader *selectedstate;
	int time;
	int selectedstatesize;
	stateheader *sh=(stateheader*)(sram_copy + 4);

	type=(menutype==SRAMMENU)?SRAMSAVE:STATESAVE;

	statecount=*menuitems;
	if(sel-offset>LASTLINE-FIRSTLINE-3 && statecount>LASTLINE-FIRSTLINE+1) {		//scroll down
		offset=sel-(LASTLINE-FIRSTLINE-3);
		if(offset>statecount-(LASTLINE-FIRSTLINE+1))	//hit bottom
			offset=statecount-(LASTLINE-FIRSTLINE+1);
	}
	if(sel-offset<3) {				//scroll up
		offset=sel-3;
		if(offset<0)					//hit top
			offset=0;
	}
	*menuoffset=offset;
	
	startline=FIRSTLINE-offset;
	cls(2);
	statecount=0;
	total=8;	//header+null terminator
	while(sh->size) {
		size=sh->size;
		if(sh->type==type || (menutype==DELETEMENU && sh->type!=CONFIGSAVE)  ) {
			if(startline+statecount>=FIRSTLINE && startline+statecount<=LASTLINE) {
				drawtext(32+startline+statecount,sh->title,sel==statecount);
			}
			if(sel==statecount) {		//keep info for selected state
				time=sh->framecount;
				selectedstatesize=size;
				selectedstate=sh;
			}
			statecount++;
		}
		total+=size;
		sh=(stateheader*)((u8*)sh+size);
	}

	freespace=save_start-total;

	if(sel!=statecount) {//not <NEW>
		getstatetimeandsize(s,time,selectedstatesize,freespace);
		drawtext(32+18,s,0);
	}
	else
	{
		//show data for the state to be created
		sh=current_save_file;
		if (sh != NULL)
		{
			time=sh->framecount;
			selectedstatesize=sh->size;

			getstatetimeandsize(s,time,selectedstatesize,freespace);
			drawtext(32+18,s,0);
		}
	}
	
	if(statecount)
		drawtext(32+19,"Push SELECT to delete",0);
	if(menutype==SAVEMENU) {
		if(startline+statecount<=LASTLINE)
			drawtext(32+startline+statecount,"<NEW>",sel==statecount);
		drawtext(32,"Save state:",0);
		statecount++;	//include <NEW> as a menuitem
	} else if(menutype==LOADMENU) {
		drawtext(32,"Load state:",0);
	} else if(menutype==SRAMMENU) {
		drawtext(32,"Erase SRAM:",0);
	} else if(menutype==DELETEMENU) {
		int freethis=needed_size-freespace;
		if (freethis>0)
		{
			strcpy(str,"Please free up ");
			number_cat(str, freethis);
			strcat(str," bytes");
			drawtext(32,str,0);
		}
		else
		{             //012345678901234567890123456789
			drawtext(32,"We now have enough space",0);
		}
	}
	*menuitems=statecount;
	totalstatesize=total;
	return selectedstate;
}

// [RAW SAVE MODIFICATION]
// Instead of compressing, we copy the data raw.
// This is the only definition of compressstate compiled.
void compressstate(lzo_uint size, u16 type, const u8 *src, u8 *dest, void *workspace)
{
    (void)workspace;  // Unused
    lzo_uint finalSize;
    stateheader *sh = (stateheader*)dest;
    
    memcpy(dest + sizeof(stateheader), src, size);  // Raw copy without compression
    finalSize = size;  // Raw size is the same as original

    // Setup header with raw data info
    sh->size = (finalSize + sizeof(stateheader) + 3) & ~3;  // Total size, word-aligned
    sh->type = type;
    sh->uncompressed_size = size;
    sh->framecount = frametotal;
    sh->checksum = checksum_romnum(romnum);
#if POGOSHELL
    if(pogoshell)
    {
        strcpy(sh->title, pogoshell_romname);
    }
    else
#endif
    {
        strncpy(sh->title, (char*)findrom(romnum)+0x134, 15);
    }
    cleanup_ewram();
}

void managesram() {
//need to check this
	int i;
	int menuitems;
	int offset=0;

	getsram();

	selected=0;
	drawstates(SRAMMENU,&menuitems,&offset,0);
	if(!menuitems)
		return;		//nothing to do!

	scrolll(0);
	do {
		i=getmenuinput(menuitems);
		if(i&SELECT) {
			updatestates(selected,1,SRAMSAVE);
			if(selected==menuitems-1) selected--;	//deleted last entry.. move up one
		}
		if(i&(SELECT+UP+DOWN+LEFT+RIGHT))
			drawstates(SRAMMENU,&menuitems,&offset,0);
	} while(menuitems && !(i&(L_BTN+R_BTN+B_BTN)));
	drawui1();
	scrollr(0);
}

void deletemenu(int statesize)
{
	int old_ui_x = ui_x;
	ui_x = 0;
	move_ui();
	
	int i;
	int menuitems;
	int offset=0;

	getsram();

	
	selected=0;
	drawstates(DELETEMENU,&menuitems,&offset,statesize);
	if (!menuitems)
	{
		return;
	}
	scrolll(0);
	do {
		i=getmenuinput(menuitems);
		if(i&SELECT)
		{
			updatestates(selected,1,-1);
			if (selected==menuitems-1) selected--;
		}
		if(i&(SELECT+UP+DOWN+LEFT+RIGHT))
			drawstates(DELETEMENU,&menuitems,&offset,statesize);
	} while(!(i&(L_BTN+R_BTN+B_BTN)));
	getsram();
	
	scrollr(0);
	ui_x = old_ui_x;
	move_ui();
}



void savestatemenu() {
//need to check this
	int i;
	int menuitems;
	int offset=0;
	
	SAVE_FORBIDDEN;

	i = savestate2();
	if (i <= 0 || i >= 57344 - 64)
	{
		writeerror();
		return;
	}
	//compressstate(i,STATESAVE,buffer2,buffer1);

	getsram();

	selected=0;
	drawstates(SAVEMENU,&menuitems,&offset,0);
	scrolll(0);
	do {
		i=getmenuinput(menuitems);
		if(i&(A_BTN)) {
			if(!updatestates(selected,0,STATESAVE))
				writeerror();
		}
		if(i&SELECT)
			updatestates(selected,1,STATESAVE);
		if(i&(SELECT+UP+DOWN+LEFT+RIGHT))
			drawstates(SAVEMENU,&menuitems,&offset,0);
	} while(!(i&(L_BTN+R_BTN+A_BTN+B_BTN)));
	drawui1();
	scrollr(0);
}

int FindStateByIndex(int index, int type, stateheader **stateptr)
{
	getsram();
	stateheader *sh = (stateheader*)(sram_copy+4);
	int size = sh->size;
	int i = 0;
	int total = 0;
	int foundstate = 0;
	while (size != 0)
	{
		if (sh->type == type)
		{
			if (index == i)
			{
				*stateptr = sh;
				foundstate = 1;
			}
			i++;
		}
		total += size;
		sh = (stateheader*)(((u8*)sh) + size);
		size = sh->size;
	}
	totalstatesize = total;
	return foundstate;
}


//locate last save by checksum
//returns save index (-1 if not found) and updates stateptr
//updates totalstatesize (so quicksave can use updatestates)
int findstate(u32 checksum,int type,stateheader **stateptr)
{
	if (sram_copy == NULL)
	{
		getsram();
	}
	
//need to check this
	int state,size,foundstate,total;
	stateheader *sh;

	getsram();
	sh=(stateheader*)(sram_copy+4);

	state=-1;
	foundstate=-1;
	total=8;
	size=sh->size;
	while(size) {
		if(sh->type==type) {
			state++;
			if(sh->checksum==checksum) {
				foundstate=state;
				if (stateptr != NULL)
				{
					*stateptr=sh;
				}
			}
		}
		total+=size;
		sh=(stateheader*)(((u8*)sh)+size);
		size=sh->size;
	}
	totalstatesize=total;
	return foundstate;
}

/*
void uncompressstate(int rom,stateheader *sh) {
//need to check this
	lzo_uint statesize=sh->size-sizeof(stateheader);
	lzo1x_decompress((u8*)(sh+1),statesize,buffer2,&statesize,NULL);
	loadstate(rom,buffer2);
	frametotal=sh->framecount;		//restore global frame counter
	setup_sram_after_loadstate();		//handle sram packing
}
*/

int using_flashcart() {
#if MOVIEPLAYER
	if (usingcache)
	{
		return 0;
	}
#endif

	return (u32)textstart&0x8000000;
}

void quickload() {
	stateheader *sh;
	int i;
	
	SAVE_FORBIDDEN;

	if(!using_flashcart())
		return;

	i=findstate(checksum_this(),STATESAVE,&sh);
	if(i>=0)
		loadstate2(romnum,sh);
}

void quicksave() {
	stateheader *sh;
	int i;
	
	SAVE_FORBIDDEN;

	if(!using_flashcart())
		return;

	ui_y=0;
	ui_x=256;
	cls(3);
	make_ui_visible();
	move_ui();
	//setdarkness(7);	//darken
	drawtext_secondary(9,"           Saving...",0);
	scrolll(1);
	
	i=savestate2();
	if (i == 0 || i >= 57344 - 64)
	{
		writeerror();
		scrollr(2);
		//cls(2);
		//setdarkness(0);	//darken
		return;
	}


	//compressstate(i,STATESAVE,buffer2,buffer1);
	i=findstate(checksum_this(),STATESAVE,&sh);
	if(i<0) i=65536;	//make new save if one doesn't exist
	if(!updatestates(i,0,STATESAVE))
	{
		writeerror();
	}
	scrollr(2);
	cls(3);
}

// In functions like backup_gb_sram() and save_new_sram(), the calls to compressstate now
// use our raw version. (See changes in backup_gb_sram() and save_new_sram() below.)

// [RAW SAVE MODIFICATION in backup_gb_sram()]
int backup_gb_sram(int called_from)
{
    if (sram_copy == NULL)
        getsram();
    
    int already_tried_to_save = 0;
restart:
    {
        int i = 0;
        configdata *cfg;
        stateheader *sh;
        u32 chk = 0;
        if (called_from == 1 && romstart != NULL)
            chk = checksum_this();
        
        if (!using_flashcart())
            return 1;
        
        lzo_workspace = sram_copy + 0xE000;
        compressed_save = lzo_workspace + 0x10000;
        current_save_file = (stateheader*)compressed_save;
        
        #if SRAM_SIZE == 128
            i = findstate(chk, SRAMSAVE, &sh);
            if (i >= 0) {
                memcpy(compressed_save, sh, sizeof(stateheader));
                // Copy 64KB from the battery save region.
                memcpy(compressed_save + sizeof(stateheader), XGB_SRAM, 0x10000);
                sh = current_save_file;
                sh->size = (0x10000 + sizeof(stateheader) + 3) & ~3;
                sh->checksum = chk;
                sh->uncompressed_size = 0x10000;
                int success = updatestates(i, 0, SRAMSAVE);
                cleanup_ewram();
                if (!success) {
                    writeerror();
                    if (!already_tried_to_save) {
                        already_tried_to_save = 1;
                        deletemenu(sh->size);
                        goto restart;
                    }
                    compressed_save = NULL;
                    current_save_file = NULL;
                    return 0;
                }
            }
            compressed_save = NULL;
            current_save_file = NULL;
            return 1;
        #else
            // Original handling for other SRAM sizes...
            return 0; // Placeholder.
        #endif
    }
}

//make new saved sram (using XGB_SRAM contents)
//this is to ensure that we have all info for this rom and can save it even after this rom is removed
// [RAW SAVE MODIFICATION]
// Modified save_new_sram to use our raw compressstate
int save_new_sram(u8 *SRAM_SOURCE)
{
    int sramsize = 0;
    #if SRAM_SIZE == 128
        sramsize = 0x10000; // 64KB battery save area.
    #elif (g_sramsize == 1)
        sramsize = 0x2000;
    #elif (g_sramsize == 2)
        sramsize = 0x2000;
    #elif (g_sramsize == 3)
        sramsize = 0x8000;
    #elif (g_sramsize == 4)
        sramsize = 0x8000;
    #elif (g_sramsize == 5)
        sramsize = 512;
    #endif

    if (SRAM_SOURCE != XGB_SRAM) {
        breakpoint();
        sramsize = 0x2000;
    }
    if (sram_copy == NULL)
        getsram();
    
    lzo_workspace = sram_copy + 0xE000;
    compressed_save = lzo_workspace + 0x10000;
    current_save_file = (stateheader*) compressed_save;
    
    compressstate(sramsize, SRAMSAVE, SRAM_SOURCE, compressed_save, lzo_workspace);
    int result = updatestates(65536, 0, SRAMSAVE);
    
    lzo_workspace = NULL;
    compressed_save = NULL;
    current_save_file = NULL;
    return result;
}
// [END RAW SAVE MODIFICATION]


int get_saved_sram(void)
{
	int i, j;
	int retval;
	u32 chk;
	configdata *cfg;
	stateheader *sh;
	lzo_uint statesize;

	if(!using_flashcart())
	{
		return 0;
	}
	if (doNotLoadSram)
	{
		return 0;
	}

	if(g_cartflags & MBC_SAV)
	{
		chk = checksum_this();
		i = findstate(0, CONFIGSAVE, (stateheader**)&cfg);
		j = findstate(chk, SRAMSAVE, &sh);
		
		if(j >= 0) {
			statesize = sh->size - sizeof(stateheader);
			// [RAW SAVE MODIFICATION]: Raw copy instead of decompression.
			memcpy(XGB_SRAM, (u8*)(sh+1), statesize);
			retval = 1;
		} else {
			save_new_sram(XGB_SRAM);
			retval = 2;
		}
		
		if (g_sramsize == 3 || g_sramsize == 4)
		{
			no_sram_owner();
		}
		else
		{
			bytecopy(MEM_SRAM + save_start, XGB_SRAM, 0x2000);
			register_sram_owner();
		}
		return retval;
	}
	else
	{
		return 0;
	}
}

void register_sram_owner()
{
	sram_owner = checksum_this();
	writeconfig();
}

void no_sram_owner()
{
	sram_owner = 0;
	writeconfig();
	flush_end_sram();
}

void setup_sram_after_loadstate() {
	int i;
	u32 chk;
	configdata *cfg;

	if(g_cartflags & MBC_SAV) {
		chk = checksum_this();
		i = findstate(0, CONFIGSAVE, (stateheader**)&cfg);
		if(i >= 0 && (chk != cfg->sram_checksum))
		{
			backup_gb_sram(0);
		}
		
		if (g_sramsize < 3)
		{
			bytecopy(MEM_SRAM + save_start, XGB_SRAM, 0x2000);
		}
		else
		{
			no_sram_owner();
		}
		i = findstate(chk, SRAMSAVE, (stateheader**)&cfg);
		if(i < 0)
			save_new_sram(XGB_SRAM);
		if (g_sramsize < 3)
		{
			register_sram_owner();
		}
		else
		{
			backup_gb_sram(1);
		}
	}
}

int find_rom_number_by_checksum(u32 sum)
{
	int i;
	for (i = 0; i < roms; i++)
	{
		if(sum == checksum_romnum(i))
			return i;
	}
	return -1;
}

void loadstatemenu() {
	stateheader *sh;
	u32 key;
	int i;
	int offset = 0;
	int menuitems;
	u32 sum;
	
	SAVE_FORBIDDEN;

	getsram();

	selected = 0;
	sh = drawstates(LOADMENU, &menuitems, &offset, 0);
	if(!menuitems)
		return;

	scrolll(0);
	do {
		key = getmenuinput(menuitems);
		if(key & (A_BTN)) {
			sum = sh->checksum;
			i = 0;
			do {
				if(sum == checksum_romnum(i)) {
					loadstate2(i, sh);
					i = 8192;
				}
				i++;
			} while(i < roms);
			if(i < 8192) {
				cls(2);
				drawtext(32+9, "       ROM not found.", 0);
				for(i = 0; i < 60; i++)
					waitframe();
			}
		} else if(key & SELECT) {
			updatestates(selected, 1, STATESAVE);
			if(selected == menuitems - 1) selected--;
		}
		if(key & (SELECT+UP+DOWN+LEFT+RIGHT))
			sh = drawstates(LOADMENU, &menuitems, &offset, 0);
	} while(menuitems && !(key & (L_BTN+R_BTN+A_BTN+B_BTN)));
	drawui1();
	scrollr(0);
}

const configdata configtemplate = {
	sizeof(configdata),
	CONFIGSAVE,
	0,0,0,0,0,0,
	"CFG"
};

void writeconfig() {
    if (sram_copy == NULL)
        getsram();
    
    configdata *cfg = NULL;
    if (!using_flashcart())
        return;
    
    compressed_save = sram_copy + 0xE000;
    current_save_file = (stateheader*)compressed_save;
    
    int i = findstate(0, CONFIGSAVE, (stateheader**)&cfg);
    if (i < 0) {
        memcpy(compressed_save, &configtemplate, sizeof(configdata));
        cfg = (configdata*)current_save_file;
    }
    
    cfg->palettebank = palettebank;
    int j = (stime & 0x3) | ((request_gb_type & 0x3) << 2) |
            ((autostate & 0x1) << 4) | ((gammavalue & 0x7) << 5);
    cfg->misc = j;
    cfg->sram_checksum = sram_owner;
    
    if (i < 0)
        updatestates(0, 0, CONFIGSAVE);
    else
        bytecopy((u8*)cfg - sram_copy + MEM_SRAM, (u8*)cfg, sizeof(configdata));
    
    compressed_save = NULL;
    current_save_file = NULL;
}


void readconfig() {
	int i;
	configdata *cfg;
	if(!using_flashcart())
		return;

	i = findstate(0, CONFIGSAVE, (stateheader**)&cfg);
	if(i >= 0) {
		palettebank = cfg->palettebank;
		i = cfg->misc;
		stime = i & 0x3;
		request_gb_type = (i & 0x0C) >> 2;
		autostate = (i & 0x10) >> 4;
		gammavalue = (i & 0xE0) >> 5;
		sram_owner = cfg->sram_checksum;
	}
}

/* savestate2() and cleanup_ewram() remain unchanged. */

int savestate2()
{
	sram_copy = NULL;
	lzo_workspace = ewram_start;
	uncompressed_save = ewram_start + 0x10000;
	
	u8 *workspace = lzo_workspace;
	u8 *uncompressedState = uncompressed_save;
	
	int stateSize = SaveState(uncompressedState);
	if (stateSize == 0)
	{
		goto fail;
	}
	
	compressed_save = uncompressed_save + stateSize;
	current_save_file = (stateheader*)compressed_save;
	
	u8 *out = compressed_save + sizeof(stateheader) + 8;
	lzo_uint compressedSize1;
	lzo1x_1_compress(uncompressedState, stateSize, out, &compressedSize1, workspace);
	u32 part1_size = ((compressedSize1 - 1) | 3) + 1;
	
	*((u32*)(out - 4)) = part1_size;
	*((u32*)(out - 8)) = stateSize;
	
	int outSize = out + part1_size + 8 - compressed_save;
	
	memcpy32(uncompressed_save, compressed_save, outSize);
	out = out - (compressed_save - uncompressed_save);
	compressed_save = uncompressed_save;
	current_save_file = (stateheader*)compressed_save;
	uncompressed_save = NULL;
	
	u8 *out2 = out + part1_size + 8;
	
	int sramSize = 0;
	if (g_sramsize == 0)
	{
		sramSize = 0;
	}
	else if (g_sramsize == 3)
	{
		sramSize = 0x8000;
	}
	else
	{
		sramSize = 0x2000;
	}
	
	int total_size = part1_size + 8 + sizeof(stateheader);
	
	if (sramSize > 0)
	{
		int sramMaxSize = sramSize + sramSize / 16 + 67;
		int remainingSpace = 0x10000 - part1_size;
		lzo_uint compressedSize2;
		int part2_size;
		lzo1x_1_compress(XGB_SRAM, sramSize, out2, &compressedSize2, workspace);
		part2_size = ((compressedSize2 - 1) | 3) + 1;
		*((u32*)(out2 - 4)) = part2_size;
		*((u32*)(out2 - 8)) = sramSize;
		total_size += part2_size + 8;
	}
	
	stateheader* sh = current_save_file;
	sh->size = total_size;
	sh->type = STATESAVE;
	sh->uncompressed_size = stateSize + sramSize;
	sh->framecount = frametotal;
	sh->checksum = checksum_this();
#if POGOSHELL
    if(pogoshell)
    {
		strcpy(sh->title, pogoshell_romname);
    }
    else
#endif
    {
		strncpy(sh->title, (char*)findrom(romnum)+0x134, 15);
    }
	uncompressed_save = NULL;
	lzo_workspace = NULL;
	cleanup_ewram();
	return total_size;
fail:
	uncompressed_save = NULL;
	compressed_save = NULL;
	current_save_file = NULL;
	lzo_workspace = NULL;
	cleanup_ewram();
	return 0;
}

void cleanup_ewram() {
    if (ewram_canary_1 != 0xDEADBEEF) {
        breakpoint();
        extern u8 vram_packets_dirty[];
        extern u8 vram_packets_registered_bank0[];
        extern u8 vram_packets_registered_bank1[];
        extern u8 vram_packets_incoming[];
        extern u8 RECENT_TILENUM[];
        extern u8 dirty_map_words[];
        extern u8 DIRTY_TILE_BITS[];
        make_instant_pages(romstart);
        memset32(vram_packets_dirty, 0, 0xC4);
        memset32(vram_packets_registered_bank0, 0, 0xC0);
        memset32(vram_packets_registered_bank1, 0, 0xC0);
        memset32(vram_packets_incoming, 0, 0xC0);
        memset32(RECENT_TILENUM, 0, 0x80);
        memset32(dirty_map_words, -1, 0x40);
        memset32(DIRTY_TILE_BITS, -1, 0x30);
        ewram_canary_1 = 0xDEADBEEF;
    }
    if (ewram_canary_2 != 0xDEADBEEF) {
        breakpoint();
        extern u8 TEXTMEM[];
        memset32(TEXTMEM, 0x20202020, 0x278);
        ewram_canary_2 = 0xDEADBEEF;
    }
}

int loadstate2(int romNumber, stateheader *sh)
{
	if (sram_copy == NULL)
	{
		getsram();
	}
	
	if (romNumber != romnum)
	{
		doNotLoadSram = true;
		int old_auto_border = auto_border;
		auto_border = 0;
		loadcart(romNumber, g_emuflags);
		auto_border = old_auto_border;
		doNotLoadSram = false;
	}
	
	u8 *src = (u8*)(sh + 1);
	u32 *src32 = (u32*)src;
	u32 uncompressedStateSize = *src32++;
	u32 compressedStateSize = *src32++;
	src = (u8*)src32;
	
	u8 *src2 = src + compressedStateSize;
	u32 uncompressedSramSize = 0;
	u32 compressedSramSize = 0;
	
	if (g_sramsize == 0 && sh->size != compressedStateSize + 8 + sizeof(stateheader))
	{
		return 0;
	}
	
	if (g_sramsize != 0)
	{
		src32 = (u32*)src2;
		uncompressedSramSize = *src32++;
		compressedSramSize = *src32++;
		src2 = (u8*)src32;
		
		if (sh->size != compressedStateSize + compressedSramSize + 16 + sizeof(stateheader))
		{
			return 0;
		}
	}
	
	if (uncompressedStateSize > 0x10000 || 0 != (uncompressedStateSize & 3) ||
		compressedStateSize > 0x10000 || 0 != (compressedStateSize & 3) ||
		uncompressedSramSize > 0x8000 || 0 != (uncompressedSramSize & 3) ||
		compressedSramSize > 0x8900 || 0 != (compressedSramSize & 3) )
	{
		return 0;
	}
	
	uncompressed_save = sram_copy + 0xE000;
	
	// For the system state, still decompress as usual
	lzo_uint bytesDecompressed = compressedStateSize;
	lzo1x_decompress(src, compressedStateSize, uncompressed_save, &bytesDecompressed, NULL);
	
	// [RAW SAVE MODIFICATION]:
	// Instead of decompressing SRAM, do a raw copy.
	if (uncompressedSramSize > 0)
	{
		memcpy(XGB_SRAM, src2, uncompressedSramSize);
	}
	
	int result = LoadState(uncompressed_save, uncompressedStateSize);
	
	uncompressed_save = NULL;
	
	if (result != 0)
	{
		return 0;
	}
	
	frametotal = sh->framecount;
	setup_sram_after_loadstate();
	
	return sh->size;
}

#endif
