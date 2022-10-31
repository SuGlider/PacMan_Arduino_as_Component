/******************************************************************************/
/*                                                                            */
/*  PACMAN GAME FOR ARDUINO DUE                                               */
/*                                                                            */
/******************************************************************************/
/*  Copyright (c) 2014  Dr. NCX (mirracle.mxx@gmail.com)                      */
/*                                                                            */
/* THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL              */
/* WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED              */
/* WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR    */
/* BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES      */
/* OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,     */
/* WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,     */
/* ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS        */
/* SOFTWARE.                                                                  */
/*                                                                            */
/*  MIT license, all text above must be included in any redistribution.       */


/*
    Controller configuration:
    Buttons UP, RIGHT, DOWN, LEFT, PAUSE and RESTART are each assigned on characters '8', '6', '2', '4', 'x', 'z' in the both case of SerialPort and WiFi UDP.
*/

/******************************************************************************/
/*   MAIN GAME VARIABLES                                                      */
/******************************************************************************/

#include "Arduino.h"

//#include <stdio.h>
//#include <string.h>
//#include "sdkconfig.h"

//#include "freertos/FreeRTOS.h"
//#include "freertos/semphr.h"

//#include "esp_heap_caps.h"
//#include "esp_log.h"

//#include "driver/gpio.h"
//#include "driver/i2c.h"
#include "board.h"
#if 0
#include "lcd.h"
#if (BOARD_DISP_TOUCH_CONTROLLER == BOARD_DISP_TOUCH_GT911)
#include "esp_lcd_touch_gt911.h"
#elif (BOARD_DISP_TOUCH_CONTROLLER == BOARD_DISP_TOUCH_TT21100)
#include "esp_lcd_touch_tt21100.h"
#elif (BOARD_DISP_TOUCH_CONTROLLER == BOARD_DISP_TOUCH_FT5X06)
#include "esp_lcd_touch_ft5x06.h"
#endif
#endif

#include "bsp.h"
#if(BOARD_TYPE == BOARD_TYPE_HMI)
#include "Game_Audio.h"
#include "SoundData.h"
#endif
#include "TFT_16bits.h"

#if 0
#if !CONFIG_IDF_TARGET_ESP32S2
#error This Sketch is inteded to run on ESP32-S2 HMI Devkit - Select "ESP32S2 Dev Module" board.
#endif

#if !CONFIG_SPIRAM
#error This sketch MUST use PSRAM, please enable it in the IDE Menu
#endif
#endif

#define SCR_WIDTH     BOARD_DISP_TOUCH_HRES
#define SCR_HEIGHT    BOARD_DISP_TOUCH_VRES

#define GPIO_DAC_OUT (GPIO_NUM_18)   // ESP32S2 DAC is 17/18 | ESP32 is 25/26 | ESP32S3 has NO DAC... DeltaSigma needed to generate a PDM waveform

void drawIndexedmap(uint8_t* indexmap, uint16_t x, uint16_t y);
void ClearKeys();
void drawButtonFace(uint8_t btId);
void  bsp_lcd_flush(int x0, int y0, int x1, int y1, void *pixels);

// creates a GFX object for drawing and allocating PSRAM for Screen Buffer
TFT_16bits tft16bits(SCR_WIDTH, SCR_HEIGHT);

#if(BOARD_TYPE == BOARD_TYPE_HMI)
Game_Audio_Class GameAudio(GPIO_DAC_OUT, 0); // GPIO25/26 ESP32 || GPIO17/18 ESP32-S2 -- TIMER number 1
Game_Audio_Wav_Class pmDeath(pacmanDeath); // pacman dyingsound
Game_Audio_Wav_Class pmWav(pacman); // pacman theme
Game_Audio_Wav_Class pmChomp(chomp); // pacman chomp
Game_Audio_Wav_Class pmEatGhost(pacman_eatghost); // pacman theme
#endif

#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

#if (BOARD_DISP_PARALLEL_CONTROLLER == BOARD_DISP_LCD_RM68120)
// R5 G6 B5 for RM68120
#define C16(_rr,_gg,_bb) ((uint16_t)(((_rr & 0xF8) << 8) | ((_gg & 0xFC) << 3) | ((_bb & 0xF8) >> 3)))
#elif (BOARD_DISP_PARALLEL_CONTROLLER == BOARD_DISP_LCD_RA8875)
// B5 R5 G6 for RA8875
#define C16(_rr,_gg,_bb) ((uint16_t)(((_bb & 0xF8) << 8) | ((_rr & 0xF8) << 3) | ((_gg & 0xFC) >> 2)))
#endif

const uint16_t _paletteW[16] =
{
  C16(0, 0, 0),
  C16(255, 0, 0),      // 1 red
  C16(222, 151, 81),   // 2 brown
  C16(255, 0, 255),    // 3 pink

  C16(0, 0, 0),
  C16(0, 255, 255),    // 5 cyan
  C16(71, 84, 255),    // 6 mid blue
  C16(255, 184, 81),   // 7 lt brown

  C16(0, 0, 0),
  C16(255, 255, 0),    // 9 yellow
  C16(0, 0, 0),
  C16(33, 33, 255),    // 11 blue

  C16(0, 255, 0),      // 12 green
  C16(71, 84, 174),    // 13 aqua
  C16(255, 184, 174),  // 14 lt pink
  C16(222, 222, 255),  // 15 whiteish
};

/*
uint8_t colors[] = {
  0,   0,   0,
  255, 0,   0,
  222, 151, 81,
  255, 0,   255,
  0,   0,   0,
  0,   255, 255,
  71,  84,  255,
  255, 184, 81,
  0,   0,   0,
  255, 255, 0,
  0,   0,   0,
  33,  33,  255,
  0,   255, 0,
  71,  84,  174,
  255, 184, 174,
  222, 222, 255,
};
*/

uint8_t SPEED = 2; // 1=SLOW 2=NORMAL 4=FAST //do not try  other values!!!


#define BONUS_INACTIVE_TIME 600
#define BONUS_ACTIVE_TIME 300

#define START_LIFES 2
#define START_LEVEL 1

uint8_t MAXLIFES = 5;
uint8_t LIFES = START_LIFES;
uint8_t GAMEWIN = 0;
uint8_t GAMEOVER = 0;
uint8_t DEMO = 1;
uint8_t LEVEL = START_LEVEL;
uint8_t ACTUALBONUS = 0;  //actual bonus icon
uint8_t ACTIVEBONUS = 0;  //status of bonus
uint8_t GAMEPAUSED = 0;

uint8_t PACMANFALLBACK = 0;

/******************************************************************************/
/*   Controll KEYPAD LOOP                                                     */
/******************************************************************************/

boolean but_A = false;        // START | PAUSE
boolean but_B = false;        // NOT USED HERE... would be for reseting the game
boolean but_UP = false;       // Indicates to go UP
boolean but_DOWN = false;     // Indicates to go DOWN
boolean but_LEFT = false;     // Indicates to go LEFT
boolean but_RIGHT = false;    // Indicates to go RIGHT

/******************************************************************************/
/*   GAME VARIABLES AND DEFINITIONS                                           */
/******************************************************************************/

#include "pacmanTiles.h"

enum GameState {
  ReadyState,
  PlayState,
  DeadGhostState, // Player got a ghost, show score sprite and only move eyes
  DeadPlayerState,
  EndLevelState
};

enum SpriteState
{
  PenState,
  RunState,
  FrightenedState,
  DeadNumberState,
  DeadEyesState,
  AteDotState,    // pacman
  DeadPacmanState
};

enum {
  MStopped = 0,
  MRight = 1,
  MDown = 2,
  MLeft = 3,
  MUp = 4,
};

#define ushort uint16_t

#define BINKY 0
#define PINKY 1
#define INKY  2
#define CLYDE 3
#define PACMAN 4
#define BONUS 5

const uint8_t _initSprites[] =
{
  BINKY,  14,     17 - 3,  31, MLeft,
  PINKY,  14 - 2, 17,      79, MLeft,
  INKY,   14,     17,     137, MLeft,
  CLYDE,  14 + 2, 17,     203, MRight,
  PACMAN, 14,     17 + 9,   0, MLeft,
  BONUS,  14,     17 + 3,   0, MLeft,
};

//  Ghost colors
const uint8_t _palette2[] =
{
  0, 11, 1, 15,  // BINKY red
  0, 11, 3, 15,  // PINKY pink
  0, 11, 5, 15,  // INKY cyan
  0, 11, 7, 15,  // CLYDE brown
  0, 11, 9, 9,   // PACMAN yellow
  0, 11, 15, 15, // FRIGHTENED
  0, 11, 0, 15,  // DEADEYES

  0, 1, 15, 2,   // cherry
  0, 1, 15, 12,  // strawberry
  0, 7, 2, 12,   // peach
  0, 9, 15, 0,   // bell

  0, 15, 1, 2,   // apple
  0, 12, 15, 5,  // grape
  0, 11, 9, 1,   // galaxian
  0, 5, 15, 15,  // key

};

const uint8_t _paletteIcon2[] =
{
  0, 9, 9, 9, // PACMAN

  0, 2,  15, 1,  // cherry
  0, 12, 15, 1,  // strawberry
  0, 12, 2,  7,  // peach
  0, 0,  15, 9,  // bell

  0, 2,  15, 1,  // apple
  0, 12, 15, 5,  // grape
  0, 1,  9,  11, // galaxian
  0, 5,  15, 15, // key
};

#define PACMANICON 1
#define BONUSICON 2

#define FRIGHTENEDPALETTE 5
#define DEADEYESPALETTE 6

#define BONUSPALETTE 7

#define FPS 60
#define CHASE 0
#define SCATTER 1

#define DOT 7
#define PILL 14
#define PENGATE 0x1B

const uint8_t _opposite[] = { MStopped, MLeft, MUp, MRight, MDown };
#define OppositeDirection(_x) *(_opposite + _x)

const uint8_t _scatterChase[] = { 7, 20, 7, 20, 5, 20, 5, 0 };
const uint8_t _scatterTargets[] = { 2, 0, 25, 0, 0, 35, 27, 35 }; // inky/clyde scatter targets are backwards
const uint8_t _pinkyTargetOffset[] = { 4, 0, 0, 4, (uint8_t) - 4, 0, (uint8_t) - 4, 4 }; // Includes pinky target bug

#define FRIGHTENEDGHOSTSPRITE 0
#define GHOSTSPRITE 2
#define NUMBERSPRITE 10
#define PACMANSPRITE 14

const uint8_t _pacLeftAnim[] = { 5, 6, 5, 4 };
const uint8_t _pacRightAnim[] = { 2, 0, 2, 4 };
const uint8_t _pacVAnim[] = { 4, 3, 1, 3 };

uint16_t _BonusInactiveTimmer = BONUS_INACTIVE_TIME;
uint16_t _BonusActiveTimmer = 0;

/******************************************************************************/
/*   GAME - Sprite Class                                                      */
/******************************************************************************/

class Sprite
{
  public:
    int16_t _x, _y;
    int16_t lastx, lasty;
    uint8_t cx, cy;        // cell x and y
    uint8_t tx, ty;        // target x and y

    SpriteState state;
    uint8_t  pentimer;     // could be the same

    uint8_t who;
    uint8_t _speed;
    uint8_t dir;
    uint8_t phase;

    // Sprite bits
    uint8_t palette2;  // 4->16 color map index
    uint8_t bits;      // index of sprite bits
    int8_t sy;

    void Init(const uint8_t* s)
    {
      ClearKeys();
      who = *s++;
      cx =  *s++;
      cy =  *s++;
      pentimer = *s++;
      dir = *s;
      _x = lastx = (int16_t)cx * 8 - 4;
      _y = lasty = (int16_t)cy * 8;
      state = PenState;
      _speed = 0;
      Target(rand() % 20, rand() % 20);
    }


    void Target(uint8_t x, uint8_t y)
    {
      tx = x;
      ty = y;
    }

    int16_t Distance(uint8_t x, uint8_t y)
    {
      int16_t dx = cx - x;
      int16_t dy = cy - y;
      return dx * dx + dy * dy; // Distance to target
    }

    //  once per sprite, not 9 times
    void SetupDraw(GameState gameState, uint8_t deadGhostIndex)
    {
      sy = 1;
      palette2 = who;
      uint8_t p = phase >> 3;

      if (who == BONUS) {
        //BONUS ICONS
        bits = 21 + ACTUALBONUS;
        palette2 = BONUSPALETTE + ACTUALBONUS;
        return;
      }

      if (who != PACMAN)
      {
        bits = GHOSTSPRITE + ((dir - 1) << 1) + (p & 1); // Ghosts
        switch (state)
        {
          case FrightenedState:
            bits = FRIGHTENEDGHOSTSPRITE + (p & 1); // frightened
            palette2 = FRIGHTENEDPALETTE;
            break;
          case DeadNumberState:
            palette2 = FRIGHTENEDPALETTE;
            bits = NUMBERSPRITE + deadGhostIndex;
            break;
          case DeadEyesState:
            palette2 = DEADEYESPALETTE;
            break;
          default:
            ;
        }
        return;
      }

      //  PACMAN animation
      uint8_t f = (phase >> 1) & 3;
      if (dir == MLeft)
        f = _pacLeftAnim[f];
      else if (dir == MRight)
        f = _pacRightAnim[f];
      else
        f = _pacVAnim[f];
      if (dir == MUp)
        sy = -1;
      bits = f + PACMANSPRITE;
    }

    //  Draw this sprite into the tile at x,y
    void Draw8(int16_t x, int16_t y, uint8_t* tile)
    {

      int16_t px = x - (_x - 4);
      if (px <= -8 || px >= 16) return;
      int16_t py = y - (_y - 4);
      if (py <= -8 || py >= 16) return;

      // Clip y
      int16_t lines = py + 8;
      if (lines > 16)
        lines = 16;
      if (py < 0)
      {
        tile -= py * 8;
        py = 0;
      }
      lines -= py;

      //  Clip in X
      uint8_t right = 16 - px;
      if (right > 8)
        right = 8;
      uint8_t left = 0;
      if (px < 0)
      {
        left = -px;
        px = 0;
      }

      //  Get bitmap
      int8_t dy = sy;
      if (dy < 0)
        py = 15 - py;  // VFlip
      uint8_t* data = (uint8_t*)(pacman16x16 + bits * 64);
      data += py << 2;
      dy <<= 2;
      data += px >> 2;
      px &= 3;

      const uint8_t* palette = _palette2 + (palette2 << 2);
      while (lines)
      {
        const uint8_t *src = data;
        uint8_t d = *src++;
        d >>= px << 1;
        uint8_t sx = 4 - px;
        uint8_t x = left;
        do
        {
          uint8_t p = d & 3;
          if (p)
          {
            p = palette[p];
            if (p)
              tile[x] = p;
          }
          d >>= 2;    // Next pixel
          if (!--sx)
          {
            d = *src++;
            sx = 4;
          }
        } while (++x < right);

        tile += 8;
        data += dy;
        lines--;
      }
    }
};

/******************************************************************************/
/*   GAME - Playfield Class                                                   */
/******************************************************************************/

class Playfield
{

    Sprite _sprites[5];

    Sprite _BonusSprite; //Bonus

    uint8_t _dotMap[(32 / 4) * (36 - 6)];

    GameState _state;
    long    _score;             // 7 digits of score
    long    _hiscore;             // 7 digits of score
    long    _lifescore;
    int8_t    _scoreStr[8];
    int8_t    _hiscoreStr[8];
    uint8_t    _icons[14];         // Along bottom of screen

    ushort  _stateTimer;
    ushort  _frightenedTimer;
    uint8_t    _frightenedCount;
    uint8_t    _scIndex;           //
    ushort  _scTimer;           // next change of sc status

    bool _inited;
    uint8_t* _dirty;
  public:
    Playfield() : _inited(false)
    {
      //  Swizzle palette TODO just fix in place
      //      uint8_t * p = (uint8_t*)_paletteW;
      //      for (int16_t i = 0; i < 16; i++)
      //      {
      //        ushort w = _paletteW[i];    // Swizzle
      //        *p++ = w >> 8;
      //        *p++ = w;
      //      }
    }

    // Draw 2 bit BG into 8 bit icon tiles at bottom
    void DrawBG2(uint8_t cx, uint8_t cy, uint8_t* tile)
    {

      uint8_t index = 0;
      int8_t b = 0;

      index = _icons[cx >> 1];   // 13 icons across bottom
      if (index == 0)
      {
        memset(tile, 0, 64);
        return;
      }
      index--;
      index <<= 2;                        // 4 tiles per icon

      b = (1 - (cx & 1)) + ((cy & 1) << 1); // Index of tile

      const uint8_t* bg = pacman8x8x2 + ((b + index) << 4);
      const uint8_t* palette = _paletteIcon2 + index;


      uint8_t x = 16;
      while (x--)
      {
        uint8_t bits = (int8_t) * bg++;
        uint8_t i = 4;
        while (i--)
        {
          tile[i] = palette[bits & 3];
          bits >>= 2;
        }
        tile += 4;
      }
    }

    uint8_t GetTile(int16_t cx, int16_t ty)
    {

      if (_state != ReadyState && ty == 20 && cx > 10 && cx < 17) return (0); //READY TEXT ZONE

      if (LEVEL % 5 == 1) return playMap1[ty * 28 + cx];
      if (LEVEL % 5 == 2) return playMap2[ty * 28 + cx];
      if (LEVEL % 5 == 3) return playMap3[ty * 28 + cx];
      if (LEVEL % 5 == 4) return playMap4[ty * 28 + cx];
      if (LEVEL % 5 == 0) return playMap5[ty * 28 + cx];
      return 0;
    }

    // Draw 1 bit BG into 8 bit tile
    void DrawBG(uint8_t cx, uint8_t cy, uint8_t* tile)
    {

      memset(tile, 0, 64);
      if (cy >= 34) //DRAW ICONS BELLOW MAZE
      {
        DrawBG2(cx, cy, tile);
        return;
      }


      uint8_t c = 11;
      if (LEVEL % 8 == 1) c = 11; // Blue
      if (LEVEL % 8 == 2) c = 12; // Green
      if (LEVEL % 8 == 3) c = 1; // Red
      if (LEVEL % 8 == 4) c = 9; // Yellow
      if (LEVEL % 8 == 5) c = 2; // Brown
      if (LEVEL % 8 == 6) c = 5; // Cyan
      if (LEVEL % 8 == 7) c = 3; // Pink
      if (LEVEL % 8 == 0) c = 15; // White

      uint8_t b = GetTile(cx, cy);
      const uint8_t* bg;

      //  This is a little messy
      memset(tile, 0, 64);
      if (cy == 20 && cx >= 11 && cx < 17)
      {
        if (DEMO == 1 && ACTIVEBONUS == 1) return;

        if ((_state != ReadyState && GAMEPAUSED != 1 && DEMO != 1) || ACTIVEBONUS == 1) b = 0; // hide 'READY!'
        else if (DEMO == 1 && cx == 11) b = 0;
        else if (DEMO == 1 && cx == 12) b = 'D';
        else if (DEMO == 1 && cx == 13) b = 'E';
        else if (DEMO == 1 && cx == 14) b = 'M';
        else if (DEMO == 1 && cx == 15) b = 'O';
        else if (DEMO == 1 && cx == 16) b = 0;
        else if (GAMEPAUSED == 1 && cx == 11) b = 'P';
        else if (GAMEPAUSED == 1 && cx == 12) b = 'A';
        else if (GAMEPAUSED == 1 && cx == 13) b = 'U';
        else if (GAMEPAUSED == 1 && cx == 14) b = 'S';
        else if (GAMEPAUSED == 1 && cx == 15) b = 'E';
        else if (GAMEPAUSED == 1 && cx == 16) b = 'D';
      }
      else if (cy == 1)
      {
        if (cx < 7)
          b = _scoreStr[cx];
        else if (cx >= 10 && cx < 17)
          b = _hiscoreStr[cx - 10]; // HiScore
      } else {
        if (b == DOT || b == PILL)  // DOT==7 or PILL==16
        {
          if (!GetDot(cx, cy))
            return;
          c = 14;
        }
        if (b == PENGATE)
          c = 14;
      }

      bg = playTiles + (b << 3);
      if (b >= '0')
        c = 15; // text is white

      for (uint8_t y = 0; y < 8; y++)
      {
        int8_t bits = (int8_t) * bg++; ///WARNING CHAR MUST BE signed !!!
        uint8_t x = 0;
        while (bits)
        {
          if (bits < 0)
            tile[x] = c;
          bits <<= 1;
          x++;
        }
        tile += 8;
      }
    }

    // Draw BG then all sprites in this cell
    void Draw(uint16_t x, uint16_t y, bool sprites)
    {
      static uint8_t tile[8 * 8];
      memset(tile, 0, sizeof(tile));

      //      Fill with BG
      if (y == 20 && x >= 11 && x < 17 && DEMO == 1 && ACTIVEBONUS == 1)  return;
      DrawBG(x, y, tile);

      //      Overlay sprites
      x <<= 3;
      y <<= 3;
      if (sprites)
      {
        for (uint8_t i = 0; i < 5; i++)
          _sprites[i].Draw8(x, y, tile);

        //AND BONUS
        if (ACTIVEBONUS) _BonusSprite.Draw8(x, y, tile);

      }

      //      Show sprite block
#if 0
      for (uint8_t i = 0; i < 5; i++)
      {
        Sprite* s = _sprites + i;
        if (s->cx == (x >> 3) && s->cy == (y >> 3))
        {
          memset(tile, 0, 8);
          for (uint8_t j = 1; j < 7; j++)
            tile[j * 8] = tile[j * 8 + 7] = 0;
          memset(tile + 56, 0, 8);
        }
      }
#endif

      x += (240 - 224) / 2;
      y += (320 - 288) / 2;


      //      Should be a direct Graphics call

      //uint8_t n = tile[0];
      //uint8_t i = 0;
      //uint16_t color = (uint16_t)_paletteW[n];

      drawIndexedmap(tile, x, y);
    }

    boolean updateMap [36][28];

    //  Mark tile as dirty (should not need range checking here)
    void Mark(int16_t x, int16_t y, uint8_t* m)
    {
      x -= 4;
      y -= 4;

      updateMap[(y >> 3)][(x >> 3)] = true;
      updateMap[(y >> 3)][(x >> 3) + 1] = true;
      updateMap[(y >> 3)][(x >> 3) + 2] = true;
      updateMap[(y >> 3) + 1][(x >> 3)] = true;
      updateMap[(y >> 3) + 1][(x >> 3) + 1] = true;
      updateMap[(y >> 3) + 1][(x >> 3) + 2] = true;
      updateMap[(y >> 3) + 2][(x >> 3)] = true;
      updateMap[(y >> 3) + 2][(x >> 3) + 1] = true;
      updateMap[(y >> 3) + 2][(x >> 3) + 2] = true;

    }

    void DrawAllBG()
    {
      for (uint8_t y = 0; y < 36; y++)
        for (uint8_t x = 0; x < 28; x++) {
          Draw(x, y, false);
        }
    }

    //  Draw sprites overlayed on cells
    void DrawAll()
    {
      uint8_t* m = _dirty;

      //  Mark sprite old/new positions as dirty
      for (uint8_t i = 0; i < 5; i++)
      {
        Sprite* s = _sprites + i;
        Mark(s->lastx, s->lasty, m);
        Mark(s->_x, s->_y, m);

      }

      // Mark BONUS sprite old/new positions as dirty
      Sprite* _s = &_BonusSprite;
      Mark(_s->lastx, _s->lasty, m);
      Mark(_s->_x, _s->_y, m);


      //  Animation
      for (uint8_t i = 0; i < 5; i++)
        _sprites[i].SetupDraw(_state, _frightenedCount - 1);

      _BonusSprite.SetupDraw(_state, _frightenedCount - 1);


      for (uint8_t tmpY = 0; tmpY < 36; tmpY++) {
        for (uint8_t tmpX = 0; tmpX < 28; tmpX++) {
          if (updateMap[tmpY][tmpX] == true) Draw(tmpX, tmpY, true);
          updateMap[tmpY][tmpX] = false;
        }

      }
    }


    int16_t Chase(Sprite* s, int16_t cx, int16_t cy)
    {
      while (cx < 0)      //  Tunneling
        cx += 28;
      while (cx >= 28)
        cx -= 28;

      uint8_t t = GetTile(cx, cy);
      if (!(t == 0 || t == DOT || t == PILL || t == PENGATE))
        return 0x7FFF;

      if (t == PENGATE)
      {
        if (s->who == PACMAN)
          return 0x7FFF;  // Pacman can't cross this to enter pen
        if (!(InPen(s->cx, s->cy) || s->state == DeadEyesState))
          return 0x7FFF;  // Can cross if dead or in pen trying to get out
      }

      int16_t dx = s->tx - cx;
      int16_t dy = s->ty - cy;
      return (dx * dx + dy * dy); // Distance to target

    }

    void UpdateTimers()
    {
      // Update scatter/chase selector, low bit of index indicates scatter
      if (_scIndex < 8)
      {
        if (_scTimer-- == 0)
        {
          uint8_t duration = _scatterChase[_scIndex++];
          _scTimer = duration * FPS;
        }
      }

      // BONUS timmer
      if (ACTIVEBONUS == 0 && _BonusInactiveTimmer-- == 0) {
        _BonusActiveTimmer = BONUS_ACTIVE_TIME; //5*FPS;
        ACTIVEBONUS = 1;
      }
      if (ACTIVEBONUS == 1 && _BonusActiveTimmer-- == 0) {
        _BonusInactiveTimmer = BONUS_INACTIVE_TIME; //10*FPS;
        ACTIVEBONUS = 0;
      }


      //  Release frightened ghosts
      if (_frightenedTimer && !--_frightenedTimer)
      {
        for (uint8_t i = 0; i < 4; i++)
        {
          Sprite* s = _sprites + i;
          if (s->state == FrightenedState)
          {
            s->state = RunState;
            s->dir = OppositeDirection(s->dir);
          }
        }
      }
    }

    //  Target closes pill, run from ghosts?
    void PacmanAI()
    {
      Sprite* pacman;
      pacman = _sprites + PACMAN;

      //  Chase frightened ghosts
      //Sprite* closestGhost = NULL;
      Sprite* frightenedGhost = NULL;
      Sprite* closestAttackingGhost = NULL;
      Sprite* DeadEyesStateGhost = NULL;
      int16_t dist = 0x7FFF;
      int16_t closestfrightenedDist = 0x7FFF;
      int16_t closestAttackingDist = 0x7FFF;
      for (uint8_t i = 0; i < 4; i++)
      {
        Sprite* s = _sprites + i;
        int16_t d = s->Distance(pacman->cx, pacman->cy);
        if (d < dist)
        {

          dist = d;
          if (s->state == FrightenedState ) {
            frightenedGhost = s;
            closestfrightenedDist = d;
          }
          else {
            closestAttackingGhost = s;
            closestAttackingDist = d;
          }
          //closestGhost = s;

          if ( s->state == DeadEyesState ) DeadEyesStateGhost = s;

        }
      }

      PACMANFALLBACK = 0;

      if (DEMO == 1 && !DeadEyesStateGhost && frightenedGhost )
      {
        pacman->Target(frightenedGhost->cx, frightenedGhost->cy);
        return;
      }



      // Under threat; just avoid closest ghost
      if (DEMO == 1 && !DeadEyesStateGhost && dist <= 32  && closestAttackingDist < closestfrightenedDist )
      {
        if (dist <= 16) {
          pacman->Target( pacman->cx * 2 - closestAttackingGhost->cx, pacman->cy * 2 - closestAttackingGhost->cy);
          PACMANFALLBACK = 1;
        } else {
          pacman->Target( pacman->cx * 2 - closestAttackingGhost->cx, pacman->cy * 2 - closestAttackingGhost->cy);
        }
        return;
      }

      if (ACTIVEBONUS == 1) {
        pacman->Target(13, 20);
        return;
      }


      //  Go for the pill
      if (GetDot(1, 6))
        pacman->Target(1, 6);
      else if (GetDot(26, 6))
        pacman->Target(26, 6);
      else if (GetDot(1, 26))
        pacman->Target(1, 26);
      else if (GetDot(26, 26))
        pacman->Target(26, 26);
      else
      {
        // closest dot
        int16_t dist = 0x7FFF;
        for (uint8_t y = 4; y < 32; y++)
        {
          for (uint8_t x = 1; x < 26; x++)
          {
            if (GetDot(x, y))
            {
              int16_t d = pacman->Distance(x, y);
              if (d < dist)
              {
                dist = d;
                pacman->Target(x, y);
              }
            }
          }
        }

        if (dist == 0x7FFF) {
          GAMEWIN = 1; // No dots, GAME WIN!
        }

      }
    }

    void Scatter(Sprite* s)
    {
      const uint8_t* st = _scatterTargets + (s->who << 1);
      s->Target(*st, *(st + 1));
    }

    void UpdateTargets()
    {
      if (_state == ReadyState)
        return;
      PacmanAI();
      Sprite* pacman = _sprites + PACMAN;

      //  Ghost AI
      bool scatter = _scIndex & 1;
      for (uint8_t i = 0; i < 4; i++)
      {
        Sprite* s = _sprites + i;

        //  Deal with returning ghost to pen
        if (s->state == DeadEyesState)
        {
          if (s->cx == 14 && s->cy == 17) // returned to pen
          {
            s->state = PenState;        // Revived in pen
            s->pentimer = 80;
          }
          else
            s->Target(14, 17);          // target pen
          continue;           //
        }

        //  Release ghost from pen when timer expires
        if (s->pentimer)
        {
          if (--s->pentimer)  // stay in pen for awhile
            continue;
          s->state = RunState;
        }

        if (InPen(s->cx, s->cy))
        {
          s->Target(14, 14 - 2); // Get out of pen first
        } else {
          if (scatter || s->state == FrightenedState)
            Scatter(s);
          else
          {
            // Chase mode targeting
            int8_t tx = pacman->cx;
            int8_t ty = pacman->cy;
            switch (s->who)
            {
              case PINKY:
                {
                  const uint8_t* pto = _pinkyTargetOffset + ((pacman->dir - 1) << 1);
                  tx += *pto;
                  ty += *(pto + 1);
                }
                break;
              case INKY:
                {
                  const uint8_t* pto = _pinkyTargetOffset + ((pacman->dir - 1) << 1);
                  Sprite* binky = _sprites + BINKY;
                  tx += *pto >> 1;
                  ty += *(pto + 1) >> 1;
                  tx += tx - binky->cx;
                  ty += ty - binky->cy;
                }
                break;
              case CLYDE:
                {
                  if (s->Distance(pacman->cx, pacman->cy) < 64)
                  {
                    const uint8_t* st = _scatterTargets + CLYDE * 2;
                    tx = *st;
                    ty = *(st + 1);
                  }
                }
                break;
            }
            s->Target(tx, ty);
          }
        }
      }
    }

    //  Default to current direction
    uint8_t ChooseDir(int16_t dir, Sprite* s)
    {
      int16_t choice[4];
      choice[0] = Chase(s, s->cx, s->cy - 1); // Up
      choice[1] = Chase(s, s->cx - 1, s->cy); // Left
      choice[2] = Chase(s, s->cx, s->cy + 1); // Down
      choice[3] = Chase(s, s->cx + 1, s->cy); // Right


      if (DEMO == 0 && s->who == PACMAN && choice[0] < 0x7FFF && but_UP) dir = MUp;
      else if (DEMO == 0 && s->who == PACMAN && choice[1] < 0x7FFF && but_LEFT) dir = MLeft;
      else if (DEMO == 0 && s->who == PACMAN && choice[2] < 0x7FFF && but_DOWN) dir = MDown;
      else if (DEMO == 0 && s->who == PACMAN && choice[3] < 0x7FFF && but_RIGHT) dir = MRight;

      else if (DEMO == 0 && choice[0] < 0x7FFF && s->who == PACMAN && dir == MUp) dir = MUp;
      else if (DEMO == 0 && choice[1] < 0x7FFF && s->who == PACMAN && dir == MLeft) dir = MLeft;
      else if (DEMO == 0 && choice[2] < 0x7FFF && s->who == PACMAN && dir == MDown) dir = MDown;
      else if (DEMO == 0 && choice[3] < 0x7FFF && s->who == PACMAN && dir == MRight) dir = MRight;
      else if ((DEMO == 0 && s->who != PACMAN) || DEMO == 1 ) {

        // Don't choose opposite of current direction?

        int16_t dist = choice[4 - dir]; // favor current direction
        uint8_t opposite = OppositeDirection(dir);
        for (uint8_t i = 0; i < 4; i++)
        {
          uint8_t d = 4 - i;
          if ((d != opposite && choice[i] < dist) || (s->who == PACMAN && PACMANFALLBACK &&  choice[i] < dist))
          {
            if (s->who == PACMAN && PACMANFALLBACK) PACMANFALLBACK = 0;
            dist = choice[i];
            dir = d;
          }

        }

      } else  {
        dir = MStopped;
      }

      return dir;
    }

    bool InPen(uint8_t cx, uint8_t cy)
    {
      if (cx <= 10 || cx >= 18) return false;
      if (cy <= 14 || cy >= 18) return false;
      return true;
    }

    uint8_t GetSpeed(Sprite* s)
    {
      if (s->who == PACMAN)
        return _frightenedTimer ? 90 : 80;
      if (s->state == FrightenedState)
        return 40;
      if (s->state == DeadEyesState)
        return 100;
      if (s->cy == 17 && (s->cx <= 5 || s->cx > 20))
        return 40;  // tunnel
      return 75;
    }

    void PackmanDied() {  // Noooo... PACMAN DIED :(
#if(BOARD_TYPE == BOARD_TYPE_HMI)
      if (DEMO == 0) {
        GameAudio.PlayWav(&pmDeath, true, 1.0);
        // wait until done
        while (GameAudio.IsPlaying());
      }
#endif
      if (LIFES <= 0) {
        GAMEOVER = 1;
        LEVEL = START_LEVEL;
        LIFES = START_LIFES;
        DEMO = 1;
        Init();
      } else {
        LIFES--;

        _inited = true;
        _state = ReadyState;
        _stateTimer = FPS / 2;
        _frightenedCount = 0;
        _frightenedTimer = 0;

        const uint8_t* s = _initSprites;
        for (int16_t i = 0; i < 5; i++)
          _sprites[i].Init(s + i * 5);

        _scIndex = 0;
        _scTimer = 1;

        memset(_icons, 0, sizeof(_icons));

        //AND BONUS
        _BonusSprite.Init(s + 5 * 5);
        _BonusInactiveTimmer = BONUS_INACTIVE_TIME;
        _BonusActiveTimmer = 0;

        for (uint8_t i = 0; i < ACTUALBONUS; i++) {
          _icons[13 - i] = BONUSICON + i;
        }

        for (uint8_t i = 0; i < LIFES; i++) {
          _icons[0 + i] = PACMANICON;
        }

        //Draw LIFE and BONUS Icons
        for (uint8_t y = 34; y < 36; y++)
          for (uint8_t x = 0; x < 28; x++) {
            Draw(x, y, false);
          }

        DrawAllBG();
      }
    }


    void MoveAll()
    {
      UpdateTimers();
      UpdateTargets();


      //  Update game state
      if (_stateTimer)
      {
        if (--_stateTimer <= 0)
        {
          switch (_state)
          {
            case ReadyState:
              _state = PlayState;
              _dirty[20 * 4 + 1] |= 0x1F; // Clear 'READY!'
              _dirty[20 * 4 + 2] |= 0x80;

              for (uint8_t tmpX = 11; tmpX < 17; tmpX++) Draw(tmpX, 20, false); // ReDraw (clear) 'READY' position

              break;
            case DeadGhostState:
              _state = PlayState;
              for (uint8_t i = 0; i < 4; i++)
              {
                Sprite* s = _sprites + i;
                if (s->state == DeadNumberState)
                  s->state = DeadEyesState;
              }
              break;
            default:
              ;
          }
        } else {
          if (_state == ReadyState)
            return;
        }
      }

      for (uint8_t i = 0; i < 5; i++)
      {
        Sprite* s = _sprites + i;

        //  In DeadGhostState, only eyes move
        if (_state == DeadGhostState && s->state != DeadEyesState)
          continue;

        //  Calculate speed
        s->_speed += GetSpeed(s);
        if (s->_speed < 100)
          continue;
        s->_speed -= 100;

        s->lastx = s->_x;
        s->lasty = s->_y;
        s->phase++;

        int16_t x = s->_x;
        int16_t y = s->_y;


        if ((x & 0x7) == 0 && (y & 0x7) == 0)   // cell aligned
          s->dir = ChooseDir(s->dir, s);      // time to choose another direction


        switch (s->dir)
        {
          case MLeft:     x -= SPEED; break;
          case MRight:    x += SPEED; break;
          case MUp:       y -= SPEED; break;
          case MDown:     y += SPEED; break;
          case MStopped:  break;
        }

        //  Wrap x because of tunnels
        while (x < 0)
          x += 224;
        while (x >= 224)
          x -= 224;

        s->_x = x;
        s->_y = y;
        s->cx = (x + 4) >> 3;
        s->cy = (y + 4) >> 3;

        if (s->who == PACMAN)
          EatDot(s->cx, s->cy);
      }

      //  Collide
      Sprite* pacman = _sprites + PACMAN;

      //  Collide with BONUS
      Sprite* _s = &_BonusSprite;
      if (ACTIVEBONUS == 1 && _s->cx == pacman->cx && _s->cy == pacman->cy)
      {
        Score(ACTUALBONUS * 50);
        ACTUALBONUS++;
        if (ACTUALBONUS > 7) {
          ACTUALBONUS = 0;
          if (LIFES < MAXLIFES) LIFES++;

          //reset all icons
          memset(_icons, 0, sizeof(_icons));

          for (uint8_t i = 0; i < LIFES; i++) {
            _icons[0 + i] = PACMANICON;
          }

        }

        for (uint8_t i = 0; i < ACTUALBONUS; i++) {
          _icons[13 - i] = BONUSICON + i;
        }

        //REDRAW LIFE and BONUS icons
        for (uint8_t y = 34; y < 36; y++)
          for (uint8_t x = 0; x < 28; x++) {
            Draw(x, y, false);
          }

        ACTIVEBONUS = 0;
        _BonusInactiveTimmer = BONUS_INACTIVE_TIME;
      }

      for (uint8_t i = 0; i < 4; i++)
      {
        Sprite* s = _sprites + i;
        //if (s->cx == pacman->cx && s->cy == pacman->cy)
        if (s->_x + SPEED >= pacman->_x && s->_x - SPEED <= pacman->_x && s->_y + SPEED >= pacman->_y && s->_y - SPEED <= pacman->_y)



        {
          if (s->state == FrightenedState)
          {
#if(BOARD_TYPE == BOARD_TYPE_HMI)
            if (DEMO == 0) {
              GameAudio.PlayWav(&pmEatGhost, true, 1.0);
            }
#endif
            s->state = DeadNumberState;     // Killed a ghost
            _frightenedCount++;
            _state = DeadGhostState;
            _stateTimer = 10;
            Score((1 << _frightenedCount) * 100);
          }
          else {               // pacman died
            if (s->state == DeadNumberState || s->state == FrightenedState || s->state == DeadEyesState) {
            } else {
              PackmanDied();
            }
          }
        }
      }
    }

    //  Mark a position dirty
    void Mark(int16_t pos)
    {
      for (uint8_t tmp = 0; tmp < 28; tmp++)
        updateMap[1][tmp] = true;

    }

    void SetScoreChar(uint8_t i, int8_t c)
    {
      if (_scoreStr[i] == c)
        return;
      _scoreStr[i] = c;
      Mark(i + 32);  //Score
      //Mark(i+32+10); //HiScore

    }

    void SetHiScoreChar(uint8_t i, int8_t c)
    {
      if (_hiscoreStr[i] == c)
        return;
      _hiscoreStr[i] = c;
      //Mark(i+32);    //Score
      Mark(i + 32 + 10); //HiScore
    }

    void Score(int16_t delta)
    {
      char str[8];
      _score += delta;
      if (DEMO == 0 && _score > _hiscore) _hiscore = _score;

      if (_score > _lifescore && _score % 10000 > 0) {
        _lifescore = (_score / 10000 + 1) * 10000;

        LIFES++; // EVERY 10000 points = 1UP

        for (uint8_t i = 0; i < LIFES; i++) {
          _icons[0 + i] = PACMANICON;
        }

        //REDRAW LIFE and BONUS icons
        for (uint8_t y = 34; y < 36; y++)
          for (uint8_t x = 0; x < 28; x++) {
            Draw(x, y, false);
          }
        _score = _score + 100;
      }

      sprintf(str, "%ld", _score);
      uint8_t i = 7 - strlen(str);
      uint8_t j = 0;
      while (i < 7)
        SetScoreChar(i++, str[j++]);
      sprintf(str, "%ld", _hiscore);
      i = 7 - strlen(str);
      j = 0;
      while (i < 7)
        SetHiScoreChar(i++, str[j++]);
    }

    bool GetDot(uint8_t cx, uint8_t cy)
    {
      return _dotMap[(cy - 3) * 4 + (cx >> 3)] & (0x80 >> (cx & 7));
    }

    void EatDot(uint8_t cx, uint8_t cy)
    {
      if (!GetDot(cx, cy))
        return;
      uint8_t mask = 0x80 >> (cx & 7);
      _dotMap[(cy - 3) * 4 + (cx >> 3)] &= ~mask;
#if(BOARD_TYPE == BOARD_TYPE_HMI)
      if (DEMO == 0) {
        GameAudio.PlayWav(&pmChomp, false, 1.0);
      }
#endif
      uint8_t t = GetTile(cx, cy);
      if (t == PILL)
      {
        _frightenedTimer = 10 * FPS;
        _frightenedCount = 0;
        for (uint8_t i = 0; i < 4; i++)
        {
          Sprite* s = _sprites + i;
          if (s->state == RunState)
          {
            s->state = FrightenedState;
            s->dir = OppositeDirection(s->dir);
          }
        }
        Score(50);
      }
      else
        Score(10);
    }

    void Init()
    {
      drawButtonFace(4);  // START / PAUSE

      if (GAMEWIN == 1) {
        GAMEWIN = 0;
      } else {
        LEVEL = START_LEVEL;
        LIFES = START_LIFES;
        ACTUALBONUS = 0; //actual bonus icon
        ACTIVEBONUS = 0; //status of bonus

        _score = 0;
        _lifescore = 10000;

        memset(_scoreStr, 0, sizeof(_scoreStr));
        _scoreStr[5] = _scoreStr[6] = '0';
      }


      _inited = true;
      _state = ReadyState;
      _stateTimer = FPS / 2;
      _frightenedCount = 0;
      _frightenedTimer = 0;

      const uint8_t* s = _initSprites;
      for (int16_t i = 0; i < 5; i++)
        _sprites[i].Init(s + i * 5);

      //AND BONUS
      _BonusSprite.Init(s + 5 * 5);
      _BonusInactiveTimmer = BONUS_INACTIVE_TIME;
      _BonusActiveTimmer = 0;

      _scIndex = 0;
      _scTimer = 1;

      memset(_icons, 0, sizeof(_icons));

      // SET BONUS icons
      for (uint8_t i = 0; i < ACTUALBONUS; i++) {
        _icons[13 - i] = BONUSICON + i;
      }

      // SET Lifes icons
      for (uint8_t i = 0; i < LIFES; i++) {
        _icons[0 + i] = PACMANICON;
      }

      //Draw LIFE and BONUS Icons
      for (uint8_t y = 34; y < 36; y++)
        for (uint8_t x = 0; x < 28; x++) {
          Draw(x, y, false);
        }

      //  Init dots from rom
      memset(_dotMap, 0, sizeof(_dotMap));
      uint8_t* map = _dotMap;
      for (uint8_t y = 3; y < 36 - 3; y++) // 30 interior lines
      {
        for (uint8_t x = 0; x < 28; x++)
        {
          uint8_t t = GetTile(x, y);
          if (t == 7 || t == 14)
          {
            uint8_t s = x & 7;
            map[x >> 3] |= (0x80 >> s);
          }
        }
        map += 4;
      }
      DrawAllBG();
    }

    void Step()
    {
      //int16_t keys = 0;

      if (GAMEWIN == 1) {
        LEVEL++;
        Init();
      }

      // Start GAME
      if (but_A && DEMO == 1 && GAMEPAUSED == 0) {
        but_A = false;
        DEMO = 0;
#if(BOARD_TYPE == BOARD_TYPE_HMI)
        GameAudio.PlayWav(&pmWav, false, 1.0);
        // wait until done
        //while(GameAudio.IsPlaying()){ }
#endif
        Init();
      } else if (but_A && DEMO == 0 && GAMEPAUSED == 0) { // Or PAUSE GAME
        but_A = false;
        GAMEPAUSED = 1;
        drawButtonFace(4);  // START / PAUSE
      }

      if (GAMEPAUSED && but_A && DEMO == 0) {
        but_A = false;
        GAMEPAUSED = 0;
        drawButtonFace(4);  // START / PAUSE
        for (uint8_t tmpX = 11; tmpX < 17; tmpX++) Draw(tmpX, 20, false);
      }

      // Reset / Start GAME
      if (but_B) {
        but_B = false;
        DEMO = 0;
        Init();
      } else if (!_inited) {
        but_B = false;
        DEMO = 1;
        Init();
      }

      // Create a bitmap of dirty tiles
      uint8_t m[(32 / 8) * 36]; // 144 bytes
      memset(m, 0, sizeof(m));
      _dirty = m;


      if (!GAMEPAUSED) MoveAll(); // IF GAME is PAUSED STOP ALL

      if ((ACTIVEBONUS == 0 && DEMO == 1) || GAMEPAUSED == 1 ) for (uint8_t tmpX = 11; tmpX < 17; tmpX++) Draw(tmpX, 20, false); // Draw 'PAUSED' or 'DEMO' text

      DrawAll();
    }
};

/******************************************************************************/
/*   SETUP                                                                    */
/******************************************************************************/

#define BLACK 0x0000 // 16bit BLACK Color

/******************************************************************************/
/*   LOOP                                                                     */
/******************************************************************************/

void ClearKeys() {
  but_A = false;
  but_B = false;
  but_UP = false;
  but_DOWN = false;
  but_LEFT = false;
  but_RIGHT = false;
}

/*
  static IRAM_ATTR bool lvgl_read_cb(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
  {
    static uint8_t tp_num = 0;
    static uint16_t x = 0, y = 0;

    touchPadRead(&tp_num, &x, &y);

    if (0 == tp_num) {
        data->state = LV_INDEV_STATE_REL;
    } else {
        // Swap xy order and invert y direction
        data->point.x = y;
        data->point.y = LV_VER_RES_MAX - x - 1;
        data->state = LV_INDEV_STATE_PR;
    }

    return false;
  }
*/

/*
  void KeyPadLoop(){
  if (Serial.available()) {
    uint8_t r = Serial.read();
    if (r == 'z') { ClearKeys();  but_A=true; delay(300); } //else but_A=false;
    if (r == 'x') { ClearKeys();  but_B=true; delay(300); }  else but_B=false;
    if (r == '8') { ClearKeys();  but_UP=true; }  //else but_UP=false;
    if (r == '2') { ClearKeys();  but_DOWN=true; }  //else but_DOWN=false;
    if (r == '4') { ClearKeys();  but_LEFT=true; }  // else but_LEFT=false;
    if (r == '6') { ClearKeys();  but_RIGHT=true; }  //else but_RIGHT=false;
  }
  if (udp.parsePacket()) {
    uint8_t r = udp.read();
    if (r == 'z') { ClearKeys();  but_A=true; delay(300); } //else but_A=false;
    if (r == 'x') { ClearKeys();  but_B=true; delay(300); }  else but_B=false;
    if (r == '8') { ClearKeys();  but_UP=true; }  //else but_UP=false;
    if (r == '2') { ClearKeys();  but_DOWN=true; }  //else but_DOWN=false;
    if (r == '4') { ClearKeys();  but_LEFT=true; }  // else but_LEFT=false;
    if (r == '6') { ClearKeys();  but_RIGHT=true; }  //else but_RIGHT=false;
  }
  }
*/

Playfield _game;

void drawIndexedmap(uint8_t* indexmap, uint16_t x, uint16_t y) {
  //x += (240 - 224) / 2;
  //y += (320 - 288) / 2;

  uint8_t i = 0;
  static uint16_t screenBuffer[16*16];
  //memset(screenBuffer, 0, 16*16*2);

  for (uint8_t tmpY = 0; tmpY < 8; tmpY++) {
    for (uint8_t tmpX = 0; tmpX < 8; tmpX++) {
      // rotate the screen 90 counterclockwise considering image duplication
      uint16_t xt = 2 * tmpY;
      uint16_t yt = 14 - 2 * tmpX;

      // duplicate width and height of the game screen (240x320 ==> 480x640)
      screenBuffer[xt + yt * 16] =  screenBuffer[xt + 1 + yt * 16] = 
      screenBuffer[xt + (yt + 1) * 16] = screenBuffer[xt + 1 + (yt + 1) * 16] = _paletteW[indexmap[i]];
      i++;
    }
  }
  // rotate the screen 90 counterclockwise considering image duplication
  uint16_t xt = 2 * y;
  uint16_t yt = SCR_HEIGHT - 2 * (x + 8);
   
  bsp_lcd_flush(xt, yt, xt + 16, yt + 16, (void *) screenBuffer);
  
}

// Rotated dimensions based on a SCR_w = 480 and SCR_H = 800
//#define BUT_W       100
//#define BUT_H       55
#define BUT_NUM     5      // removed BUT B = reset
#define BUT_X       0
#define BUT_Y       1
#define BUT_COLOR   2
#define BUT_W       3
#define BUT_H       4

uint32_t debounceTimeStart = 0;

const uint16_t buttons[BUT_NUM][5] = {
  // Using rotated coordintes the same way that the touch sensor does
  // X0 is 0 .. SCR_HEIGHT -1
  // Y0 is 0 .. SCR_WIDTH - 1
  //X0   Y0    color          W    H
  { 255, 610, _paletteW[15], 100, 55},  // UP
  { 370, 675, _paletteW[15], 100, 55},  // LEFT
  { 140, 675, _paletteW[15], 100, 55},  // RIGHT
  { 255, 740, _paletteW[15], 100, 55},  // DOWN
  { 0,   620, _paletteW[6],  60,  60},   // A
  /*
    { 610, 255, _paletteW[15], 100, 55},  // UP
    { 675, 370, _paletteW[15], 100, 55},  // LEFT
    { 675, 140, _paletteW[15], 100, 55},  // RIGHT
    { 740, 255, _paletteW[15], 100, 55},  // DOWN
    { 620,   0, _paletteW[6],  60,  60},   // A
  */
  //  { 10,  640, _paletteW[6]},   // A
  //  { 10,  740, _paletteW[1]},   // B
};

int8_t getTouchedButton(uint16_t x, uint16_t y) {
  // ft5x06 driver inverts X coordinate and also rotates it
  uint8_t delta = 15;
  //x = SCR_HEIGHT - 1 - x;
  int8_t pressedButton = -1;
  for (uint8_t b = 0; b < BUT_NUM; b++) {
    if (b > BUT_NUM - 1) delta = 0; // Buttons A and B are not "expanded" in the touch area
    if (x >= buttons[b][BUT_X] - delta && x < buttons[b][BUT_X] + buttons[b][BUT_W] + delta
        && y >= buttons[b][BUT_Y] - delta && y < buttons[b][BUT_Y] + buttons[b][BUT_H] + delta) {
      pressedButton = b;
      break;
    }
  }
  return pressedButton;
}

void drawButtonFace(uint8_t btId) {
  // rotate the coordinates by sawpping X & Y
  uint16_t x0 = buttons[btId][BUT_Y];
  uint16_t y0 = buttons[btId][BUT_X];

  uint16_t _w = buttons[btId][BUT_H], _h = buttons[btId][BUT_W];
  uint8_t r = min(_w, _h) / 4; // Corner radius
  int16_t _x1 = 0, _y1 = 0;

  size_t canvasSize = _w * _h * sizeof(uint16_t);
  uint16_t *canvas = (uint16_t *)heap_caps_malloc(canvasSize, MALLOC_CAP_8BIT);
  if (canvas == NULL) {
    printf("Memeory Allocation error in drawButtonFace()\n");
    return;
  } 
  memset(canvas, 0, canvasSize);
  tft16bits.setBuffer(canvas);
  tft16bits.setWidthHeight(_w, _h);

  tft16bits.fillRoundRect(_x1, _y1, _w, _h, r, buttons[btId][BUT_COLOR]);

  switch (btId) {
    case 0:   // UP
      tft16bits.drawRoundRect(_x1, _y1, _w, _h, r, RED);
      tft16bits.fillTriangle(_x1 + 10, _y1 + _h / 2, _x1 + _w - 15, _y1 + _h / 2 + 20, _x1 + _w - 15, _y1 + _h / 2 - 20, BLACK);
      //tft16bits.fillRect(_x1 + 10, _y1 + _h / 2, 4, 4, RED);
      //tft16bits.fillRect(_x1 + _w - 15, _y1 + _h / 2 + 20, 4, 4, BLUE);
      //tft16bits.fillRect(_x1 + _w - 15, _y1 + _h / 2 - 20, 4, 4, BLACK);
      break;
    case 1:   // LEFT
      tft16bits.drawRoundRect(_x1, _y1, _w, _h, r, RED);
      tft16bits.fillTriangle(_x1 + _w - 15, _y1 + _h / 2 - 15, _x1 + 10, _y1 + _h / 2 - 15, _x1 + _w / 2, _y1 + _h / 2 + 20, BLACK);
      //tft16bits.fillRect(_x1 + _w - 15, _y1 + _h / 2 - 15, 4, 4, RED);
      //tft16bits.fillRect(_x1 + 10, _y1 + _h / 2 - 15, 4, 4, BLUE);
      //tft16bits.fillRect(_x1 + _w / 2, _y1 + _h / 2 + 20, 4, 4, BLACK);
      break;
    case 2:   // RIGHT
      tft16bits.drawRoundRect(_x1, _y1, _w, _h, r, RED);
      tft16bits.fillTriangle(_x1 + _w - 15, _y1 + _h / 2 + 15, _x1 + 10, _y1 + _h / 2 + 15, _x1 + _w / 2, _y1 + _h / 2 - 20, BLACK);
      break;
    case 3:   // DOWN
      tft16bits.drawRoundRect(_x1, _y1, _w, _h, r, RED);
      tft16bits.fillTriangle(_x1 + _w - 10, _y1 + _h / 2, _x1 + 15, _y1 + _h / 2 + 20, _x1 + 15, _y1 + _h / 2 - 20, BLACK);
      break;
    case 4:   // START/PAUSE
      tft16bits.drawRoundRect(_x1, _y1, _w, _h, r, CYAN);
      if (DEMO == 1 || GAMEPAUSED == 1) {
        // Button Action is PLAY
        tft16bits.fillTriangle(_x1 + _w - 10, _y1 + _h / 2 + 15, _x1 + 10, _y1 + _h / 2 + 15, _x1 + _w / 2, _y1 + _h / 2 - 20, RED);
      } else if (GAMEPAUSED == 0) {
        // Button Action is PAUSE
        tft16bits.fillRect(_x1 + 10, _y1 + _h / 2 + 4, 40, 15, RED);
        tft16bits.fillRect(_x1 + 10, _y1 + 10, 40, 15, RED);
      }
      break;
  }
  _x1 = x0 + _w;
  _y1 = y0 + _h;
  bsp_lcd_flush(x0, y0, _x1, _y1, (void *)canvas);
  //delay(1);  // we don't check when it finishes drawing... so just wait few ms
  free(canvas);
}

void drawAllButtons() {
  for (uint8_t b = 0; b < BUT_NUM; b++) {
#if 1
    drawButtonFace(b);
#else
    // just rectagles as button - testing
    for (uint16_t y1 = 0; y1 < buttons[b][BUT_W]; y1++)
      for (uint16_t x1 = 0; x1 < buttons[b][BUT_H]; x1++) {
        screenBuffer[x0 + x1 + (y0 + y1) * SCR_WIDTH] = buttons[b][BUT_COLOR];
      }
#endif
  }
}

void setup() {
  lcd_driver_install();
  
/*
// ===> POR ALGUM MOTIVO alocar o PSRAM buffer aqui gera um ERRO no APP todo...
  if (!tft16bits.createPSRAMFrameBuffer()) {
    log_e("Can't allocate the screen buffer - check PSRAM.");
    while (1);
  }
  screenBuffer = tft16bits.getBuffer();
  //tft16bits.setRotation(3);
*/

#if 0
  // clear screen with black
  for (uint32_t i = 0; i < SCR_WIDTH * SCR_HEIGHT; i++)
    screenBuffer[i] = 0;
  printf("\n\nCLEAR SCREEN...");
  bsp_lcd_flush(0, 0, SCR_WIDTH - 1, SCR_HEIGHT - 1, (void *)screenBuffer);
  printf("\n\nCLEAR SCREEN... DONE!");
  delay(2000);
  printf("\nNExT...");
#endif
#if 0
{
  printf("\n Drawing again...START!\n");
  uint16_t tile[64];
  int xs = BOARD_DISP_PARALLEL_HRES / 8;
  int ys = BOARD_DISP_PARALLEL_VRES / 8;
  for (int y = 0; y < ys; y++)
    for (int x = 0; x < xs; x++) {
       uint16_t color = (y<<5) + (x<<1);
       memset(tile, color, sizeof(tile));
       //printf("drawing (%d, %d) color %x in screen\n===============\n", x*8, y*8, color);
       //bsp_lcd_flush(x * 8, y * 8, 8, 8, (void *)tile);
       bsp_lcd_flush( x * 8, y * 8, x * 8 + 8, y * 8 + 8, (void *)tile);

    }
  printf("\n Drawing again...DONE!\n");
  //delay(3000);
}
#endif
#if 0
  printf("\n NOVO REDESENHO DA TELA...\n");
  uint16_t tile[64];
  memset(tile, 0, sizeof(tile));
  int xs = SCR_WIDTH / 8;
  int ys = SCR_HEIGHT / 8;
  for (int y = 0; y < ys; y++)
    for (int x = 0; x < xs; x++) {
       //uint16_t color = (y<<6) + x;
       //memset(tile, color, sizeof(tile));
       //printf("\ndrawing (%d, %d) color %x in screen\n===============", x*8, y*8, 0);
       //lcd_parallel8080_draw(lcd_parallel8080, x * 8, y * 8, x * 8 + 7, y * 8 + 7, (void *)tile);
       bsp_lcd_flush(x * 8, y * 8, x * 8 + 8, y * 8 + 8, (void *)tile);
    }
  printf("\n\nCLEAR SCREEN... DONE ==> using rectangles... !");
  //delay(5000);
#endif
  // Draw touch screen buttons
  drawAllButtons();
  //  drawButton(_paletteW[15], 620, 255);  // UP
  //  drawButton(_paletteW[15], 680, 370);  // LEFT
  //  drawButton(_paletteW[15], 680, 140);  // RIGHT
  //  drawButton(_paletteW[15], 740, 255);  // DOWN

  //  drawButton(_paletteW[6], 640, 10);  // A
  //  drawButton(_paletteW[1], 720, 10);  // B

  //printf("\n====================\nPacman DEMO -- Starting.\n\n");

#if 0
#if(BOARD_TYPE == BOARD_TYPE_HMI)
  // Testing Audio System and Game Snd Fx
  // Game_Audio_Wav_Class pointer, Shall Interrupt Current sound, float sampleRateMultiplier
  delay(500); GameAudio.PlayWav(&pmChomp, false, 1.0);
  while (GameAudio.IsPlaying());  // wait until done
  delay(500); GameAudio.PlayWav(&pmWav, false, 1.0);
  while (GameAudio.IsPlaying());  // wait until done
  delay(500); GameAudio.PlayWav(&pmDeath, true, 1.0);
  while (GameAudio.IsPlaying());  // wait until done
  delay(500); GameAudio.PlayWav(&pmEatGhost, true, 1.0);
  while (GameAudio.IsPlaying());  // wait until done
#endif
#endif
}

void loop() {
  static uint32_t lastTime = 0;
  // calculate next game screen, considering Player interaction and game speed
  if (millis() > lastTime) {
    lastTime = millis() + 34; //34;
    _game.Step();
  }
  // copies ScrBuf to LCD
  //bsp_lcd_flush(0, 0, SCR_WIDTH - 1, SCR_HEIGHT - 1, (void *)screenBuffer);
  // checks Player interaction with TouchScreen
  uint8_t numTouchedPoints = 0;
  uint16_t scrTouchX = 0, scrTouchY = 0;
  if (touchPadRead(&numTouchedPoints, &scrTouchX, &scrTouchY) == ESP_OK) {
    //    Serial.printf("-- We got %d touched points...", numTouchedPoints);
    if (numTouchedPoints > 0) {
      int8_t pressedButton = getTouchedButton(scrTouchX, scrTouchY);
      //printf("X = %d | Y = %d -- BUTTON = %d\n", scrTouchX, scrTouchY, pressedButton);
      switch (pressedButton) {
        case 0:
          ClearKeys();
          but_UP = true;
          break;
        case 1:
          ClearKeys();
          but_LEFT = true;
          break;
        case 2:
          ClearKeys();
          but_RIGHT = true;
          break;
        case 3:
          ClearKeys();
          but_DOWN = true;
          break;
        case 4:
          if (debounceTimeStart < millis()) {
            debounceTimeStart = millis() + 250;
            but_A = true;
          }
          break;
        case 5:
          if (debounceTimeStart < millis()) {
            debounceTimeStart = millis() + 250;
            but_B = true;
          }
          break;
      }
    }
  }
}
