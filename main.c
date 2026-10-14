/*
 * BLOXORZ for PSP  -  v3
 * 100 levels, split-block teleporters, switches & bridges, stone/rust
 * textures, gritty synthesized sound + ambient music, save progress.
 *
 * Build with PSPSDK (pspdev). Everything (graphics, sound) is generated in
 * code, so EBOOT.PBP is the only file needed.
 */

#include <pspkernel.h>
#include <psputils.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspaudio.h>
#include <psppower.h>
#include <pspge.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

PSP_MODULE_INFO("Bloxorz", PSP_MODULE_USER, 3, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU);

#define BUF_WIDTH  512
#define SCR_WIDTH  480
#define SCR_HEIGHT 272
#define FB1_OFFSET 0x88000
#define ZB_OFFSET  0x110000

#define MAXW 16
#define MAXH 12
#define PI_F 3.14159265f

#define DT          (1.0f / 60.0f)
#define ROLL_TIME   0.14f
#define FALL_TIME   0.90f
#define WIN_TIME    0.95f
#define REPEAT_WAIT 0.28f
#define BUFFER_TIME 0.30f

static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

/* ================================================================== */
/* Exit callback (HOME button)                                         */
/* ================================================================== */
static int exit_callback(int a1, int a2, void *c)
{
    sceKernelExitGame();
    return 0;
}

static int callback_thread(SceSize args, void *argp)
{
    int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

static void setup_callbacks(void)
{
    int thid = sceKernelCreateThread("update_thread", callback_thread, 0x11, 0xFA0, 0, 0);
    if (thid >= 0) sceKernelStartThread(thid, 0, 0);
}

/* ================================================================== */
/* Levels                                                              */
/*  # floor  S start  G goal hole  F fragile  . empty                  */
/*  1-4 bridge (colour group)                                          */
/*  switches (colour = position inside each group of 4 letters):       */
/*   a-d square toggle   e-h cross toggle   i-l square ON              */
/*   m-p square OFF      q-t cross ON       u-x cross OFF              */
/*  T teleport pad   U,V where the two cubes appear                    */
/* ================================================================== */
typedef struct { const char *name; int init; const char *hint; const char *rows[12]; } LevelDef;

static const LevelDef LEVELS[] = {
    { "PLAIN 1", 0, "ROLL THE BLOCK AND STAND IT UP ON THE HOLE", { "####.........","####.........","####.........","#S########G##","..........###", NULL } },
    { "PLAIN 2", 0, "", { "###..","##S..","###..",".####",".#...",".####",".####",".####","..##G", NULL } },
    { "PLAIN 3", 0, "", { ".......G##",".......###","##.#...###","S#######..","......##..","......##..","......##..","......##..","...#####..","...#####..", NULL } },
    { "PLAIN 4", 0, "", { "####..........","###S..........","############G#","####.####..###",".....#.#......",".....#.#......",".....#.#......",".....###......",".....###......",".....###......", NULL } },
    { "PLAIN 5", 0, "", { ".........G###","#############","S##......####","###......####","..#..........","..#..........","###..........","###..........","###..........", NULL } },
    { "PLAIN 6", 0, "", { "....####....","....####....","##..#######G","#S######..##","##..........", NULL } },
    { "PLAIN 7", 0, "", { "..##........","..##........","..##........","..##........","..##......##","..##......##","####......##","S##########G", NULL } },
    { "PLAIN 8", 0, "", { "......####...","......####...","##....####...","##....####...","#S....#.#....","##....#.#....",".#..###.#..##",".##########G#","....#####..##","....###......", NULL } },
    { "PLAIN 9", 0, "", { "....#G","#S.###","##..##",".#..#.",".#..#.",".#..#.",".####.",".####.",".####.",".####.", NULL } },
    { "PLAIN 10", 0, "", { "......####","#S#...####","##########",".#########",".##.....#.",".##.....#.",".##..###G.",".##..####.",".##..####.", NULL } },
    { "CRACKED 1", 0, "ORANGE TILES BREAK IF YOU STAND ON THEM", { ".....####.",".....#F##.","......##..","......###.","......#G##","......####","####..####","##S#####F#", NULL } },
    { "CRACKED 2", 0, "", { "......###.","....#.###.","#S###F###.","##F..##...","###..##...","###...#...","......#...","......#...","......###G","........##", NULL } },
    { "CRACKED 3", 0, "", { ".###........",".SF#........",".###.....##G",".#F#########",".#.#.....###",".#.F.....###","####........","####........","###.........","###.........", NULL } },
    { "CRACKED 4", 0, "", { "#S#.......","##F...###.","###...###.","F##F#F###.","#.....#...","####..#...","####..#...","......#...","......####","......###G","......####", NULL } },
    { "CRACKED 5", 0, "", { ".....##....",".....##....",".....##....",".....##....",".....##....",".....##....","####.##....","#S##.##G###","####F######","####..##F##", NULL } },
    { "CRACKED 6", 0, "", { "#####........","S###.........","...F.........","...#......#G#","...##.....###","...##.....###","...####F#####","......F......", NULL } },
    { "CRACKED 7", 0, "", { "......###.......","......###.......","......###.......","......#.........","......F.........","##SF#####.......","###..####...G###","..#######...####",".....#F###F####.","........#.......","........#.......", NULL } },
    { "CRACKED 8", 0, "", { "############..","####.....###..","S###.....###..","###F.......#..","..##.......F..","..##.......G#F","..###......###","..###......###", NULL } },
    { "CRACKED 9", 0, "", { "#S#F###.......","##..###.......","##..#.........","....F.......G#","....###.....##","....##F.....##","....#######F##",".....##.......", NULL } },
    { "CRACKED 10", 0, "", { "###...........","S##..#F#...##F","#####F####FG##","###..###......",".....F........",".....#........",".....#........",".....####.....",".....####.....", NULL } },
    { "SWITCH 1", 0, "STEP ON A SWITCH TO MAKE BRIDGES APPEAR", { ".....##........",".....##..##G...",".....#######...",".....#...######",".....1...###...",".....1.........","...####........","...####........","...####........","####aS#........", NULL } },
    { "SWITCH 2", 0, "", { ".......#G##",".##....####",".S####1####",".#.....####",".#.........","##.........","##.........","#a.........", NULL } },
    { "SWITCH 3", 0, "", { ".##.............",".S#...##........",".#....######1G##",".#....##.....###","####..##.....###","##a####F........","####............","####............", NULL } },
    { "SWITCH 4", 0, "", { "..#.........","..#.........","#S##........","############","......#####a","........####",".........#..",".........1..",".......##1..",".......##1..",".......#G#..", NULL } },
    { "SWITCH 5", 0, "", { "#a#..........","###..........","##S..........",".#...........",".#...........",".#.###.......",".#.###.......",".#.###.......",".#####11#####","..#......G###", NULL } },
    { "SWITCH 6", 0, "", { ".11########.....",".#...########G##","###..........###","###.............","#a#.............","##S.............", NULL } },
    { "SWITCH 7", 0, "", { ".......###.....",".......F##.....",".......###.....","a##S11####.....","####...####F##G",".......#.....##",".......#.....##","....####.......","......##.......","......##.......", NULL } },
    { "SWITCH 8", 0, "", { "....#######...","....#..####...","....#..#......","#####..#......","a#S##..#......","#####..#......","..#.#..#11####","..#.##....####","..####....G###","....##........","....##........", NULL } },
    { "SWITCH 9", 0, "", { "...........###","####.......###","####.......G##","####........1.","###S........1.","..#.........1.","..#.....#####.","..#.....#####.","..##a#######..","..###.........", NULL } },
    { "SWITCH 10", 0, "", { "......###....","......###....","......###11##","........#.1#G","........#.#..","........#.#..","........####.","S#####..####.","#####F######.","#a##.....###.","####......#..", NULL } },
    { "SWITCH 11", 0, "", { "...........####","...........####",".....####11G###",".....####......",".....####......","###FF##........","###..##........","S##..##a.......",".....###.......", NULL } },
    { "SWITCH 12", 0, "", { ".......###.",".##....###.",".FS#######.",".##....###.",".##.....1..",".#......1..",".#.....####","##.....#G##","#a.........", NULL } },
    { "HEAVY 1", 0, "CROSS SWITCHES NEED THE BLOCK STANDING UP", { "#Se#......","####..#G##","####11####","...#..####","...#..####","...#......","..##......","..##......","..##......", NULL } },
    { "HEAVY 2", 0, "", { "#####......","#####..#e##","#S###..####","###########",".......####","........1..","........1..",".......G#..",".......##..",".......##..", NULL } },
    { "HEAVY 3", 0, "", { "##S...........","###...##......","##e...########","#.#222##..#G##","1.#...##......","#.#...........","#.#...........","####..........","#b####........", NULL } },
    { "HEAVY 4", 0, "", { "S##........","###........","a###...####","...1..F####","...1###.#..","..f##FF22..","..##..###..","..##..##G..","..##.......", NULL } },
    { "HEAVY 5", 0, "", { "...#........","..S##...G###","..f##...####","..a##...####","..#.....2...","..F.....2...","..#...####..","..#...####..","##########..","###...####..","##1.........", NULL } },
    { "HEAVY 6", 0, "", { ".###.........",".###.........",".###.......##",".##S.......#G",".#..........#",".#..........#","###......###2","###......b##.","e##......###.","#####11#####.", NULL } },
    { "HEAVY 7", 0, "", { "............##.","............##.","............##.","......####..##.","......##b#####.",".......#.....2.","#S#1####.....2.","##.........####","##.........###G","e#.........####", NULL } },
    { "HEAVY 8", 0, "", { "......###.....","......###..###","......#f#..###","......###2##G#",".......##..###","###S...##.....","####...#1.....","#a##..###.....","##F#F1###.....", NULL } },
    { "HEAVY 9", 0, "", { "..####..........","..####..........","..S###..........","..#.............","###.............","..#.............","..#.....111####G","..#....###....##","..#....###......","..#....###......","..#####e##......", NULL } },
    { "HEAVY 10", 0, "", { "##e.......","#S#....###","###....###",".#.....G##",".#.....###",".####..#..",".####..#..",".####11#..", NULL } },
    { "ONE WAY 1", 0, "SOME SWITCHES ONLY OPEN OR ONLY CLOSE BRIDGES", { "#i##...........","#S##...........","..#######111###",".....##....###G","...........####", NULL } },
    { "ONE WAY 2", 0, "", { "S##.......","###.......","i#m.......",".1...####.",".1...####.","#r#..####.","###...#.#.","######2##.","#.....####","......##G#", NULL } },
    { "ONE WAY 3", 0, "", { "###S.........","####.....G###","####.....####","...#.....####","...#......#..",".#i#......#..",".###......#..",".m##111####..", NULL } },
    { "ONE WAY 4", 0, "", { "#S#............","#q#............","###............","###............",".#111####......",".....####...#..",".....####...#..",".....########..","...........####","...........#uG#", NULL } },
    { "ONE WAY 5", 0, "", { ".......#####",".........###",".........2.#","###......2.#","#u#......2.#","###.....####","#Sq.....####","..#.....####","..1.....##G#",".#j#.......#",".###########", NULL } },
    { "ONE WAY 6", 0, "", { "..........####","..........####",".....###..G###","##...#####F##.","##...#m#......","##...###......","iS#11#F.......", NULL } },
    { "ONE WAY 7", 0, "", { "....####......","...#####..G###","...#.#########","#..#..........","#..1..........","#..#..........","####..........","##Sq..........","####..........", NULL } },
    { "ONE WAY 8", 0, "", { "###...........","S##...........","#####F#F##1###","#q#........###","...........###","...........###","...........#..","...........#..","...........#..","...........u#.","...........G#.", NULL } },
    { "ONE WAY 9", 0, "", { "...........###","...........###","##F........###","###..###22#G##","S##111##......",".##...##......",".##...........",".i#...........",".rn...........", NULL } },
    { "ONE WAY 10", 0, "", { "..............#G","#u#S..........##","####..........##","qj##..........#.","1.............#.","#.............#.","#.###22#F######.","#.###..####.....","#.###..####.....","#####..####.....", NULL } },
    { "COLORS 1", 0, "A SWITCH ONLY AFFECTS BRIDGES OF ITS COLOR", { "..........####","...####F######","########..####",".#######..####",".#.##qS#...2..",".#.....#...2..",".#.....F...##G",".#.....#...###",".##...m##..###",".##111b##.....", NULL } },
    { "COLORS 2", 0, "", { ".......o#.....","..###11##2##F#","..###......###","..###......#..","...F.......#..","###F.......#..","kSa#.......3..","f##.......#G##","..........####", NULL } },
    { "COLORS 3", 0, "", { "q##..###........","S##..##m.#......","#....#2..#......","F....32..#......","#....3#3###F#...","1######..####...","1####....####...","1##j#.......####","###c#.......F...","...........#G...","...........##...", NULL } },
    { "COLORS 4", 0, "", { ".....####..###",".....###b..G##","##...#####2###","##.....#...###","qS.....#......",".1...##F#.....",".1...####.....",".#.....#......",".#.....#......",".#######......",".###..........", NULL } },
    { "COLORS 5", 0, "", { "....#..######..","....#..####.###","....#######.###","f##.1..###F.#G#","S##.#.......#.2","###.#.......#.#","###.#....####.#","..#.#....######","..#.#....####..","..###....####..","..##a..........", NULL } },
    { "COLORS 6", 0, "", { "...####.....##","...######F##g#","#1######....##","#..#b###....##","#..2...#.....3","F..#...#.....3","#..#...#.....3","i###.###...###","#u##F###...##G","##S..###...###","###..###......", NULL } },
    { "COLORS 7", 0, "", { "###r.........","q##S.........","...#....###..","...#....###..","...F....###..","...F....###..","...#....#....","...#....#....","...1.###2....",".##1.###33G##",".#s#####..##v", NULL } },
    { "COLORS 8", 0, "", { "####...........","#S##........##.",".#....##....##.",".F###1nj#222##.",".##.........##.",".##..........#.",".##..........#.",".####......G###",".##F#......####",".###q......####",".####......####", NULL } },
    { "COLORS 9", 0, "", { ".##S#...........",".####...##..G###",".#f##...##..####",".####.F###22####",".#....###.......",".#....#.........","###...#.........","###...####......","##q...####......",".##111####......", NULL } },
    { "COLORS 10", 0, "", { ".....###...G#",".....###...##","###..#.....F.","#e#..#.....3.","##S..#...####","#F#..#2######",".F...#...####",".1...F.......",".###s#.......",".#bn##.......",".#####.......", NULL } },
    { "SPLIT 1", 0, "THE PAD SPLITS THE BLOCK. L OR R SWAPS CUBES", { "###...........","ST#.....##V#..","###.########..","###.#.....#...","....#.....#...","....#....####.","....#....##G#.","...##....#####","...##.........","...#U.........", NULL } },
    { "SPLIT 2", 0, "", { ".....####V#","S###.#...##","####.#..###","##T#.#..#..","####.#..#..",".#...#..#..",".....#..#..","...###..##.","...#U#..G#.", NULL } },
    { "SPLIT 3", 0, "", { "###...........","###...##V.....","##S...######..","T##...#....#..","......#....##.","......#....##.","......#....##.","......#...####","..U#..#...#G##","..#####.......", NULL } },
    { "SPLIT 4", 0, "", { ".###..........",".S#T.....V###.",".###.....##...",".........#....",".........#....",".........#....","####.....####G","####.....#..##","###U######....", NULL } },
    { "SPLIT 5", 0, "", { "###....V##...","###.######...","T#S.#..####..","....#.....#..","....#.....#..","....#.....#..","....#....#G##","...##....####","...#U.....#..","...##........","...##........", NULL } },
    { "SPLIT 6", 0, "", { "#S#.............","###..####.......","#T#..####....###","##########F##G##",".#...#V##....###",".#..............",".#..............","U##.............","###.............", NULL } },
    { "SPLIT 7", 0, "", { "............#.##",".........##F##G#",".........#......","####....###.....","##S########.....","#T###...##V.....","....#...###.....","....#...........","....#...........","....##..........","....#U..........", NULL } },
    { "SPLIT 8", 0, "", { "......F######.",".....#U...###.","##S..a#...###.","###..##.....#.","#T#..##.....#.",".....#1...#G##","......1...####","......1...F###",".....V#...####",".....##.......", NULL } },
    { "SPLIT 9", 0, "", { "##......###...","T#####1####...","S##.....#V#...","###.......#...","..#.......F#..","..#.......#...","#U#.......F...","#a#...##..##G#","###...##..####","......###F###.","......##......", NULL } },
    { "SPLIT 10", 0, "", { "F##......V#.....","###########...##","T#S1.....#....##","...1.....F....##","...1....####..#G","...#.....###..#.","...#.....###..#.","...###U..######.","...##a#.........","...####.........", NULL } },
    { "SPLIT 11", 0, "", { "..##..........","..U#..........","..##.......G##","...#.......###","...#.......###","T###...##..###","#S#######....#","###....V##F###","###...........", NULL } },
    { "SPLIT 12", 0, "", { "...........###",".......###.###","....#F####.#G#","##..#..###.###","##..##.V##.#..","ST..#..#...#..","....#..#...#..","....#..F####..",".U###....###..",".####....###..", NULL } },
    { "SPLIT 13", 0, "", { ".S##..F#######..",".###..F.....##..",".T##..###...##..",".###..###...#...","......##V..##...",".......#....#...","U###...1....###G","##a###11.....###",".............#F#", NULL } },
    { "SPLIT 14", 0, "", { ".........####..",".........####..",".###.########..",".T#S.1...####..",".###.#....F....",".....F....#....",".....#....#####","U#..V#....#####","#aF###.....###G","##.........####", NULL } },
    { "SPLIT 15", 0, "", { ".##T#......###",".####......##G",".##S#......###","...........#..","...........#..","......V###.#..","##########.#..","#.....######..","#.....####....","##............","#U............", NULL } },
    { "SPLIT 16", 0, "", { "##S.............","T##......#######","###......##...##",".........#V...##",".........##...#.",".........#....F.",".........#....#.","....U##..#...###","....######...#G#",".............###", NULL } },
    { "SPLIT 17", 0, "", { ".........F######","S##......F..####","###......#..####","#T#......#..#G##",".........#......",".........#......",".....##..###....",".#U..V#..###....",".#########......",".##.............", NULL } },
    { "SPLIT 18", 0, "", { "##S########F....","####..##V#.#....","###T.......#....","#..........#....","#..........1....","a##.....###F#F#G","##U.....####..##","........####....","........####....", NULL } },
    { "MASTER 1", 0, "CUBES JOIN WHEN THEY TOUCH. USE ALL YOU KNOW", { "###............","##T########.##G","#S#...i#V#1F###",".#......#.1....",".#......###....",".#......####...",".#......####...",".####...####...",".####...####...",".U###..........",".####..........", NULL } },
    { "MASTER 2", 0, "", { "....V#..........","TS..##..........","##..##..........","##..##......#...","##..##......####","....##.####.###G","....##.#####F###","U##.1#.####..###","e#####1####.....", NULL } },
    { "MASTER 3", 0, "", { "..#####V#.....","..#...###.....","..#....#......","###....F......","##T....#..##G#","S##..###..####","###..a##11####","..#...##..####","..#...........","#U##..........","####..........", NULL } },
    { "MASTER 4", 0, "", { "ST##11##........","###...V##F######","###....#....###G","#.#....#........","#.#..####.......","e##..####.......","U##..######.....","###..####.......", NULL } },
    { "MASTER 5", 0, "", { "..###...........","..ST#...........","..###...bV#...##","..###...##m...#G","........##.....#","......#.1F.....#","......#.#F..##.#","e#....#.##..##.#","######F###..##.#","#U......##22###F","........##......", NULL } },
    { "MASTER 6", 0, "", { "...........##","......#V...##","......#j222##","#S#....F...#F","###....1....#","F#T....1....#","###....F...##",".......##..##",".......#u..##","..#a...##..#G","..U###F##....", NULL } },
    { "MASTER 7", 0, "", { "......F22#####","......#...####","...###F...####","...e#U#......#","...#..#......#","...#..#...####","...#..V###...#","...1..####..##","##S#11###b..##","####........G#","T###..........", NULL } },
    { "MASTER 8", 0, "", { ".............##",".......########",".......F.....##",".##TF..eV#...2.",".###S..###...#.",".#####.#n#..##.",".......1F...##.",".......1#...#G.",".......##...##.","##....#b#......","U##F##F##......", NULL } },
    { "MASTER 9", 0, "", { "........###j....","........##e#..G#","........###U..##","..ST.....F....##","####.....#....##","..##.....#....2.",".........#....2.",".........#..###.",".......##V..###.",".......#####1F#.",".......###......", NULL } },
    { "MASTER 10", 0, "", { "####1F###..##","#T##1..##..##","##S##..jV222.","....#..##..F.","....#......#.","....#......#.","....#......#.","...#F....###.","..####...###.","..#U##...G##.","..#ma#...##..", NULL } },
    { "MASTER 11", 0, "", { "T##............","#S###F##V......","####...#i...###","...#...n#...###","...#...##...###","...U#...1...F##","...##...1....2#","...##...#....2#","........##...##","........##.####","........j##FF#G", NULL } },
    { "MASTER 12", 0, "", { "ST..............","##..#####...###F","##..####....####","##1#V###......##","1...#.........##","1...#.........##","#...2.......####","U##F2.......###G","je#u2.........#.","####F####.....#.","####...#####F#F.", NULL } },
    { "MASTER 13", 0, "", { "..#####.......","###.##V###222#","T##..........#","S##..........#","..1.......####","..1.......####","..#U#.....#F..","..###.....#F..","..b##.....###.","..#i#.....#G#.", NULL } },
    { "MASTER 14", 0, "", { ".....VF#......","T##..###......","S#######......","###..#v#..##..","F......1..##..","#.#....1..##..","####..##..2#..","####..######..","#U##..#j..2...","a###..##..2###","..........F##G", NULL } },
    { "MASTER 15", 0, "", { "............F###",".......111###b#F","......###.#..#..","......#V#.#..F..","##....###...####","ST....###...u###","##....#.......#.","##....#.......#.","....###.......2.","....F##....####.","....i#U....###G.", NULL } },
    { "MASTER 16", 0, "", { ".....vU#....G###",".....i##..2###F#",".......#..2.....",".......F..#.....","####...#..####..","##SF...1..####..","T#######..####..","####..V#..####..","......#b###.....", NULL } },
    { "MASTER 17", 0, "", { "..........####..","..........####..","#T#####V..####..","###...aj..##v#..","##S...1...#.....","###...1...#.....",".F...###..#.....",".#...#####F.#G##","##...F##..2.###F","##...###..22####","U#..............", NULL } },
    { "MASTER 18", 0, "", { "###F###....","..1.2Vb....","..1.2....##","###.2....##","###.##...#.","ST#.##...#.","..#.#####FG","..F.....###","..#.....v##","####.......","##eU.......", NULL } },
    { "MASTER 19", 0, "", { "....v#a........","....FU#.....F#.","....F.......##.","....1.......##.","TS..V#.F######.","##..##.#....##.","##..b#.#....##.","....#..#...####","....2..#...G###","....2###...##F#",".....###...####", NULL } },
    { "MASTER 20", 0, "", { "...........###.","...........###.","......##111###.","####..##....F..","##S###aV....#..","####........2..","T###........2..","##..........F..",".#........###..","b#U.......#####","###.......G##..", NULL } },
};
#define NUM_LEVELS 100

/* ================================================================== */
/* Audio: gritty procedural sound effects + ambient music (own thread) */
/* ================================================================== */
#define SR      44100
#define ABLOCK  1024
#define NVOICES 14
#define FIRST_SFX 5

typedef struct {
    volatile int on;
    int   wave;           /* 0 sine, 1 square, 2 noise, 3 triangle */
    float ph, f, fmul, a, amul;
    int   delay;
    unsigned rng;
    float lp, ly, ly2, gain;   /* lowpass coef (0 = off), filter states, makeup gain */
    int   poles;          /* 1 or 2 cascaded filter stages (2 = much smoother) */
    int   crush;          /* bit-crush quantisation steps (0 = off) */
    float env, envStep;   /* attack envelope */
    float lfoR, lfoD, lfoP;
    int   hold;           /* 1 = never stops (drones) */
} Voice;

#define SINTAB 2048
static float sinTab[SINTAB + 1];

static void initSinTab(void)
{
    for (int i = 0; i <= SINTAB; i++) sinTab[i] = sinf((float)i * 6.2831853f / (float)SINTAB);
}

/* phase in [0,1) -> sine, table lookup with interpolation (much cheaper than sinf) */
static float fastSin(float ph)
{
    float x = ph * (float)SINTAB;
    int i = (int)x;
    if (i < 0) i = 0;
    if (i >= SINTAB) i = SINTAB - 1;
    float f = x - (float)i;
    return sinTab[i] + (sinTab[i + 1] - sinTab[i]) * f;
}

static Voice vc[NVOICES];
static volatile int sndOn = 1;
static volatile int audioRun = 1;
static int nextSlot = 0;

static void voiceSet(int i, int wave, float f, float slide, float a, float decay, float delay)
{
    Voice *v = &vc[i];
    v->on = 0;
    v->wave = wave;
    v->ph = 0.0f;
    v->f = f;
    v->fmul = powf(slide, 1.0f / SR);
    v->a = a;
    v->amul = expf(-decay / SR);
    v->delay = (int)(delay * SR);
    v->rng = 12345u + (unsigned)i * 77u + (unsigned)(a * 1000.0f);
    v->lp = 0.0f; v->ly = 0.0f; v->ly2 = 0.0f; v->gain = 1.0f; v->poles = 1;
    v->crush = 0;
    v->env = 1.0f; v->envStep = 0.0f;
    v->lfoR = 0.0f; v->lfoD = 0.0f; v->lfoP = 0.0f;
    v->hold = 0;
    v->on = 1;
}

/* grit: lowpass k (0..1), bit-crush steps, attack seconds */
static void voiceFx(int i, float lp, int crush, float attack)
{
    Voice *v = &vc[i];
    v->on = 0;
    if (lp > 0.0f) {
        v->lp = lp;
        v->gain = sqrtf((2.0f - lp) / lp);
        if (v->gain > 6.0f) v->gain = 6.0f;
    }
    v->crush = crush;
    if (attack > 0.0f) { v->env = 0.0f; v->envStep = 1.0f / (attack * SR); }
    v->on = 1;
}

/* second filter stage: steeper rolloff, removes the harsh highs of plain noise */
static void voicePoles(int i)
{
    Voice *v = &vc[i];
    v->on = 0;
    v->poles = 2;
    v->gain *= 2.2f;
    if (v->gain > 14.0f) v->gain = 14.0f;
    v->on = 1;
}

static void voiceLfo(int i, float rate, float depth, int hold)
{
    Voice *v = &vc[i];
    v->on = 0;
    v->lfoR = rate; v->lfoD = depth; v->hold = hold;
    v->on = 1;
}

static float voiceSample(Voice *v)
{
    if (!v->on) return 0.0f;
    if (v->delay > 0) { v->delay--; return 0.0f; }
    float s;
    switch (v->wave) {
    case 0:  s = fastSin(v->ph); break;
    case 1:  s = (v->ph < 0.5f) ? 0.5f : -0.5f; break;
    case 2:  v->rng = v->rng * 1664525u + 1013904223u;
             s = (float)((int)((v->rng >> 16) & 0xFFFF)) / 32768.0f - 1.0f; break;
    default: s = (v->ph < 0.25f) ? 4.0f * v->ph :
                 (v->ph < 0.75f) ? 2.0f - 4.0f * v->ph : 4.0f * v->ph - 4.0f; break;
    }
    if (v->lp > 0.0f) {
        v->ly += v->lp * (s - v->ly);
        if (v->poles == 2) { v->ly2 += v->lp * (v->ly - v->ly2); s = v->ly2 * v->gain; }
        else s = v->ly * v->gain;
    }
    if (v->crush > 0) { float c = (float)v->crush; s = floorf(s * c + 0.5f) / c; }
    float amp = v->a;
    if (v->env < 1.0f) { v->env += v->envStep; if (v->env > 1.0f) v->env = 1.0f; amp *= v->env; }
    if (v->lfoD > 0.0f) {
        v->lfoP += v->lfoR / SR;
        if (v->lfoP >= 1.0f) v->lfoP -= 1.0f;
        amp *= 1.0f - v->lfoD * 0.5f + v->lfoD * 0.5f * fastSin(v->lfoP);
    }
    s *= amp;
    v->ph += v->f / SR;
    if (v->ph >= 1.0f) v->ph -= 1.0f;
    v->f *= v->fmul;
    v->a *= v->amul;
    if (!v->hold && v->a < 0.0005f) v->on = 0;
    return s;
}

/* ---- ambient music: low drones, breathing grit bed, sparse bells ---- */
static int mSamp = 0, mStep = 0;
static unsigned mRng = 987654321u;
static int musicStarted = 0;
static const float bellRatio[5] = { 1.0f, 1.2f, 1.3333f, 1.5f, 1.8f };

static void musicStart(void)
{
    voiceSet(0, 3, 55.0f, 1.0f, 0.20f, 0.0f, 0.0f);
    voiceFx(0, 0.18f, 0, 3.0f); voiceLfo(0, 0.08f, 0.5f, 1);
    voiceSet(1, 0, 82.41f, 1.0f, 0.10f, 0.0f, 0.0f);
    voiceFx(1, 0.0f, 0, 4.0f); voiceLfo(1, 0.05f, 0.6f, 1);
    voiceSet(2, 2, 0.0f, 1.0f, 0.10f, 0.0f, 0.0f);
    voiceFx(2, 0.02f, 18, 5.0f); voiceLfo(2, 0.13f, 0.7f, 1);
    musicStarted = 1;
}

static void musicStep(void)
{
    if (!musicStarted) musicStart();
    mRng = mRng * 1664525u + 1013904223u;
    unsigned r = mRng >> 16;
    if (r % 5 < 2) {
        float f = 220.0f * bellRatio[(r >> 4) % 5] * (((r >> 8) & 1) ? 2.0f : 1.0f);
        voiceSet(3, 0, f, 1.0f, 0.13f, 1.6f, 0.0f);
        voiceSet(4, 0, f * 2.0f, 1.0f, 0.04f, 2.4f, 0.0f);
    }
    if (mStep % 32 == 0) vc[1].f = 82.41f;
    if (mStep % 32 == 16) vc[1].f = 73.42f;
    mStep++;
}

static short __attribute__((aligned(64))) abuf[ABLOCK * 2];

static void mixBlock(short *out)
{
    for (int i = 0; i < ABLOCK; i++) {
        float s = 0.0f;
        if (sndOn) {
            if (mSamp <= 0) { musicStep(); mSamp = (int)(SR * 0.35f); }
            mSamp--;
            for (int v = 0; v < NVOICES; v++) s += voiceSample(&vc[v]);
        }
        s *= 1.25f;
        s = s / (1.0f + fabsf(s));          /* soft saturation = a little grit */
        out[i * 2]     = (short)(s * 30000.0f);
        out[i * 2 + 1] = (short)(s * 30000.0f);
    }
}

static int audioThread(SceSize args, void *argp)
{
    int ch = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, ABLOCK, PSP_AUDIO_FORMAT_STEREO);
    if (ch < 0) return 0;
    while (audioRun) {
        mixBlock(abuf);
        sceAudioOutputBlocking(ch, PSP_AUDIO_VOLUME_MAX, abuf);
    }
    sceAudioChRelease(ch);
    return 0;
}

static void audioInit(void)
{
    initSinTab();
    memset(vc, 0, sizeof(vc));
    int thid = sceKernelCreateThread("audio_thread", audioThread, 0x12, 0x10000,
                                     PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU, NULL);
    if (thid >= 0) sceKernelStartThread(thid, 0, NULL);
}

enum { SFX_ROLL, SFX_LAND, SFX_FALL, SFX_CRACK, SFX_WIN, SFX_SWITCH, SFX_BRIDGE,
       SFX_TELE, SFX_MENU, SFX_OK, SFX_BACK };

static int slot(void) { int s = FIRST_SFX + (nextSlot % (NVOICES - FIRST_SFX)); nextSlot++; return s; }

/* smooth version: two filter stages */
static void grit2(float lp, int crush, float a, float decay, float attack, float delay);

/* helper: noise burst with lowpass / crush / attack */
static void grit(float lp, int crush, float a, float decay, float attack, float delay)
{
    int s = slot();
    voiceSet(s, 2, 0.0f, 1.0f, a, decay, delay);
    voiceFx(s, lp, crush, attack);
}

static void grit2(float lp, int crush, float a, float decay, float attack, float delay)
{
    int s = slot();
    voiceSet(s, 2, 0.0f, 1.0f, a, decay, delay);
    voiceFx(s, lp, crush, attack);
    voicePoles(s);
}

static void sfx(int id)
{
    if (!sndOn) return;
    switch (id) {
    case SFX_ROLL:       /* smooth low stone slide: dark filtered noise + soft thump, no harsh highs */
        grit2(0.030f, 0, 0.20f, 6.5f, 0.06f, 0.0f);
        grit2(0.090f, 0, 0.05f, 9.0f, 0.05f, 0.0f);
        voiceSet(slot(), 0, 68.0f, 0.75f, 0.17f, 9.0f, 0.0f);
        break;
    case SFX_LAND:       /* soft heavy thud */
        voiceSet(slot(), 0, 90.0f, 0.30f, 0.55f, 12.0f, 0.0f);
        grit2(0.12f, 0, 0.20f, 22.0f, 0.0f, 0.0f);
        break;
    case SFX_CRACK:
        grit(0.40f, 24, 0.50f, 7.0f, 0.0f, 0.0f);
        voiceSet(slot(), 0, 140.0f, 0.2f, 0.40f, 10.0f, 0.0f);
        grit(0.60f, 16, 0.18f, 30.0f, 0.0f, 0.06f);
        break;
    case SFX_FALL:
        voiceSet(slot(), 0, 420.0f, 0.05f, 0.25f, 1.4f, 0.0f);
        grit2(0.05f, 0, 0.22f, 2.0f, 0.06f, 0.0f);
        grit2(0.14f, 0, 0.25f, 6.0f, 0.0f, 0.62f);
        break;
    case SFX_WIN: {
        static const float n[4] = { 523.25f, 659.25f, 783.99f, 1046.5f };
        for (int i = 0; i < 4; i++) {
            voiceSet(slot(), 0, n[i], 1.0f, 0.30f, 2.5f, 0.14f * i);
            voiceSet(slot(), 0, n[i] * 2.0f, 1.0f, 0.08f, 3.0f, 0.14f * i);
        }
        voiceSet(slot(), 0, 130.81f, 1.0f, 0.25f, 1.2f, 0.0f);
        break; }
    case SFX_SWITCH:
        voiceSet(slot(), 3, 330.0f, 1.0f, 0.22f, 26.0f, 0.0f);
        grit(0.30f, 0, 0.16f, 45.0f, 0.0f, 0.0f);
        voiceSet(slot(), 3, 495.0f, 1.0f, 0.20f, 26.0f, 0.06f);
        break;
    case SFX_BRIDGE:     /* stone grinding into place */
        grit2(0.05f, 0, 0.28f, 2.2f, 0.12f, 0.08f);
        voiceSet(slot(), 3, 48.0f, 1.0f, 0.26f, 2.5f, 0.08f);
        break;
    case SFX_TELE:
        voiceSet(slot(), 3, 180.0f, 3.5f, 0.35f, 2.5f, 0.0f);
        voiceSet(slot(), 0, 360.0f, 3.0f, 0.20f, 2.5f, 0.05f);
        grit(0.15f, 0, 0.12f, 3.0f, 0.0f, 0.0f);
        break;
    case SFX_MENU:
        voiceSet(slot(), 3, 520.0f, 1.0f, 0.20f, 28.0f, 0.0f);
        break;
    case SFX_OK:
        voiceSet(slot(), 3, 520.0f, 1.0f, 0.22f, 20.0f, 0.0f);
        voiceSet(slot(), 3, 780.0f, 1.0f, 0.22f, 20.0f, 0.07f);
        break;
    case SFX_BACK:
        voiceSet(slot(), 3, 400.0f, 1.0f, 0.22f, 20.0f, 0.0f);
        voiceSet(slot(), 3, 300.0f, 1.0f, 0.22f, 20.0f, 0.07f);
        break;
    }
}

static void setSound(int on)
{
    sndOn = on;
    if (!on) { for (int i = 0; i < NVOICES; i++) vc[i].on = 0; musicStarted = 0; }
}

/* ================================================================== */
/* Save data                                                           */
/* ================================================================== */
typedef struct { unsigned magic; int unlocked; int sound; int best[128]; } Save;
#define SAVE_MAGIC 0x42584F53u

static Save save;
static char savePath[300];

static void saveInitPath(const char *argv0)
{
    savePath[0] = 0;
    if (argv0) {
        const char *slash = strrchr(argv0, '/');
        if (slash) {
            size_t n = (size_t)(slash - argv0) + 1;
            if (n < sizeof(savePath) - 20) { memcpy(savePath, argv0, n); savePath[n] = 0; }
        }
    }
    strcat(savePath, "bloxorz.sav");
}

static void saveWrite(void)
{
    FILE *f = fopen(savePath, "wb");
    if (!f) return;
    fwrite(&save, sizeof(save), 1, f);
    fclose(f);
}

static void saveLoad(void)
{
    memset(&save, 0, sizeof(save));
    save.magic = SAVE_MAGIC;
    save.unlocked = 0;
    save.sound = 1;
    FILE *f = fopen(savePath, "rb");
    if (f) {
        Save tmp;
        if (fread(&tmp, sizeof(tmp), 1, f) == 1 && tmp.magic == SAVE_MAGIC &&
            tmp.unlocked >= 0 && tmp.unlocked < NUM_LEVELS) save = tmp;
        fclose(f);
    }
    setSound(save.sound);
}

/* ================================================================== */
/* Game state                                                          */
/* ================================================================== */
typedef struct { int x, z, o; } Block;   /* o: 0 standing, 1 lies along X, 2 lies along Z */

enum { ST_INTRO, ST_IDLE, ST_ROLL, ST_FALL, ST_WIN };
enum { SCR_MENU, SCR_SELECT, SCR_HELP, SCR_GAME, SCR_END };
#define HELP_PAGES 3
#define PAUSE_ITEMS 6

static char  map[MAXH][MAXW];
static int   mapW, mapH;
static Block blk, prevBlk;
static int   rollDX, rollDZ;
static float anim, introT, stateT;
static int   state;
static int   level, moves, totalMoves;
static int   bridgeMask;
static float bridgeAnim[4];
static int   screen = SCR_MENU;
static int   paused, pauseSel, menuSel, selSel, helpPage;
static int   gTick;
static float fade = 1.0f;
static int   showFps;
static float fpsShown;
static int   fpsFrames;
static unsigned fpsLast;
static int   fellOnFragile;

/* split-block (teleporter) state */
static int   split;
static int   cubeX[2], cubeZ[2], activeCube, rollCube, fallCube;
static int   prevCX, prevCZ;
static int   spawnX[2], spawnZ[2];
static float spawnT;

static int   bufferedDir = -1;
static float bufferAge;
static int   heldDir = -1;
static float heldTime;
static int   lastPressed = -1;

static const int DIR_DX[4] = {  0,  0, -1,  1 };   /* up, down, left, right */
static const int DIR_DZ[4] = { -1,  1,  0,  0 };

static void computeCamera(void);

static char tileAt(int x, int z)
{
    if (x < 0 || z < 0 || x >= MAXW || z >= MAXH) return '.';
    return map[z][x];
}

/* a tile you can stand a single cube on (the goal hole is NOT one) */
static int tilePresent(char c)
{
    if (c == '.' || c == 0 || c == 'G') return 0;
    if (c >= '1' && c <= '4') return (bridgeMask >> (c - '1')) & 1;
    return 1;
}

static void loadLevel(int n, int quick)
{
    memset(map, '.', sizeof(map));
    mapW = mapH = 0;
    spawnX[0] = spawnX[1] = spawnZ[0] = spawnZ[1] = -1;
    const LevelDef *L = &LEVELS[n];
    for (int z = 0; z < MAXH && L->rows[z]; z++) {
        int len = (int)strlen(L->rows[z]);
        for (int x = 0; x < len && x < MAXW; x++) {
            char c = L->rows[z][x];
            map[z][x] = c;
            if (c == 'S') { blk.x = x; blk.z = z; blk.o = 0; }
            if (c == 'U') { spawnX[0] = x; spawnZ[0] = z; }
            if (c == 'V') { spawnX[1] = x; spawnZ[1] = z; }
        }
        if (len > mapW) mapW = len;
        mapH = z + 1;
    }
    bridgeMask = L->init;
    for (int g = 0; g < 4; g++) bridgeAnim[g] = (float)((bridgeMask >> g) & 1);
    computeCamera();
    level = n;
    moves = 0;
    split = 0;
    activeCube = 0;
    fallCube = -1;
    spawnT = 1.0f;
    state = ST_INTRO;
    introT = quick ? 1.35f : 0.0f;
    anim = 0.0f;
    stateT = 0.0f;
    paused = 0;
    bufferedDir = -1;
    heldDir = -1;
    heldTime = 0.0f;
}

static Block rollBlock(Block b, int dx, int dz)
{
    Block n = b;
    if (b.o == 0) {
        if      (dx ==  1) { n.x = b.x + 1; n.o = 1; }
        else if (dx == -1) { n.x = b.x - 2; n.o = 1; }
        else if (dz ==  1) { n.z = b.z + 1; n.o = 2; }
        else               { n.z = b.z - 2; n.o = 2; }
    } else if (b.o == 1) {
        if      (dx ==  1) { n.x = b.x + 2; n.o = 0; }
        else if (dx == -1) { n.x = b.x - 1; n.o = 0; }
        else               { n.z = b.z + dz; }
    } else {
        if      (dz ==  1) { n.z = b.z + 2; n.o = 0; }
        else if (dz == -1) { n.z = b.z - 1; n.o = 0; }
        else               { n.x = b.x + dx; }
    }
    return n;
}

static int blockValid(Block b)
{
    int n = (b.o == 0) ? 1 : 2;
    for (int i = 0; i < n; i++) {
        int x = b.x + (b.o == 1 ? i : 0);
        int z = b.z + (b.o == 2 ? i : 0);
        char c = tileAt(x, z);
        if (c == 'G') { if (b.o != 0) return 0; continue; }   /* only upright may enter the hole */
        if (!tilePresent(c)) return 0;
    }
    if (b.o == 0 && tileAt(b.x, b.z) == 'F') return 0;
    return 1;
}

/* switch kinds: a-d sq toggle, e-h cross toggle, i-l sq ON, m-p sq OFF, q-t cross ON, u-x cross OFF */
static int applyKind(char c, int upright, int cube)
{
    if (c < 'a' || c > 'x') return 0;
    static const int softTab[6] = { 1, 0, 1, 1, 0, 0 };
    static const int modeTab[6] = { 0, 0, 1, 2, 1, 2 };   /* 0 toggle 1 on 2 off */
    int idx = c - 'a', kind = idx / 4, g = idx % 4;
    if (!softTab[kind] && (cube || !upright)) return 0;
    int bit = 1 << g, old = bridgeMask;
    if (modeTab[kind] == 0)      bridgeMask ^= bit;
    else if (modeTab[kind] == 1) bridgeMask |= bit;
    else                         bridgeMask &= ~bit;
    return bridgeMask != old;
}

static void applySwitchesBlock(Block b)
{
    int n = (b.o == 0) ? 1 : 2, changed = 0;
    for (int i = 0; i < n; i++) {
        int x = b.x + (b.o == 1 ? i : 0);
        int z = b.z + (b.o == 2 ? i : 0);
        changed |= applyKind(tileAt(x, z), b.o == 0, 0);
    }
    if (changed) { sfx(SFX_SWITCH); sfx(SFX_BRIDGE); }
}

static void startRoll(int dir)
{
    rollDX = DIR_DX[dir];
    rollDZ = DIR_DZ[dir];
    if (split) {
        int a = activeCube;
        int nx = cubeX[a] + rollDX, nz = cubeZ[a] + rollDZ;
        if (nx == cubeX[1 - a] && nz == cubeZ[1 - a]) return;   /* cubes can't overlap */
        prevCX = cubeX[a]; prevCZ = cubeZ[a];
        cubeX[a] = nx; cubeZ[a] = nz;
        rollCube = a;
    } else {
        prevBlk = blk;
        blk = rollBlock(blk, rollDX, rollDZ);
    }
    anim = 0.0f;
    state = ST_ROLL;
    moves++;
    totalMoves++;
    sfx(SFX_ROLL);
}

static void swapCube(void)
{
    if (split) { activeCube ^= 1; sfx(SFX_MENU); }
}

static void finishLevel(void)
{
    if (save.best[level] == 0 || moves < save.best[level]) save.best[level] = moves;
    if (level + 1 < NUM_LEVELS && save.unlocked < level + 1) save.unlocked = level + 1;
    saveWrite();
}

static void gotoScreen(int s);

static void startFall(int which, int onFragile)
{
    state = ST_FALL;
    stateT = 0.0f;
    anim = 0.0f;
    fallCube = which;
    fellOnFragile = onFragile;
    sfx(onFragile ? SFX_CRACK : SFX_LAND);
    sfx(SFX_FALL);
    bufferedDir = -1;
}

static void landBlock(void)
{
    if (!blockValid(blk)) {
        startFall(-1, blk.o == 0 && tileAt(blk.x, blk.z) == 'F');
        return;
    }
    sfx(SFX_LAND);
    applySwitchesBlock(blk);
    if (blk.o == 0 && tileAt(blk.x, blk.z) == 'G') {
        state = ST_WIN; stateT = 0.0f; anim = 0.0f;
        sfx(SFX_WIN);
        finishLevel();
        bufferedDir = -1;
        return;
    }
    if (blk.o == 0 && tileAt(blk.x, blk.z) == 'T' && spawnX[0] >= 0 && spawnX[1] >= 0) {
        split = 1;
        cubeX[0] = spawnX[0]; cubeZ[0] = spawnZ[0];
        cubeX[1] = spawnX[1]; cubeZ[1] = spawnZ[1];
        activeCube = 0;
        spawnT = 0.0f;
        sfx(SFX_TELE);
    }
    state = ST_IDLE;
}

static void landCube(void)
{
    int a = rollCube;
    char c = tileAt(cubeX[a], cubeZ[a]);
    if (!tilePresent(c)) { startFall(a, 0); return; }
    sfx(SFX_LAND);
    if (applyKind(c, 1, 1)) { sfx(SFX_SWITCH); sfx(SFX_BRIDGE); }
    int ox = cubeX[1 - a], oz = cubeZ[1 - a];
    int dx = cubeX[a] - ox, dz = cubeZ[a] - oz;
    if ((dx < 0 ? -dx : dx) + (dz < 0 ? -dz : dz) == 1) {     /* cubes touch -> join */
        blk.x = cubeX[a] < ox ? cubeX[a] : ox;
        blk.z = cubeZ[a] < oz ? cubeZ[a] : oz;
        blk.o = (cubeZ[a] == oz) ? 1 : 2;
        split = 0;
        sfx(SFX_OK);
    }
    state = ST_IDLE;
}

static void updateGame(void)
{
    for (int g = 0; g < 4; g++) {
        float target = (float)((bridgeMask >> g) & 1);
        if (bridgeAnim[g] < target) { bridgeAnim[g] += DT * 3.0f; if (bridgeAnim[g] > target) bridgeAnim[g] = target; }
        if (bridgeAnim[g] > target) { bridgeAnim[g] -= DT * 3.0f; if (bridgeAnim[g] < target) bridgeAnim[g] = target; }
    }
    if (spawnT < 1.0f) { spawnT += DT / 0.5f; if (spawnT > 1.0f) spawnT = 1.0f; }

    switch (state) {
    case ST_INTRO: {
        float before = introT;
        introT += DT;
        if (before < 1.62f && introT >= 1.62f) sfx(SFX_LAND);
        if (introT >= 1.7f) state = ST_IDLE;
        break;
    }
    case ST_ROLL:
        anim += DT / ROLL_TIME;
        if (anim >= 1.0f) {
            anim = 0.0f;
            if (split) landCube(); else landBlock();
        }
        break;
    case ST_FALL:
        anim += DT / FALL_TIME;
        stateT += DT;
        if (anim >= 1.0f) loadLevel(level, 1);
        break;
    case ST_WIN:
        anim += DT / WIN_TIME;
        stateT += DT;
        if (anim >= 1.0f) {
            if (level + 1 >= NUM_LEVELS) { gotoScreen(SCR_END); }
            else { loadLevel(level + 1, 0); fade = 0.6f; }
        }
        break;
    default: break;
    }
}

/* ================================================================== */
/* Input                                                               */
/* ================================================================== */
static SceCtrlData pad;
static unsigned int oldBtn, btnPressed;
static int analogActive;
static int dirDown[4], dirPrev[4], dirEdge[4];
static int waitRelease;

static void pollInput(int faceDirs)
{
    sceCtrlPeekBufferPositive(&pad, 1);
    unsigned int b = pad.Buttons;
    btnPressed = b & ~oldBtn;
    oldBtn = b;

    dirDown[0] = (b & PSP_CTRL_UP)    != 0;
    dirDown[1] = (b & PSP_CTRL_DOWN)  != 0;
    dirDown[2] = (b & PSP_CTRL_LEFT)  != 0;
    dirDown[3] = (b & PSP_CTRL_RIGHT) != 0;
    if (faceDirs) {
        dirDown[0] |= (b & PSP_CTRL_TRIANGLE) != 0;
        dirDown[1] |= (b & PSP_CTRL_CROSS)    != 0;
        dirDown[2] |= (b & PSP_CTRL_SQUARE)   != 0;
        dirDown[3] |= (b & PSP_CTRL_CIRCLE)   != 0;
    }

    int ax = (int)pad.Lx - 128, ay = (int)pad.Ly - 128;
    int mx = ax < 0 ? -ax : ax, my = ay < 0 ? -ay : ay;
    int m = mx > my ? mx : my;
    if (!analogActive && m > 70) analogActive = 1;
    else if (analogActive && m < 40) analogActive = 0;
    if (analogActive) {
        if (mx > my) dirDown[ax < 0 ? 2 : 3] = 1;
        else         dirDown[ay < 0 ? 0 : 1] = 1;
    }

    if (waitRelease) {
        for (int i = 0; i < 4; i++) { dirPrev[i] = dirDown[i]; dirEdge[i] = 0; }
        btnPressed = 0;
        if (!(b & (PSP_CTRL_CROSS | PSP_CTRL_CIRCLE | PSP_CTRL_SQUARE | PSP_CTRL_TRIANGLE |
                   PSP_CTRL_START | PSP_CTRL_SELECT))) waitRelease = 0;
        return;
    }
    for (int i = 0; i < 4; i++) {
        dirEdge[i] = dirDown[i] && !dirPrev[i];
        dirPrev[i] = dirDown[i];
    }
}

#define CONFIRM_BTN (PSP_CTRL_CROSS | PSP_CTRL_CIRCLE | PSP_CTRL_START)
#define BACK_BTN    (PSP_CTRL_TRIANGLE | PSP_CTRL_SQUARE)
#define SWAP_BTN    (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER)

static void gotoScreen(int s)
{
    screen = s;
    fade = 1.0f;
    waitRelease = 1;
    paused = 0;
}

static void startLevel(int n)
{
    if (n == 0) totalMoves = 0;
    loadLevel(n, 0);
    gotoScreen(SCR_GAME);
}

static void skipLevel(void)
{
    if (level + 1 < NUM_LEVELS) {
        if (save.unlocked < level + 1) { save.unlocked = level + 1; saveWrite(); }
        loadLevel(level + 1, 0);
    }
}

static void gameInput(void)
{
    if (paused) {
        if (dirEdge[0]) { pauseSel = (pauseSel + PAUSE_ITEMS - 1) % PAUSE_ITEMS; sfx(SFX_MENU); }
        if (dirEdge[1]) { pauseSel = (pauseSel + 1) % PAUSE_ITEMS; sfx(SFX_MENU); }
        if (btnPressed & PSP_CTRL_START) { paused = 0; waitRelease = 1; sfx(SFX_BACK); return; }
        if (btnPressed & BACK_BTN)       { paused = 0; waitRelease = 1; sfx(SFX_BACK); return; }
        if (btnPressed & (PSP_CTRL_CROSS | PSP_CTRL_CIRCLE)) {
            sfx(SFX_OK);
            switch (pauseSel) {
            case 0: paused = 0; waitRelease = 1; break;
            case 1: loadLevel(level, 1); waitRelease = 1; break;
            case 2: skipLevel(); waitRelease = 1; break;
            case 3: gotoScreen(SCR_SELECT); selSel = level; break;
            case 4: gotoScreen(SCR_MENU); break;
            case 5: save.sound = !save.sound; setSound(save.sound); saveWrite(); sfx(SFX_OK); break;
            }
        }
        return;
    }

    if (btnPressed & PSP_CTRL_START) { paused = 1; pauseSel = 0; sfx(SFX_OK); return; }
    if (btnPressed & PSP_CTRL_SELECT) { loadLevel(level, 1); sfx(SFX_BACK); return; }
    if ((btnPressed & SWAP_BTN) && split && state == ST_IDLE) swapCube();

    int edge = -1;
    for (int i = 0; i < 4; i++) if (dirEdge[i]) edge = i;
    if (edge >= 0) lastPressed = edge;

    if (lastPressed >= 0 && dirDown[lastPressed]) {
        if (heldDir != lastPressed) { heldDir = lastPressed; heldTime = 0.0f; }
        else heldTime += DT;
    } else {
        heldDir = -1;
        for (int i = 0; i < 4; i++) if (dirDown[i]) { heldDir = i; heldTime = 0.0f; lastPressed = i; }
    }

    if (bufferedDir >= 0) {
        bufferAge += DT;
        if (bufferAge > BUFFER_TIME) bufferedDir = -1;
    }
    if (edge >= 0 && (state == ST_ROLL || state == ST_INTRO)) { bufferedDir = edge; bufferAge = 0.0f; }

    if (state == ST_IDLE) {
        int go = -1;
        if (edge >= 0)                                   go = edge;
        else if (bufferedDir >= 0)                       go = bufferedDir;
        else if (heldDir >= 0 && heldTime > REPEAT_WAIT) go = heldDir;
        if (go >= 0) { bufferedDir = -1; startRoll(go); }
    }
}

/* ================================================================== */
/* Rendering primitives                                                */
/* ================================================================== */
typedef struct { float u, v; unsigned int color; float x, y, z; } TVert;
typedef struct { unsigned int color; float x, y, z; } V2;

#define TEXSZ 64
static unsigned int __attribute__((aligned(16))) list[262144];
static unsigned int __attribute__((aligned(16))) tex[4][TEXSZ * TEXSZ];
enum { TEX_WHITE, TEX_STONE, TEX_RUST, TEX_WOOD };
static int curTex = -1;
static void *texVram[4];
#define TEX_VRAM_OFFSET 0x180000

static float hash2(int x, int y)
{
    unsigned h = (unsigned)x * 374761393u + (unsigned)y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (float)(h & 0xFFFF) / 65535.0f;
}

/* tileable value noise: lattice period `per` */
static float vnoise(float x, float y, int perx, int pery)
{
    int xi = (int)floorf(x), yi = (int)floorf(y);
    float fx = x - xi, fy = y - yi;
    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);
    int x0 = ((xi % perx) + perx) % perx, x1 = (x0 + 1) % perx;
    int y0 = ((yi % pery) + pery) % pery, y1 = (y0 + 1) % pery;
    float a = hash2(x0, y0), b = hash2(x1, y0), c = hash2(x0, y1), d = hash2(x1, y1);
    return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
}

static unsigned int rgbPix(float r, float g, float b)
{
    int ir = (int)clampf(r, 0.0f, 255.0f), ig = (int)clampf(g, 0.0f, 255.0f), ib = (int)clampf(b, 0.0f, 255.0f);
    return 0xFF000000u | ((unsigned)ib << 16) | ((unsigned)ig << 8) | (unsigned)ir;
}

static void makeTextures(void)
{
    for (int y = 0; y < TEXSZ; y++) {
        for (int x = 0; x < TEXSZ; x++) {
            int idx = y * TEXSZ + x;
            tex[TEX_WHITE][idx] = 0xFFFFFFFFu;

            float n1 = vnoise(x / 16.0f, y / 16.0f, 4, 4);
            float n2 = vnoise(x / 4.0f,  y / 4.0f,  16, 16);
            float n3 = vnoise(x / 2.0f,  y / 2.0f,  32, 32);
            float px = hash2(x * 7 + 3, y * 13 + 5);

            /* --- stone slab (grey) --- */
            float s = 0.90f + 0.10f * (n1 - 0.5f) * 2.0f + 0.05f * (n2 - 0.5f) * 2.0f + 0.04f * (n3 - 0.5f);
            if (px > 0.985f) s -= 0.16f;                           /* tiny pits */
            if (x < 3 || y < 3) s += 0.10f;                        /* lit bevel */
            else if (x > TEXSZ - 4 || y > TEXSZ - 4) s -= 0.20f;   /* shaded bevel */
            if (x == 0 || y == 0 || x == TEXSZ - 1 || y == TEXSZ - 1) s -= 0.18f;
            tex[TEX_STONE][idx] = rgbPix(s * 255.0f, s * 255.0f, s * 255.0f);

            /* --- rusty metal (block) --- */
            float grime = 0.62f + 0.38f * n1;
            float streak = vnoise(x / 2.0f, 0.5f, 32, 1);
            float rustMix = 0.0f;
            if (n2 > 0.62f) rustMix = (n2 - 0.62f) * 2.6f;
            if (rustMix > 1.0f) rustMix = 1.0f;
            float rr = 160.0f, rg = 120.0f, rb = 116.0f;
            rr += (182.0f - rr) * rustMix; rg += (96.0f - rg) * rustMix; rb += (58.0f - rb) * rustMix;
            float k = grime * (0.80f + 0.28f * streak) * (0.92f + 0.14f * n3);
            if (px > 0.99f) k *= 1.35f;                            /* bright specks */
            if (x < 2 || y < 2) k *= 1.25f;
            else if (x > TEXSZ - 3 || y > TEXSZ - 3) k *= 0.75f;
            tex[TEX_RUST][idx] = rgbPix(rr * k, rg * k, rb * k);

            /* --- fragile planks (orange wood) --- */
            float wg = vnoise(x / 32.0f, y / 2.0f, 2, 32);
            float wk = 0.80f + 0.22f * wg + 0.08f * (n2 - 0.5f);
            if (y % 16 == 0 || y % 16 == 15) wk *= 0.62f;          /* plank seams */
            int d1 = ((x * 3 + y * 5) % 47) - 23;
            int d2 = ((x * 5 - y * 3 + 192) % 53) - 26;
            if (((d1 > -2 && d1 < 1) && x > 10 && x < 54) || ((d2 > -2 && d2 < 1) && y > 14)) wk *= 0.45f;
            if (x < 2) wk *= 1.2f; else if (x > TEXSZ - 3) wk *= 0.7f;
            tex[TEX_WOOD][idx] = rgbPix(220.0f * wk, 150.0f * wk, 88.0f * wk);
        }
    }
    /* copy to VRAM: texture fetches from VRAM are much faster than from main RAM */
    char *vram = (char *)sceGeEdramGetAddr() + TEX_VRAM_OFFSET;
    for (int i = 0; i < 4; i++) {
        texVram[i] = vram + i * TEXSZ * TEXSZ * 4;
        memcpy(texVram[i], tex[i], TEXSZ * TEXSZ * 4);
    }
    sceKernelDcacheWritebackAll();
}

static void bindTex(int id)
{
    if (id == curTex) return;
    curTex = id;
    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    sceGuTexImage(0, TEXSZ, TEXSZ, TEXSZ, texVram[id] ? texVram[id] : (void *)tex[id]);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGB);
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);
    sceGuTexWrap(GU_REPEAT, GU_REPEAT);
}

static void begin3D(void)
{
    sceGuEnable(GU_DEPTH_TEST);
    sceGuDisable(GU_BLEND);
    sceGuEnable(GU_TEXTURE_2D);
    curTex = -1;
}

static void begin2D(void)
{
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    curTex = -1;
}

/* per-face colour multipliers: bottom, top, back(-z), front(+z), left(-x), right(+x) */
enum { PROF_TILE, PROF_BLOCK };
static const float PROF[2][6][3] = {
    { {0.35f,0.35f,0.37f}, {1.00f,1.00f,1.00f}, {0.74f,0.74f,0.76f},
      {0.66f,0.66f,0.69f}, {0.84f,0.84f,0.86f}, {0.58f,0.58f,0.61f} },
    { {0.28f,0.22f,0.20f}, {1.00f,1.00f,1.00f}, {0.50f,0.40f,0.37f},
      {0.50f,0.41f,0.38f}, {0.56f,0.45f,0.42f}, {0.50f,0.41f,0.38f} }
};

static unsigned int shadeRGB(unsigned int c, float fr, float fg, float fb)
{
    int r = (int)((c & 0xFF) * fr);
    int g = (int)(((c >> 8) & 0xFF) * fg);
    int b = (int)(((c >> 16) & 0xFF) * fb);
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    return (c & 0xFF000000u) | ((unsigned)b << 16) | ((unsigned)g << 8) | (unsigned)r;
}

static unsigned int shade(unsigned int c, float f) { return shadeRGB(c, f, f, f); }

static unsigned int mixCol(unsigned int a, unsigned int b, float t)
{
    float ar = (float)(a & 0xFF), ag = (float)((a >> 8) & 0xFF), ab = (float)((a >> 16) & 0xFF);
    float br = (float)(b & 0xFF), bg = (float)((b >> 8) & 0xFF), bb = (float)((b >> 16) & 0xFF);
    return rgbPix(ar + (br - ar) * t, ag + (bg - ag) * t, ab + (bb - ab) * t);
}

static const unsigned char faceIdx[6][4] = {
    {0,1,5,4}, {2,3,7,6}, {0,1,3,2}, {4,5,7,6}, {0,4,6,2}, {1,5,7,3}
};

static int gAllFaces = 0;   /* 1 while drawing rotating boxes (bottom face can show) */

static void drawBox(float x0, float y0, float z0, float x1, float y1, float z1,
                    unsigned int c, int texId, int prof)
{
    bindTex(texId);
    TVert *v = (TVert *)sceGuGetMemory(36 * sizeof(TVert));
    float cx[2] = { x0, x1 }, cy[2] = { y0, y1 }, cz[2] = { z0, z1 };
    static const int tri[6] = { 0, 1, 2, 0, 2, 3 };
    int n = 0;
    for (int f = 0; f < 6; f++) {
        if (f == 0 && !gAllFaces) continue;
        unsigned int col = shadeRGB(c, PROF[prof][f][0], PROF[prof][f][1], PROF[prof][f][2]);
        for (int t = 0; t < 6; t++) {
            int i = faceIdx[f][tri[t]];
            float px = cx[i & 1], py = cy[(i >> 1) & 1], pz = cz[(i >> 2) & 1];
            v[n].color = col;
            v[n].x = px; v[n].y = py; v[n].z = pz;
            if (f < 2)       { v[n].u = px; v[n].v = pz; }
            else if (f < 4)  { v[n].u = px; v[n].v = py; }
            else             { v[n].u = pz; v[n].v = py; }
            n++;
        }
    }
    sceGumDrawArray(GU_TRIANGLES,
                    GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
                    n, 0, v);
}

/* flat translucent quad on the floor (shadows) */
static void drawFlat(float x0, float z0, float x1, float z1, float y, unsigned int c)
{
    bindTex(TEX_WHITE);
    TVert *v = (TVert *)sceGuGetMemory(6 * sizeof(TVert));
    static const int ix[6] = { 0, 1, 1, 0, 1, 0 };
    static const int iz[6] = { 0, 0, 1, 0, 1, 1 };
    for (int i = 0; i < 6; i++) {
        v[i].u = 0; v[i].v = 0; v[i].color = c;
        v[i].x = ix[i] ? x1 : x0; v[i].y = y; v[i].z = iz[i] ? z1 : z0;
    }
    sceGumDrawArray(GU_TRIANGLES,
                    GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
                    6, 0, v);
}

static void modelIdentity(void)
{
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
}

/* ---- 2D ---- */
static void drawQuad2D(float x0, float y0, float x1, float y1,
                       unsigned int ctop, unsigned int cbot)
{
    V2 *v = (V2 *)sceGuGetMemory(6 * sizeof(V2));
    v[0].color = ctop; v[0].x = x0; v[0].y = y0; v[0].z = 0;
    v[1].color = ctop; v[1].x = x1; v[1].y = y0; v[1].z = 0;
    v[2].color = cbot; v[2].x = x1; v[2].y = y1; v[2].z = 0;
    v[3].color = ctop; v[3].x = x0; v[3].y = y0; v[3].z = 0;
    v[4].color = cbot; v[4].x = x1; v[4].y = y1; v[4].z = 0;
    v[5].color = cbot; v[5].x = x0; v[5].y = y1; v[5].z = 0;
    sceGuDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 6, 0, v);
}

static void rect(float x, float y, float w, float h, unsigned int c)
{
    drawQuad2D(x, y, x + w, y + h, c, c);
}

static void frame2D(float x, float y, float w, float h, float t, unsigned int c)
{
    rect(x, y, w, t, c);
    rect(x, y + h - t, w, t, c);
    rect(x, y, t, h, c);
    rect(x + w - t, y, t, h, c);
}

/* ---- vector font ---- */
static const signed char gl_65[] = {0,12,0,4,4,0,8,4,8,12,100,0,7,8,7,100,101};
static const signed char gl_66[] = {0,0,0,12,6,12,8,10,8,8,6,6,0,6,100,6,6,8,4,8,2,6,0,0,0,100,101};
static const signed char gl_67[] = {8,2,6,0,2,0,0,2,0,10,2,12,6,12,8,10,100,101};
static const signed char gl_68[] = {0,0,0,12,4,12,8,8,8,4,4,0,0,0,100,101};
static const signed char gl_69[] = {8,0,0,0,0,12,8,12,100,0,6,6,6,100,101};
static const signed char gl_70[] = {8,0,0,0,0,12,100,0,6,6,6,100,101};
static const signed char gl_71[] = {8,2,6,0,2,0,0,2,0,10,2,12,6,12,8,10,8,6,4,6,100,101};
static const signed char gl_72[] = {0,0,0,12,100,8,0,8,12,100,0,6,8,6,100,101};
static const signed char gl_73[] = {2,0,6,0,100,4,0,4,12,100,2,12,6,12,100,101};
static const signed char gl_74[] = {8,0,8,10,6,12,2,12,0,10,100,101};
static const signed char gl_75[] = {0,0,0,12,100,8,0,0,6,8,12,100,101};
static const signed char gl_76[] = {0,0,0,12,8,12,100,101};
static const signed char gl_77[] = {0,12,0,0,4,6,8,0,8,12,100,101};
static const signed char gl_78[] = {0,12,0,0,8,12,8,0,100,101};
static const signed char gl_79[] = {2,0,6,0,8,2,8,10,6,12,2,12,0,10,0,2,2,0,100,101};
static const signed char gl_80[] = {0,12,0,0,6,0,8,2,8,4,6,6,0,6,100,101};
static const signed char gl_81[] = {2,0,6,0,8,2,8,10,6,12,2,12,0,10,0,2,2,0,100,5,8,8,12,100,101};
static const signed char gl_82[] = {0,12,0,0,6,0,8,2,8,4,6,6,0,6,100,4,6,8,12,100,101};
static const signed char gl_83[] = {8,2,6,0,2,0,0,2,0,4,2,6,6,6,8,8,8,10,6,12,2,12,0,10,100,101};
static const signed char gl_84[] = {0,0,8,0,100,4,0,4,12,100,101};
static const signed char gl_85[] = {0,0,0,10,2,12,6,12,8,10,8,0,100,101};
static const signed char gl_86[] = {0,0,4,12,8,0,100,101};
static const signed char gl_87[] = {0,0,2,12,4,6,6,12,8,0,100,101};
static const signed char gl_88[] = {0,0,8,12,100,8,0,0,12,100,101};
static const signed char gl_89[] = {0,0,4,6,8,0,100,4,6,4,12,100,101};
static const signed char gl_90[] = {0,0,8,0,0,12,8,12,100,101};
static const signed char gl_48[] = {2,0,6,0,8,2,8,10,6,12,2,12,0,10,0,2,2,0,100,2,10,6,2,100,101};
static const signed char gl_49[] = {2,2,4,0,4,12,100,2,12,6,12,100,101};
static const signed char gl_50[] = {0,2,2,0,6,0,8,2,8,4,0,12,8,12,100,101};
static const signed char gl_51[] = {0,0,8,0,4,6,6,6,8,8,8,10,6,12,2,12,0,10,100,101};
static const signed char gl_52[] = {6,12,6,0,0,8,8,8,100,101};
static const signed char gl_53[] = {8,0,0,0,0,6,6,6,8,8,8,10,6,12,2,12,0,10,100,101};
static const signed char gl_54[] = {8,2,6,0,2,0,0,2,0,10,2,12,6,12,8,10,8,8,6,6,0,6,100,101};
static const signed char gl_55[] = {0,0,8,0,2,12,100,101};
static const signed char gl_56[] = {2,0,6,0,8,2,8,4,6,6,2,6,0,4,0,2,2,0,100,2,6,0,8,0,10,2,12,6,12,8,10,8,8,6,6,100,101};
static const signed char gl_57[] = {8,6,2,6,0,4,0,2,2,0,6,0,8,2,8,10,6,12,2,12,0,10,100,101};
static const signed char gl_46[] = {4,11,4,12,100,101};
static const signed char gl_58[] = {4,3,4,4,100,4,8,4,9,100,101};
static const signed char gl_45[] = {2,6,6,6,100,101};
static const signed char gl_47[] = {0,12,8,0,100,101};
static const signed char gl_33[] = {4,0,4,8,100,4,11,4,12,100,101};
static const signed char gl_63[] = {0,2,2,0,6,0,8,2,8,4,4,6,4,8,100,4,11,4,12,100,101};
static const signed char gl_44[] = {4,10,2,13,100,101};
static const signed char gl_43[] = {4,3,4,9,100,1,6,7,6,100,101};
static const signed char gl_62[] = {1,0,7,6,1,12,100,101};
static const signed char gl_60[] = {7,0,1,6,7,12,100,101};
static const signed char gl_37[] = {0,0,8,12,100,1,1,2,2,100,6,10,7,11,100,101};
static const signed char *glyphTab[128] = {
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,gl_33,0,0,0,gl_37,0,0,0,0,0,gl_43,gl_44,gl_45,gl_46,gl_47,gl_48,gl_49,gl_50,gl_51,gl_52,gl_53,gl_54,gl_55,gl_56,gl_57,gl_58,0,gl_60,0,gl_62,gl_63,0,gl_65,gl_66,gl_67,gl_68,gl_69,gl_70,gl_71,gl_72,gl_73,gl_74,gl_75,gl_76,gl_77,gl_78,gl_79,gl_80,gl_81,gl_82,gl_83,gl_84,gl_85,gl_86,gl_87,gl_88,gl_89,gl_90,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

static const signed char *glyphOf(int c)
{
    if (c >= 'a' && c <= 'z') c -= 32;
    if (c < 0 || c >= 128) return 0;
    return glyphTab[c];
}

static float textW(const char *s, float sc)
{
    int n = (int)strlen(s);
    return n > 0 ? n * 5.6f * sc - 1.6f * sc : 0.0f;
}

static void drawTextRaw(float x, float y, float sc, unsigned int col, const char *s)
{
    int segs = 0;
    for (const char *p = s; *p; p++) {
        const signed char *g = glyphOf(*p);
        if (!g) continue;
        int i = 0, cnt = 0;
        while (g[i] != 101) {
            if (g[i] == 100) { cnt = 0; i++; continue; }
            cnt++;
            if (cnt >= 2) segs++;
            i += 2;
        }
    }
    if (segs == 0) return;
    V2 *v = (V2 *)sceGuGetMemory(segs * 6 * sizeof(V2));
    int n = 0;
    float th = sc * 0.55f;
    if (th < 1.3f) th = 1.3f;
    float pen = x;
    for (const char *p = s; *p; p++, pen += 5.6f * sc) {
        const signed char *g = glyphOf(*p);
        if (!g) continue;
        int i = 0, cnt = 0;
        float px = 0, py = 0;
        while (g[i] != 101) {
            if (g[i] == 100) { cnt = 0; i++; continue; }
            float cx = pen + g[i] * 0.5f * sc;
            float cy = y + g[i + 1] * 0.5f * sc;
            if (cnt >= 1) {
                float dx = cx - px, dy = cy - py;
                float len = sqrtf(dx * dx + dy * dy);
                if (len < 0.001f) len = 0.001f;
                float ux = dx / len, uy = dy / len;
                float e = th * 0.5f;
                float ax = px - ux * e, ay = py - uy * e;
                float bx = cx + ux * e, by = cy + uy * e;
                float nx = -uy * e, ny = ux * e;
                V2 *q = &v[n];
                q[0].x = ax + nx; q[0].y = ay + ny;
                q[1].x = bx + nx; q[1].y = by + ny;
                q[2].x = bx - nx; q[2].y = by - ny;
                q[3].x = ax + nx; q[3].y = ay + ny;
                q[4].x = bx - nx; q[4].y = by - ny;
                q[5].x = ax - nx; q[5].y = ay - ny;
                for (int k = 0; k < 6; k++) { q[k].color = col; q[k].z = 0; }
                n += 6;
            }
            px = cx; py = cy;
            cnt++;
            i += 2;
        }
    }
    sceGuDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, n, 0, v);
}

static void drawText(float x, float y, float sc, unsigned int col, const char *s)
{
    drawTextRaw(x + sc * 0.35f, y + sc * 0.35f, sc, GU_RGBA(0, 0, 0, 170), s);
    drawTextRaw(x, y, sc, col, s);
}

static void drawTextC(float y, float sc, unsigned int col, const char *s)
{
    drawText((SCR_WIDTH - textW(s, sc)) * 0.5f, y, sc, col, s);
}

/* UI palette */
#define COL_STEEL  GU_RGBA(200, 208, 220, 255)
#define COL_DIM    GU_RGBA(130, 138, 152, 255)
#define COL_RUST   GU_RGBA(236, 124, 62, 255)
#define COL_GOLD   GU_RGBA(250, 205, 90, 255)
#define COL_WHITE  GU_RGBA(255, 255, 255, 255)

/* ================================================================== */
/* 3D scene                                                            */
/* ================================================================== */
static const unsigned int groupCol[4] = {
    GU_RGBA(60, 205, 215, 255), GU_RGBA(225, 95, 205, 255),
    GU_RGBA(135, 225, 75, 255), GU_RGBA(245, 185, 65, 255)
};

#define STONE_COL GU_RGBA(190, 198, 206, 255)
#define PAD_COL   GU_RGBA(170, 90, 235, 255)
#define SPAWN_COL GU_RGBA(80, 215, 225, 255)

static void drawFrameBox(float cx, float cz, float r, float t, float y0, float y1, unsigned int c)
{
    drawBox(cx - r, y0, cz - r, cx + r, y1, cz - r + t, c, TEX_WHITE, PROF_TILE);
    drawBox(cx - r, y0, cz + r - t, cx + r, y1, cz + r, c, TEX_WHITE, PROF_TILE);
    drawBox(cx - r, y0, cz - r + t, cx - r + t, y1, cz + r - t, c, TEX_WHITE, PROF_TILE);
    drawBox(cx + r - t, y0, cz - r + t, cx + r, y1, cz + r - t, c, TEX_WHITE, PROF_TILE);
}

static void drawTileAt(int x, int z, char t, float yoff)
{
    const float gap = 0.015f;
    float x0 = x + gap, x1 = x + 1 - gap, z0 = z + gap, z1 = z + 1 - gap;
    float cx = x + 0.5f, cz = z + 0.5f;

    if (t == 'G') {   /* plain recessed square hole, like the original */
        unsigned int wall = GU_RGBA(96, 88, 88, 255);
        drawBox(x0, -0.31f + yoff, z0, x1, -0.28f + yoff, z1, GU_RGBA(12, 10, 10, 255), TEX_WHITE, PROF_TILE);
        drawBox(x0, -0.28f + yoff, z0, x1, yoff, z0 + 0.03f, wall, TEX_WHITE, PROF_TILE);
        drawBox(x0, -0.28f + yoff, z1 - 0.03f, x1, yoff, z1, wall, TEX_WHITE, PROF_TILE);
        drawBox(x0, -0.28f + yoff, z0, x0 + 0.03f, yoff, z1, wall, TEX_WHITE, PROF_TILE);
        drawBox(x1 - 0.03f, -0.28f + yoff, z0, x1, yoff, z1, wall, TEX_WHITE, PROF_TILE);
        return;
    }

    unsigned int col = STONE_COL;
    int texId = TEX_STONE;
    float hv = (hash2(x * 31 + 7, z * 17 + 3) - 0.5f) * 0.10f;   /* slight per-tile variation */
    col = shade(col, 1.0f + hv);
    if (t == 'F') { col = GU_RGBA(255, 255, 255, 255); texId = TEX_WOOD; }
    else if (t >= '1' && t <= '4') col = mixCol(col, groupCol[t - '1'], 0.38f);
    else if (t == 'T') col = mixCol(col, PAD_COL, 0.30f);

    drawBox(x0, -0.28f + yoff, z0, x1, yoff, z1, col, texId, PROF_TILE);

    if (t >= 'a' && t <= 'x') {
        int idx = t - 'a', kind = idx / 4, g = idx % 4;
        unsigned int gc = groupCol[g];
        int heavy = (kind == 1 || kind == 4 || kind == 5);
        int modeOn = (kind == 2 || kind == 4), modeOff = (kind == 3 || kind == 5);
        if (!heavy) {
            drawBox(cx - 0.27f, yoff, cz - 0.27f, cx + 0.27f, yoff + 0.05f, cz + 0.27f, gc, TEX_STONE, PROF_TILE);
        } else {
            drawBox(cx - 0.33f, yoff, cz - 0.33f, cx + 0.33f, yoff + 0.03f, cz + 0.33f, shade(gc, 0.45f), TEX_WHITE, PROF_TILE);
            drawBox(cx - 0.30f, yoff, cz - 0.07f, cx + 0.30f, yoff + 0.07f, cz + 0.07f, gc, TEX_WHITE, PROF_TILE);
            drawBox(cx - 0.07f, yoff, cz - 0.30f, cx + 0.07f, yoff + 0.07f, cz + 0.30f, gc, TEX_WHITE, PROF_TILE);
        }
        if (modeOn)  drawBox(cx - 0.07f, yoff + 0.05f, cz - 0.07f, cx + 0.07f, yoff + 0.10f, cz + 0.07f, GU_RGBA(250, 250, 250, 255), TEX_WHITE, PROF_TILE);
        if (modeOff) drawBox(cx - 0.07f, yoff + 0.05f, cz - 0.07f, cx + 0.07f, yoff + 0.10f, cz + 0.07f, GU_RGBA(25, 25, 30, 255), TEX_WHITE, PROF_TILE);
    }
    if (t == 'T') {   /* teleport pad: purple plate + ring */
        float pulse = 0.75f + 0.25f * sinf(gTick * 0.08f);
        unsigned int pc = shade(PAD_COL, pulse);
        drawBox(cx - 0.22f, yoff, cz - 0.22f, cx + 0.22f, yoff + 0.04f, cz + 0.22f, pc, TEX_WHITE, PROF_TILE);
        drawFrameBox(cx, cz, 0.38f, 0.06f, yoff, yoff + 0.05f, pc);
    }
    if (t == 'U' || t == 'V') {   /* cube spawn marker */
        drawFrameBox(cx, cz, 0.30f, 0.05f, yoff, yoff + 0.04f, SPAWN_COL);
    }
}

static void blockExtents(Block b, float *x0, float *x1, float *z0, float *z1, float *h)
{
    *x0 = (float)b.x;
    *z0 = (float)b.z;
    *x1 = (float)(b.x + (b.o == 1 ? 2 : 1));
    *z1 = (float)(b.z + (b.o == 2 ? 2 : 1));
    *h  = (b.o == 0) ? 2.0f : 1.0f;
}

/* box that rolls a quarter turn around its bottom edge */
static void drawRollingBox(float x0, float x1, float z0, float z1, float h, float a)
{
    ScePspFVector3 p = { 0.0f, 0.0f, 0.0f };
    modelIdentity();
    if (rollDX == 1)       { p.x = x1; sceGumTranslate(&p); sceGumRotateZ(-a); }
    else if (rollDX == -1) { p.x = x0; sceGumTranslate(&p); sceGumRotateZ( a); }
    else if (rollDZ == 1)  { p.z = z1; sceGumTranslate(&p); sceGumRotateX( a); }
    else                   { p.z = z0; sceGumTranslate(&p); sceGumRotateX(-a); }
    gAllFaces = 1;
    drawBox(x0 - p.x, 0.002f, z0 - p.z, x1 - p.x, h, z1 - p.z, GU_RGBA(255, 255, 255, 255), TEX_RUST, PROF_BLOCK);
    gAllFaces = 0;
}

static float rollAngle(void)
{
    float t = anim > 1.0f ? 1.0f : anim;
    float e = 0.5f * t + 0.5f * (t * t * (3.0f - 2.0f * t));
    return e * PI_F * 0.5f;
}

static void drawSolid(float x0, float x1, float z0, float z1, float h, float yoff)
{
    modelIdentity();
    drawBox(x0, 0.002f + yoff, z0, x1, h + yoff, z1, GU_RGBA(255, 255, 255, 255), TEX_RUST, PROF_BLOCK);
}

static void drawCubeMarker(float cx, float cz, float yBase)
{
    float bob = 0.12f * sinf(gTick * 0.12f);
    modelIdentity();
    drawBox(cx - 0.12f, yBase + 0.35f + bob, cz - 0.12f, cx + 0.12f, yBase + 0.55f + bob, cz + 0.12f,
            GU_RGBA(250, 205, 70, 255), TEX_WHITE, PROF_TILE);
}

static void drawPlayer(void)
{
    float x0, x1, z0, z1, h;

    if (state == ST_INTRO) {
        if (introT < 1.35f) return;
        float k = clampf((introT - 1.35f) / 0.27f, 0.0f, 1.0f);
        float yoff = (1.0f - k) * (1.0f - k) * 10.0f;
        blockExtents(blk, &x0, &x1, &z0, &z1, &h);
        drawSolid(x0, x1, z0, z1, h, yoff);
        return;
    }

    if (!split) {
        if (state == ST_ROLL) {
            blockExtents(prevBlk, &x0, &x1, &z0, &z1, &h);
            drawRollingBox(x0, x1, z0, z1, h, rollAngle());
            return;
        }
        float yoff = 0.0f;
        if (state == ST_FALL) yoff = -(anim * anim) * 25.0f;
        if (state == ST_WIN)  yoff = -anim * 2.8f;
        blockExtents(blk, &x0, &x1, &z0, &z1, &h);
        drawSolid(x0, x1, z0, z1, h, yoff);
        return;
    }

    /* split: two cubes */
    for (int i = 0; i < 2; i++) {
        float fx = (float)cubeX[i], fz = (float)cubeZ[i];
        if (state == ST_ROLL && rollCube == i) {
            drawRollingBox((float)prevCX, (float)prevCX + 1.0f, (float)prevCZ, (float)prevCZ + 1.0f, 1.0f, rollAngle());
        } else {
            float yoff = 0.0f;
            if (state == ST_FALL && fallCube == i) yoff = -(anim * anim) * 25.0f;
            if (spawnT < 1.0f) yoff += (1.0f - spawnT) * (1.0f - spawnT) * 7.0f;
            drawSolid(fx, fx + 1.0f, fz, fz + 1.0f, 1.0f, yoff);
        }
    }
    if (state == ST_IDLE || state == ST_ROLL) {
        float mx = (float)cubeX[activeCube] + 0.5f, mz = (float)cubeZ[activeCube] + 0.5f;
        float my = 1.0f;
        if (state == ST_ROLL) { mx = (float)cubeX[activeCube] + 0.5f; mz = (float)cubeZ[activeCube] + 0.5f; }
        drawCubeMarker(mx, mz, my);
    }
}

/* soft shadow cast to the right of whatever stands on the floor */
static void shadowFor(float x0, float x1, float z0, float z1, float h)
{
    float sx0 = x0 + 0.05f, sx1 = x1 + 0.28f * h + 0.10f;
    float sz0 = z0 + 0.04f, sz1 = z1 + 0.05f;
    for (int z = (int)sz0; z <= (int)sz1 && z < mapH; z++)
        for (int x = (int)sx0; x <= (int)sx1 && x < mapW; x++) {
            char t = tileAt(x, z);
            if (t == '.' || t == 'G' || t == 0) continue;
            if (t >= '1' && t <= '4' && !tilePresent(t)) continue;
            float ax = sx0 > x ? sx0 : (float)x, bx = sx1 < x + 1 ? sx1 : (float)(x + 1);
            float az = sz0 > z ? sz0 : (float)z, bz = sz1 < z + 1 ? sz1 : (float)(z + 1);
            if (bx - ax < 0.01f || bz - az < 0.01f) continue;
            drawFlat(ax, az, bx, bz, 0.006f, GU_RGBA(0, 0, 0, 95));
        }
}

static void drawShadows(void)
{
    if (state != ST_IDLE && !(state == ST_INTRO && introT >= 1.65f)) return;
    modelIdentity();
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuDepthMask(GU_TRUE);
    if (!split) {
        float x0, x1, z0, z1, h;
        blockExtents(blk, &x0, &x1, &z0, &z1, &h);
        shadowFor(x0, x1, z0, z1, h);
    } else {
        for (int i = 0; i < 2; i++)
            shadowFor((float)cubeX[i], (float)cubeX[i] + 1.0f, (float)cubeZ[i], (float)cubeZ[i] + 1.0f, 1.0f);
    }
    sceGuDepthMask(0);
    sceGuDisable(GU_BLEND);
}

static void drawWorld(void)
{
    modelIdentity();
    for (int z = 0; z < mapH; z++) {
        for (int x = 0; x < mapW; x++) {
            char t = map[z][x];
            if (t == '.') continue;
            float yoff = 0.0f;

            if (state == ST_INTRO) {
                float d = (x + z) * 0.035f;
                float k = clampf((introT - d) / 0.45f, 0.0f, 1.0f);
                yoff = (1.0f - k) * (1.0f - k) * -14.0f;
            }
            if (t >= '1' && t <= '4') {
                float a = bridgeAnim[t - '1'];
                if (a < 0.02f) continue;
                yoff += -(1.0f - a) * (1.0f - a) * 1.2f - (1.0f - a) * 0.6f;
            }
            if (state == ST_FALL && fellOnFragile && t == 'F' && !split && blk.o == 0 && blk.x == x && blk.z == z)
                yoff = -(anim * anim) * 25.0f;
            drawTileAt(x, z, t, yoff);
        }
    }
}

/* camera: angled view, auto-fitted to the real tiles of the level */
#define CAM_PITCH 0.80f
#define CAM_YAW   0.46f
#define CAM_FOVY  50.0f
#define CAM_TY   (-0.35f)

static float camCX, camCZ, camDist = 14.0f;

static void computeCamera(void)
{
    int minx = MAXW, maxx = -1, minz = MAXH, maxz = -1;
    for (int z = 0; z < mapH; z++)
        for (int x = 0; x < mapW; x++)
            if (map[z][x] != '.') {
                if (x < minx) minx = x;
                if (x > maxx) maxx = x;
                if (z < minz) minz = z;
                if (z > maxz) maxz = z;
            }
    if (maxx < 0) { minx = minz = 0; maxx = maxz = 0; }
    camCX = (minx + maxx + 1) * 0.5f;
    camCZ = (minz + maxz + 1) * 0.5f;

    float hy = tanf(CAM_FOVY * 0.5f * PI_F / 180.0f);
    float hx = hy * 16.0f / 9.0f;
    float cyaw = cosf(CAM_YAW), syaw = sinf(CAM_YAW);
    float cp = cosf(CAM_PITCH), sp = sinf(CAM_PITCH);
    float dist = 4.0f;
    for (int z = 0; z < mapH; z++) {
        for (int x = 0; x < mapW; x++) {
            if (map[z][x] == '.') continue;
            for (int i = 0; i < 8; i++) {
                float px = (float)(x + (i & 1)) - camCX;
                float py = (i & 2) ? 2.0f : -0.3f;
                float pz = (float)(z + ((i >> 2) & 1)) - camCZ;
                float x1 = px * cyaw + pz * syaw;
                float z1 = -px * syaw + pz * cyaw;
                float y2 = py * cp - z1 * sp + CAM_TY;
                float z2 = py * sp + z1 * cp;
                float needX = z2 + fabsf(x1) / (0.94f * hx);
                float needY = (y2 > 0.0f) ? z2 + y2 / (0.66f * hy) : z2 + (-y2) / (0.86f * hy);
                if (needX > dist) dist = needX;
                if (needY > dist) dist = needY;
            }
        }
    }
    camDist = dist;
}

static void setupCameraGame(void)
{
    float swoop = 0.0f;
    if (state == ST_INTRO) {
        float k = clampf(introT / 1.7f, 0.0f, 1.0f);
        swoop = (1.0f - k) * (1.0f - k) * 5.0f;
    }

    sceGumMatrixMode(GU_PROJECTION);
    sceGumLoadIdentity();
    sceGumPerspective(CAM_FOVY, 16.0f / 9.0f, 0.5f, 200.0f);

    sceGumMatrixMode(GU_VIEW);
    sceGumLoadIdentity();
    ScePspFVector3 t1 = { 0.0f, CAM_TY, -(camDist + swoop) };
    ScePspFVector3 t2 = { -camCX, 0.0f, -camCZ };
    sceGumTranslate(&t1);
    sceGumRotateX(CAM_PITCH + swoop * 0.04f);
    sceGumRotateY(CAM_YAW);
    sceGumTranslate(&t2);

    modelIdentity();
    sceGumUpdateMatrix();
}

/* ---- menu backdrop scene ---- */
static const char *menuRows[3] = { "#####", "#####", "####G" };

static void drawMenuScene(void)
{
    float t = gTick / 60.0f;
    sceGumMatrixMode(GU_PROJECTION);
    sceGumLoadIdentity();
    sceGumPerspective(50.0f, 16.0f / 9.0f, 0.5f, 200.0f);

    sceGumMatrixMode(GU_VIEW);
    sceGumLoadIdentity();
    ScePspFVector3 t1 = { 4.0f, -1.2f, -11.5f };
    ScePspFVector3 t2 = { -2.5f, 0.0f, -1.5f };
    sceGumTranslate(&t1);
    sceGumRotateX(0.62f);
    sceGumRotateY(CAM_YAW + sinf(t * 0.35f) * 0.25f);
    sceGumTranslate(&t2);
    modelIdentity();
    sceGumUpdateMatrix();

    for (int z = 0; z < 3; z++)
        for (int x = 0; x < 5; x++) {
            float bob = sinf(t * 1.2f + x * 0.7f + z * 0.5f) * 0.10f;
            drawTileAt(x, z, menuRows[z][x], bob);
        }

    float bob = sinf(t * 1.2f + 2 * 0.7f + 1 * 0.5f) * 0.10f;
    ScePspFVector3 p = { 1.5f, bob, 1.5f };
    modelIdentity();
    sceGumTranslate(&p);
    sceGumRotateY(t * 0.7f);
    gAllFaces = 1;
    drawBox(-0.5f, 0.002f, -0.5f, 0.5f, 2.0f, 0.5f, GU_RGBA(255, 255, 255, 255), TEX_RUST, PROF_BLOCK);
    gAllFaces = 0;
}

/* ================================================================== */
/* HUD and screens                                                     */
/* ================================================================== */
static void menuItem(float y, int selected, const char *label, float sc)
{
    float w = textW(label, sc);
    float x = (SCR_WIDTH - w) * 0.5f;
    if (selected) {
        float pulse = 0.5f + 0.5f * sinf(gTick * 0.12f);
        unsigned a = (unsigned)(60 + 50 * pulse);
        rect(x - 26, y - 5, w + 52, 6 * sc + 10, GU_RGBA(236, 124, 62, a));
        drawText(x - 22, y, sc, COL_RUST, ">");
        drawText(x + w + 6, y, sc, COL_RUST, "<");
        drawText(x, y, sc, COL_WHITE, label);
    } else {
        drawText(x, y, sc, COL_DIM, label);
    }
}

static void menuItemL(float x, float y, int selected, const char *label, float sc)
{
    float w = textW(label, sc);
    if (selected) {
        float pulse = 0.5f + 0.5f * sinf(gTick * 0.12f);
        unsigned a = (unsigned)(60 + 50 * pulse);
        rect(x - 14, y - 5, w + 28, 6 * sc + 10, GU_RGBA(236, 124, 62, a));
        drawText(x - 30, y, sc, COL_RUST, ">");
        drawText(x, y, sc, COL_WHITE, label);
    } else {
        drawText(x, y, sc, COL_DIM, label);
    }
}

static void updateMenu(void)
{
    if (dirEdge[0]) { menuSel = (menuSel + 4) % 5; sfx(SFX_MENU); }
    if (dirEdge[1]) { menuSel = (menuSel + 1) % 5; sfx(SFX_MENU); }
    if (btnPressed & CONFIRM_BTN) {
        sfx(SFX_OK);
        switch (menuSel) {
        case 0: startLevel(save.unlocked); break;
        case 1: selSel = save.unlocked; gotoScreen(SCR_SELECT); break;
        case 2: helpPage = 0; gotoScreen(SCR_HELP); break;
        case 3: save.sound = !save.sound; setSound(save.sound); saveWrite(); sfx(SFX_OK); break;
        case 4: showFps = !showFps; break;
        }
    }
}

static void drawMenu(void)
{
    float glow = 0.5f + 0.5f * sinf(gTick * 0.04f);
    drawTextC(14, 10.5f, GU_RGBA(210, (unsigned)(150 + 40 * glow), (unsigned)(120 + 30 * glow), 255), "BLOXORZ");
    drawTextC(84, 2.4f, COL_DIM, "PSP EDITION");

    char buf[40];
    menuItemL(46, 116, menuSel == 0, save.unlocked > 0 ? "CONTINUE" : "PLAY", 2.6f);
    menuItemL(46, 148, menuSel == 1, "LEVEL SELECT", 2.6f);
    menuItemL(46, 180, menuSel == 2, "HOW TO PLAY", 2.6f);
    snprintf(buf, sizeof(buf), "SOUND: %s", save.sound ? "ON" : "OFF");
    menuItemL(46, 212, menuSel == 3, buf, 2.6f);
    snprintf(buf, sizeof(buf), "FPS DISPLAY: %s", showFps ? "ON" : "OFF");
    menuItemL(46, 244, menuSel == 4, buf, 2.6f);
}

#define SEL_COLS 10
#define SEL_ROWS 5
#define SEL_PAGE (SEL_COLS * SEL_ROWS)

static void updateSelect(void)
{
    if (dirEdge[2] && selSel > 0)                    { selSel--; sfx(SFX_MENU); }
    if (dirEdge[3] && selSel + 1 < NUM_LEVELS)       { selSel++; sfx(SFX_MENU); }
    if (dirEdge[0] && selSel - SEL_COLS >= 0)        { selSel -= SEL_COLS; sfx(SFX_MENU); }
    if (dirEdge[1] && selSel + SEL_COLS < NUM_LEVELS){ selSel += SEL_COLS; sfx(SFX_MENU); }
    if ((btnPressed & PSP_CTRL_LTRIGGER) && selSel >= SEL_PAGE)           { selSel -= SEL_PAGE; sfx(SFX_MENU); }
    if ((btnPressed & PSP_CTRL_RTRIGGER) && selSel + SEL_PAGE < NUM_LEVELS){ selSel += SEL_PAGE; sfx(SFX_MENU); }
    if (btnPressed & BACK_BTN) { sfx(SFX_BACK); menuSel = 1; gotoScreen(SCR_MENU); return; }
    if (btnPressed & (PSP_CTRL_CROSS | PSP_CTRL_CIRCLE | PSP_CTRL_START)) {
        if (selSel <= save.unlocked) { sfx(SFX_OK); totalMoves = 0; loadLevel(selSel, 0); gotoScreen(SCR_GAME); }
        else sfx(SFX_BACK);
    }
}

static void drawSelect(void)
{
    int page = selSel / SEL_PAGE;
    int pages = (NUM_LEVELS + SEL_PAGE - 1) / SEL_PAGE;
    char buf[32];
    drawTextC(8, 3.0f, COL_RUST, "SELECT LEVEL");
    snprintf(buf, sizeof(buf), "PAGE %d/%d  (L/R)", page + 1, pages);
    drawText(SCR_WIDTH - 12 - textW(buf, 1.5f), 14, 1.5f, COL_DIM, buf);

    const float cw = 42, ch = 34, gap = 4;
    float gx = (SCR_WIDTH - (SEL_COLS * cw + (SEL_COLS - 1) * gap)) * 0.5f;
    float gy = 36;
    for (int k = 0; k < SEL_PAGE; k++) {
        int i = page * SEL_PAGE + k;
        if (i >= NUM_LEVELS) break;
        int cx = k % SEL_COLS, cy = k / SEL_COLS;
        float x = gx + cx * (cw + gap), y = gy + cy * (ch + gap);
        int unlocked = i <= save.unlocked;
        unsigned bg = unlocked ? GU_RGBA(52, 60, 78, 225) : GU_RGBA(24, 24, 30, 220);
        if (save.best[i] > 0) bg = GU_RGBA(78, 52, 44, 235);
        rect(x, y, cw, ch, bg);
        if (i == selSel) {
            float pulse = 0.5f + 0.5f * sinf(gTick * 0.15f);
            frame2D(x - 2, y - 2, cw + 4, ch + 4, 2.5f, GU_RGBA(250, 205, 90, (unsigned)(180 + 75 * pulse)));
        }
        snprintf(buf, sizeof(buf), "%d", i + 1);
        unsigned tc = unlocked ? COL_WHITE : GU_RGBA(95, 100, 118, 255);
        drawTextRaw(x + (cw - textW(buf, 2.1f)) * 0.5f, y + 4, 2.1f, tc, buf);
        if (save.best[i] > 0) {
            snprintf(buf, sizeof(buf), "%d", save.best[i]);
            drawTextRaw(x + (cw - textW(buf, 1.3f)) * 0.5f, y + 22, 1.3f, COL_GOLD, buf);
        }
    }
    snprintf(buf, sizeof(buf), "LEVEL %d", selSel + 1);
    drawText(12, 248, 2.0f, COL_STEEL, buf);
    drawText(130, 248, 2.0f, COL_DIM, LEVELS[selSel].name);
    if (selSel > save.unlocked) drawText(SCR_WIDTH - 12 - textW("LOCKED", 2.0f), 248, 2.0f, COL_RUST, "LOCKED");
}

static void updateHelp(void)
{
    if (dirEdge[2] && helpPage > 0)              { helpPage--; sfx(SFX_MENU); }
    if (dirEdge[3] && helpPage < HELP_PAGES - 1) { helpPage++; sfx(SFX_MENU); }
    if (btnPressed & (CONFIRM_BTN | BACK_BTN)) { sfx(SFX_BACK); menuSel = 2; gotoScreen(SCR_MENU); }
}

static void swatch(float x, float y, unsigned int col)
{
    rect(x, y, 16, 16, col);
    frame2D(x, y, 16, 16, 1.2f, GU_RGBA(255, 255, 255, 120));
}

static void helpLine(float *y, const char *s, unsigned col)
{
    drawText(24, *y, 1.9f, col, s);
    *y += 20;
}

static void helpItem(float *y, unsigned swatchCol, const char *s)
{
    swatch(24, *y - 2, swatchCol);
    drawText(50, *y, 1.9f, COL_STEEL, s);
    *y += 24;
}

static void drawHelp(void)
{
    drawTextC(8, 3.0f, COL_RUST, "HOW TO PLAY");
    float y = 44;
    if (helpPage == 0) {
        helpLine(&y, "ROLL THE BLOCK ACROSS THE TILES.", COL_STEEL);
        helpLine(&y, "STAND IT UPRIGHT ON THE SQUARE HOLE", COL_STEEL);
        helpLine(&y, "TO COMPLETE THE LEVEL.", COL_STEEL);
        helpLine(&y, "FALL OFF AN EDGE AND YOU RESTART.", COL_STEEL);
        y += 10;
        helpItem(&y, GU_RGBA(190, 198, 206, 255), "NORMAL TILE");
        helpItem(&y, GU_RGBA(220, 150, 88, 255), "CRACKED: BREAKS IF YOU STAND UP");
        helpItem(&y, GU_RGBA(20, 16, 16, 255), "HOLE: ONLY THE UPRIGHT BLOCK FITS");
    } else if (helpPage == 1) {
        helpItem(&y, GU_RGBA(120, 130, 150, 255), "SQUARE SWITCH: ANY TOUCH WORKS");
        rect(30, y - 20, 4, 4, groupCol[1]);
        helpItem(&y, GU_RGBA(120, 130, 150, 255), "CROSS SWITCH: ONLY STANDING UP");
        rect(29, y - 17, 6, 2, groupCol[1]); rect(31, y - 19, 2, 6, groupCol[1]);
        helpItem(&y, groupCol[0], "BRIDGE: LINKED TO ITS COLOR");
        y += 6;
        helpLine(&y, "TOGGLE SWITCHES FLIP A BRIDGE.", COL_STEEL);
        helpLine(&y, "A WHITE DOT: ONLY OPENS BRIDGES.", COL_STEEL);
        helpLine(&y, "A BLACK DOT: ONLY CLOSES THEM.", COL_STEEL);
    } else {
        helpItem(&y, PAD_COL, "PAD: STAND UP ON IT TO SPLIT");
        helpLine(&y, "THE BLOCK BECOMES TWO SMALL CUBES.", COL_STEEL);
        helpLine(&y, "L OR R SWAPS THE ACTIVE CUBE.", COL_STEEL);
        helpLine(&y, "CUBES JOIN WHEN THEY TOUCH.", COL_STEEL);
        y += 8;
        helpLine(&y, "D-PAD / STICK / FACE BUTTONS: ROLL", COL_GOLD);
        helpLine(&y, "START: PAUSE     SELECT: RESTART", COL_GOLD);
    }
    char buf[24];
    snprintf(buf, sizeof(buf), "< PAGE %d/%d >", helpPage + 1, HELP_PAGES);
    drawTextC(246, 2.0f, COL_DIM, buf);
}

static void drawGameHud(void)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "LEVEL %d", level + 1);
    drawText(10, 8, 2.4f, COL_WHITE, buf);
    drawText(10, 28, 1.5f, COL_DIM, LEVELS[level].name);
    snprintf(buf, sizeof(buf), "MOVES %d", moves);
    drawText(SCR_WIDTH - 10 - textW(buf, 2.4f), 8, 2.4f, COL_GOLD, buf);
    if (save.best[level] > 0) {
        snprintf(buf, sizeof(buf), "BEST %d", save.best[level]);
        drawText(SCR_WIDTH - 10 - textW(buf, 1.6f), 29, 1.6f, COL_DIM, buf);
    }

    const char *hint = LEVELS[level].hint;
    if (hint && hint[0] && (state == ST_IDLE || state == ST_INTRO) && moves < 2 && !paused) {
        float w = textW(hint, 1.6f);
        rect((SCR_WIDTH - w) * 0.5f - 10, 238, w + 20, 22, GU_RGBA(0, 0, 0, 170));
        drawTextC(243, 1.6f, COL_STEEL, hint);
    }
    if (split && state != ST_WIN && state != ST_FALL)
        drawText(10, 250, 1.6f, COL_GOLD, "L/R: SWAP CUBE");

    if (state == ST_FALL && anim > 0.25f) {
        float a = clampf((anim - 0.25f) * 3.0f, 0.0f, 1.0f);
        drawTextC(110, 7.0f, GU_RGBA(236, 90, 70, (unsigned)(255 * a)), "OOPS!");
    }
    if (state == ST_WIN) {
        float a = clampf(anim * 4.0f, 0.0f, 1.0f);
        drawTextC(100, 6.0f, GU_RGBA(250, 205, 90, (unsigned)(255 * a)), "COMPLETE!");
        snprintf(buf, sizeof(buf), "%d MOVES", moves);
        drawTextC(150, 3.0f, GU_RGBA(255, 255, 255, (unsigned)(255 * a)), buf);
    }
}

static void drawPause(void)
{
    rect(0, 0, SCR_WIDTH, SCR_HEIGHT, GU_RGBA(0, 0, 0, 160));
    rect(120, 22, 240, 228, GU_RGBA(24, 26, 34, 240));
    frame2D(120, 22, 240, 228, 2.0f, COL_RUST);
    drawTextC(32, 3.4f, COL_RUST, "PAUSED");
    char buf[32];
    snprintf(buf, sizeof(buf), "SOUND: %s", save.sound ? "ON" : "OFF");
    const char *items[PAUSE_ITEMS] = { "RESUME", "RESTART", "SKIP LEVEL", "LEVEL SELECT", "MAIN MENU", buf };
    for (int i = 0; i < PAUSE_ITEMS; i++) menuItem(76 + i * 28, pauseSel == i, items[i], 2.4f);
}

static void updateEnd(void)
{
    if (btnPressed & (CONFIRM_BTN | BACK_BTN)) { sfx(SFX_OK); menuSel = 0; gotoScreen(SCR_MENU); }
}

static void drawEnd(void)
{
    float glow = 0.5f + 0.5f * sinf(gTick * 0.06f);
    drawTextC(40, 5.0f, GU_RGBA(240, (unsigned)(150 + 50 * glow), 90, 255), "CONGRATULATIONS!");
    char buf[64];
    snprintf(buf, sizeof(buf), "YOU FINISHED ALL %d LEVELS", NUM_LEVELS);
    drawTextC(110, 2.8f, COL_WHITE, buf);
    int sum = 0;
    for (int i = 0; i < NUM_LEVELS; i++) if (save.best[i] > 0) sum += save.best[i];
    snprintf(buf, sizeof(buf), "BEST MOVES TOTAL: %d", sum);
    drawTextC(150, 2.4f, COL_GOLD, buf);
    drawTextC(230, 2.2f, COL_DIM, "PRESS X TO RETURN TO THE MENU");
}

/* ================================================================== */
/* Frame                                                               */
/* ================================================================== */
static void renderFrame(void)
{
    sceGuStart(GU_DIRECT, list);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuClearColor(GU_RGBA(0, 0, 0, 255));       /* pure black background */
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

    if (screen == SCR_MENU) {
        begin3D();
        drawMenuScene();
    } else if (screen == SCR_GAME) {
        begin3D();
        setupCameraGame();
        drawWorld();
        drawShadows();
        drawPlayer();
    }

    begin2D();
    switch (screen) {
    case SCR_MENU:   drawMenu(); break;
    case SCR_SELECT: drawSelect(); break;
    case SCR_HELP:   drawHelp(); break;
    case SCR_GAME:   drawGameHud(); if (paused) drawPause(); break;
    case SCR_END:    drawEnd(); break;
    }
    if (showFps) {
        char fb[24];
        snprintf(fb, sizeof(fb), "%d FPS", (int)(fpsShown + 0.5f));
        drawText(SCR_WIDTH - 10 - textW(fb, 1.7f), 256, 1.7f, fpsShown >= 58.0f ? GU_RGBA(120, 230, 120, 255) : GU_RGBA(240, 120, 90, 255), fb);
    }
    if (fade > 0.0f) rect(0, 0, SCR_WIDTH, SCR_HEIGHT, GU_RGBA(0, 0, 0, (unsigned)(255 * fade)));

    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

static void initGu(void)
{
    sceGuInit();
    sceGuStart(GU_DIRECT, list);
    sceGuDrawBuffer(GU_PSM_8888, (void *)0, BUF_WIDTH);
    sceGuDispBuffer(SCR_WIDTH, SCR_HEIGHT, (void *)FB1_OFFSET, BUF_WIDTH);
    sceGuDepthBuffer((void *)ZB_OFFSET, BUF_WIDTH);
    sceGuOffset(2048 - (SCR_WIDTH / 2), 2048 - (SCR_HEIGHT / 2));
    sceGuViewport(2048, 2048, SCR_WIDTH, SCR_HEIGHT);
    sceGuDepthRange(0xc350, 0x2710);
    sceGuScissor(0, 0, SCR_WIDTH, SCR_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDepthFunc(GU_GEQUAL);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuFrontFace(GU_CW);
    sceGuShadeModel(GU_SMOOTH);
    sceGuEnable(GU_CLIP_PLANES);
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

int main(int argc, char **argv)
{
    setup_callbacks();
    scePowerSetClockFrequency(333, 333, 166);   /* full speed: CPU 333, bus 166 */
    saveInitPath(argc > 0 ? argv[0] : NULL);
    initGu();
    makeTextures();

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    saveLoad();
    audioInit();

    level = 0;
    loadLevel(0, 0);
    screen = SCR_MENU;
    menuSel = 0;
    waitRelease = 1;

    fpsLast = sceKernelGetSystemTimeLow();
    for (;;) {
        gTick++;
        fpsFrames++;
        unsigned now = sceKernelGetSystemTimeLow();
        if (now - fpsLast >= 1000000u) {
            fpsShown = (float)fpsFrames * 1000000.0f / (float)(now - fpsLast);
            fpsFrames = 0;
            fpsLast = now;
        }
        if (fade > 0.0f) { fade -= 0.06f; if (fade < 0.0f) fade = 0.0f; }

        pollInput(screen == SCR_GAME && !paused);

        switch (screen) {
        case SCR_MENU:   updateMenu(); break;
        case SCR_SELECT: updateSelect(); break;
        case SCR_HELP:   updateHelp(); break;
        case SCR_GAME:
            gameInput();
            if (!paused) updateGame();
            break;
        case SCR_END:    updateEnd(); break;
        }

        renderFrame();
    }
    return 0;
}
