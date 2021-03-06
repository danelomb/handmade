#include <windows.h>
#include <stdint.h>
#include <xinput.h>
#include <dsound.h>
#include <math.h>
#include <stdio.h>


#define internal static
#define local_persist static
#define global_variable static
#define Pi32 3.14159265359f

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef i32 bool32;


typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float real32;
typedef double real64;

#include "handmade.cpp"

struct win32_offscreen_buffer
{
  BITMAPINFO Info;
  void *Memory;
  int Width;
  int Height;
  int Pitch;
};

struct win32_window_dimensions
{
  int Width;
  int Height;
};

struct win32_sound_output
{
  //Sound Test
  int SamplesPerSecond;
  int ToneHz;
  u32 RunningSampleIndex;
  int WavePeriod;
  int BytesPerSample;
  int SecondaryBufferSize;
  int ToneVolume;
};

global_variable bool GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;


//XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex,  XINPUT_STATE* pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
  return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

//XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
  return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

//DirectSoundCreate
#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

internal void Win32LoadXInput(void)
{
  HMODULE XInputLibrary = LoadLibrary("xinput1_4.dll");

  if(!XInputLibrary)
  {
    XInputLibrary = LoadLibrary("xinput1_3.dll");
  }


  if(XInputLibrary)
  {
    XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
    XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
  }
}

internal void Win32InitDSound(HWND Window, i32 SamplesPerSecond, i32 BufferSize)
{
  //Load Library
  HMODULE DSoundLibrary = LoadLibrary("dsound.dll");

  if(DSoundLibrary)
  {
    //Get a DirectSound object
    direct_sound_create *DirectSoundCreate = (direct_sound_create *)GetProcAddress(DSoundLibrary, "DirectSoundCreate");

    //TODO(dane):dounble check that this works on xp, DirectSound8, or 7.
    LPDIRECTSOUND DirectSound;
    if(DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
    {
      //set the wave format
      tWAVEFORMATEX WaveFormat = {};
      WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
      WaveFormat.nChannels = 2;
      WaveFormat.nSamplesPerSec = SamplesPerSecond;
      WaveFormat.wBitsPerSample = 16;
      WaveFormat.nBlockAlign = (WaveFormat.nChannels*WaveFormat.wBitsPerSample) / 8;
      WaveFormat.nAvgBytesPerSec = WaveFormat.nBlockAlign*WaveFormat.nSamplesPerSec;
      WaveFormat.cbSize = 0;

      if(SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
      {

        //"Create" a primary buffer
        DSBUFFERDESC BufferDescription = {};
        BufferDescription.dwSize = sizeof(BufferDescription);
        BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;
        LPDIRECTSOUNDBUFFER PrimaryBuffer;
        if(SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
        {
          if(SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat)))
          {
            OutputDebugStringA("Primary buffer set\n");
            //We have finally set the format!
          }
          else
          {
            //TODO(dane):Diagnostic
          }
        }
      }
      //Create a secondary buffer
      DSBUFFERDESC BufferDescription = {};
      BufferDescription.dwSize = sizeof(BufferDescription);
      BufferDescription.dwFlags = 0;
      BufferDescription.dwBufferBytes = BufferSize;
      BufferDescription.lpwfxFormat = &WaveFormat;

      if(SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0)))
      {
        OutputDebugStringA("Secondary buffer created successfully\n");
      }
      else
      {
        //TODO(dane):Diagnostic
      }
    }
    else
    {
      //TODO(dane):Diagnostic
    }
  }
  else
  {
    //TODO(dane):Diagnostic
  }
}

internal win32_window_dimensions GetWindowDimension(HWND Window)
{
  win32_window_dimensions Result;
  RECT ClientRect;
  GetClientRect(Window, &ClientRect);
  Result.Width = ClientRect.right - ClientRect.left;
  Result.Height = ClientRect.bottom - ClientRect.top;
  return(Result);
}

internal void RenderWeirdGradient(win32_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset)
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

internal void Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height)
{
  if(Buffer->Memory)
  {
    VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
  }

  Buffer->Width = Width;
  Buffer->Height = Height;
  int BytesPerPixel = 4;

  Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
  Buffer->Info.bmiHeader.biWidth = Buffer->Width;
  Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
  Buffer->Info.bmiHeader.biPlanes = 1;
  //8 bits for each color and an extra 8 to make sure that pixels are aligned on 4 byte boundaries
  Buffer->Info.bmiHeader.biBitCount = 32;
  Buffer->Info.bmiHeader.biCompression = BI_RGB;

  int BitmapMemorySize = (Buffer->Width*Buffer->Height)*BytesPerPixel;
  Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
  //How many bytes a pointer has to move to go from 1 row to the next
  Buffer->Pitch = Buffer->Width*BytesPerPixel;
}



internal void Win32DisplayBufferInWindow(HDC DeviceContext, int Width, int Height,
                                         win32_offscreen_buffer *Buffer)
{
  //Takes rectangle of 1 size in 1 buffer and copies it to rectangle of other size in another buffer.
  StretchDIBits(DeviceContext,
                0, 0, Width, Height,
                0, 0, Buffer->Width, Buffer->Height,
                Buffer->Memory,
                &Buffer->Info,
                DIB_RGB_COLORS, SRCCOPY);
}


  void
Win32FillSoundBuffer(win32_sound_output *SoundOutput, DWORD ByteToLock, DWORD BytesToWrite)
{
  VOID *Region1;
  DWORD Region1Size;
  VOID *Region2;
  DWORD Region2Size;
  real32 TimeCounter = 0;
  if(SUCCEEDED(GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite,
                                           &Region1, &Region1Size,
                                           &Region2, &Region2Size,
                                           0)))
  {
    i16 *SampleOut =(i16 *)Region1;
    DWORD Region1SampleCount = Region1Size/SoundOutput->BytesPerSample;

    for(DWORD SampleIndex = 0;
        SampleIndex < Region1SampleCount;
        ++SampleIndex)
    {
      real32 t =2.0f*Pi32*(real32)SoundOutput->RunningSampleIndex / (real32)SoundOutput->WavePeriod;
      real32 SineValue = sinf(t);
      i16 SampleValue = i16(SineValue * SoundOutput->ToneVolume);
      *SampleOut++ = SampleValue;
      *SampleOut++ = SampleValue;
      SoundOutput->RunningSampleIndex++;
    }

    DWORD Region2SampleCount = Region2Size/SoundOutput->BytesPerSample;
    SampleOut =(i16 *)Region2;
    for(DWORD SampleIndex = 0;
        SampleIndex < Region2SampleCount;
        ++SampleIndex)
    {
      real32 t =2.0f*Pi32*(real32)SoundOutput->RunningSampleIndex / (real32)SoundOutput->WavePeriod;
      real32 SineValue = sinf(t);
      i16 SampleValue = i16(SineValue * SoundOutput->ToneVolume);
      *SampleOut++ = SampleValue;
      *SampleOut++ = SampleValue;
      SoundOutput->RunningSampleIndex++;
    }
  }
  GlobalSecondaryBuffer->Unlock( Region1, Region1Size, Region2, Region2Size);

}

  LRESULT CALLBACK
Win32MainWindowCallback(HWND   Window,
                        UINT   Message,
                        WPARAM WParam,
                        LPARAM LParam)
{
  LRESULT Result = 0;

  switch(Message)
  {
    case WM_SIZE:
      {
      } break;

    case WM_CLOSE:
      {
        //TODO(dane): Handle as message to user?
        GlobalRunning = false;
      } break;


    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYUP:
    case WM_KEYDOWN:
      {
        u32 VKCode = WParam;
        bool WasDown = ((LParam & (1 << 30)) != 0);
        bool IsDown = ((LParam & (1 << 31)) == 0);
        if(WasDown != IsDown)
        {
          if(VKCode == 'W')
          {
          }
          else if(VKCode == 'A')
          {
          }
          else if(VKCode == 'S')
          {
          }
          else if(VKCode == 'D')
          {
          }
          else if(VKCode == 'Q')
          {
          }
          else if(VKCode == 'E')
          {
          }


          else if(VKCode == VK_UP)
          {
          }
          else if(VKCode == VK_LEFT)
          {
          }
          else if(VKCode == VK_RIGHT)
          {
          }
          else if(VKCode == VK_DOWN)
          {
          }
          else if(VKCode == VK_ESCAPE)
          {
            OutputDebugStringA("Escape ");
            if(IsDown)
            {
              OutputDebugStringA("IsDown");
            }
            if(WasDown)
            {
              OutputDebugStringA("WasDown");
            }
            OutputDebugStringA("\n");
          }
          else if(VKCode == VK_SPACE)
          {
          }
        }
        bool32 AltKeyWasDown = ((LParam & (1 << 29)) != 0);
        if(AltKeyWasDown && (VKCode == VK_F4))
        {
          GlobalRunning = false;
        }

      } break;

    case WM_DESTROY:
      {
        //TODO(dane): Handle as error - recreate window?
        GlobalRunning = false;
      } break;

    case WM_ACTIVATEAPP:
      {
        OutputDebugStringA("WM_ACTIVATEAPP\n");
      } break;

    case WM_PAINT:
      {
        PAINTSTRUCT Paint;
        HDC DeviceContext = BeginPaint(Window, &Paint);
        win32_window_dimensions Dimension = GetWindowDimension(Window);
        Win32DisplayBufferInWindow(DeviceContext, Dimension.Width, Dimension.Height,
                                   &GlobalBackBuffer);

        EndPaint(Window, &Paint);
      } break;
    default :
      {
        //OutputDebugStringA("default\n");
        Result = DefWindowProc(Window, Message, WParam, LParam);
      } break;
  }
  return(Result);
}





//Windows entry point
  int CALLBACK
WinMain(HINSTANCE Instance,
        HINSTANCE PrevInstance,
        LPSTR     CommandLine,
        int       ShowCode)

{

  Win32LoadXInput();

  WNDCLASSA WindowClass = {};
  WindowClass.style = CS_HREDRAW|CS_VREDRAW;
  WindowClass.lpfnWndProc = Win32MainWindowCallback;
  WindowClass.hInstance = Instance;
  //WindowClass.hIcon = ;
  WindowClass.lpszClassName = "HandmadeHeroWindowClass";

  Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);

  if(RegisterClass(&WindowClass))
  {
    HWND Window =
      CreateWindowExA(0,
                      WindowClass.lpszClassName,
                      "Handmade Hero",
                      WS_OVERLAPPEDWINDOW|WS_VISIBLE,
                      CW_USEDEFAULT,
                      CW_USEDEFAULT,
                      CW_USEDEFAULT,
                      CW_USEDEFAULT,
                      0,
                      0,
                      Instance,
                      0);

    if(Window)
    {

      HDC DeviceContext = GetDC(Window);
      //Graphics Test
      int XOffset = 0;
      int YOffset = 0;

      win32_sound_output SoundOutput = {};

      SoundOutput.SamplesPerSecond = 48000;
      SoundOutput.ToneHz = 256;
      SoundOutput.RunningSampleIndex = 0;
      SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond/SoundOutput.ToneHz;
      SoundOutput.BytesPerSample = sizeof(i16)*2;
      SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond*SoundOutput.BytesPerSample;
      SoundOutput.ToneVolume = 600;

      Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
      Win32FillSoundBuffer(&SoundOutput, 0, SoundOutput.SecondaryBufferSize);
      GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

      LARGE_INTEGER LastCounter;
      QueryPerformanceCounter(&LastCounter);
      u64 LastCycleCount = __rdtsc();

      GlobalRunning = true;

      LARGE_INTEGER PerformanceFrequencyResult;
      QueryPerformanceFrequency(&PerformanceFrequencyResult);
      i64 PerfCountFrequency = PerformanceFrequencyResult.QuadPart;

      //Main loop for window: Checks for message from window. If a message is found, jump to Win32MainWindowCallback.
      while(GlobalRunning)
      {
        LARGE_INTEGER BeginCounter;
        QueryPerformanceCounter(&BeginCounter);
        BeginCounter.QuadPart;


        MSG Message;
        while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
        {
          if(Message.message == WM_QUIT)
          {
            GlobalRunning = false;
          }

          TranslateMessage(&Message);
          DispatchMessage(&Message);
        }

        //TODO(dane):poll more frequently?
        //TODO(dane):preferably only poll controllers that are in use.
        for (DWORD ControllerIndex=0;
             ControllerIndex< XUSER_MAX_COUNT;
             ControllerIndex++ )
        {
          XINPUT_STATE ControllerState;
          if(XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
          {
            OutputDebugStringA("Controller");
            //Controller is plugged in
            XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

            bool Up            = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
            bool Down          = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
            bool Left          = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
            bool Right         = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
            bool Start         = (Pad->wButtons & XINPUT_GAMEPAD_START);
            bool Back          = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
            bool LeftShoulder  = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
            bool RightShoulder = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
            bool AButton       = (Pad->wButtons & XINPUT_GAMEPAD_A);
            bool BButton       = (Pad->wButtons & XINPUT_GAMEPAD_B);
            bool XButton       = (Pad->wButtons & XINPUT_GAMEPAD_X);
            bool YButton       = (Pad->wButtons & XINPUT_GAMEPAD_Y);

            i16 StickX = Pad->sThumbLX;
            i16 StickY = Pad->sThumbLY;

            XOffset += StickX >> 14;
            YOffset -= StickY >> 14;

          }
          else
          {
            //Controller not available
          }
        }


        game_offscreen_buffer Buffer = {};
        Buffer.Memory = GlobalBackBuffer.Memory;
        Buffer.Width = GlobalBackBuffer.Width;
        Buffer.Height = GlobalBackBuffer.Height;
        Buffer.Pitch = GlobalBackBuffer.Pitch;

        GameUpdateAndRender(&Buffer, XOffset, YOffset);

        DWORD PlayCursor;
        DWORD WriteCursor;
        //DirectSound Output test

        if(SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition( &PlayCursor, &WriteCursor )))
        {
          DWORD ByteToLock = (SoundOutput.RunningSampleIndex*SoundOutput.BytesPerSample) % SoundOutput.SecondaryBufferSize;
          DWORD BytesToWrite;

          if(ByteToLock > PlayCursor)
          {
            BytesToWrite = SoundOutput.SecondaryBufferSize - ByteToLock;
            BytesToWrite += PlayCursor;
          }
          else
          {
            BytesToWrite = PlayCursor - ByteToLock;
          }

          Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite);
        }
        win32_window_dimensions Dimension = GetWindowDimension(Window);
        Win32DisplayBufferInWindow(DeviceContext, Dimension.Width, Dimension.Height,
                                   &GlobalBackBuffer);
        ReleaseDC(Window, DeviceContext);

        u64 EndCycleCount = __rdtsc();
        LARGE_INTEGER EndCounter;
        QueryPerformanceCounter(&EndCounter);

        u64 CyclesElapsed = EndCycleCount - LastCycleCount;
        i64 CounterElapsed = EndCounter.QuadPart - LastCounter.QuadPart;
        real32 MSPerFrame = real32(((1000.0f*(real32)CounterElapsed) / (real32)PerfCountFrequency));
        real32 FPS = ((real32)PerfCountFrequency / (real32)CounterElapsed);
        real32 MCPF = ((real32)CyclesElapsed/(1000.0f*1000.0f));
#if 0
        char Buffer[256];
        sprintf(Buffer, "%.2fms/f, %.2fFPS,   %.2fMc/f\n", MSPerFrame, FPS, MCPF);
        OutputDebugStringA(Buffer);
#endif
        LastCounter = EndCounter;
        LastCycleCount = EndCycleCount;
      }

    }
    else
    {
      //TODO(dane): Logging
    }
    return(0);
  }
  else
  {
    //TODO(dane): Logging
  }
  return(0);
}
