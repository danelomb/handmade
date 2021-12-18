#include "handmade.h"

internal void RenderWeirdGradient(game_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset)
{
  u8 *Row = (u8 *)Buffer->Memory;
  for(int Y = 0;
      Y < Buffer->Height;
      ++Y)
  {
    u32 *Pixel = (u32 *)Row;
    for(int X = 0;
        X < Buffer->Width;
        ++X)
    {
      u8 Blue = (X + BlueOffset);
      u8 Green = (Y + GreenOffset);

      *Pixel++ = ((Green << 8) | (Blue));
    }
    Row += Buffer->Pitch;
  }
}



internal void GameUpdateAndRender(game_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset)
{
  RenderWeirdGradient(Buffer, BlueOffset, GreenOffset);
}

