// LakeSnes
// https://github.com/dinkc64/LakeSnes
// https://github.com/angelo-wf/LakeSnes

/*
MIT License

Copyright (c) 2021-2023 angelo_wf and contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/


// must include -lm in compiler options!

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include <sys/types.h>
#include <sys/stat.h>
//#include "strings.h"

/*
#ifdef SDL2SUBDIR
#include "SDL2/SDL.h"
#else
#include "SDL.h"
#endif
*/


#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <time.h>

#include <sys/ioctl.h>
#include <linux/kd.h>
#include <termios.h>

int frame_divider = 5; // 60 FPS divided by this number
int frame_counter = 0;

unsigned long previous_clock = 0;

unsigned short screen_large_buffer[640*480];
unsigned short screen_small_buffer[320*240];
unsigned char screen_handheld = 0;
int screen_file = 0;

unsigned short sound_array[2048];
int sound_file = 0;
int sound_enable = 1;


// added
#include "apu.c"
#include "cart.c"
#include "cpu.c"
#include "cx4.c"
#include "dma.c"
#include "dsp.c"
#include "input.c"
#include "ppu.c"
#include "snes.c"
#include "snes_other.c"
#include "spc.c"
#include "statehandler.c"
#include "zip.c"
#include "tracing.c"

// original
#include "zip.h"
#include "snes.h"
#include "tracing.h"



/* depends on behaviour:
casting uintX_t to/from intX_t does 'expceted' unsigned<->signed conversion
  ((int8_t) 255) == -1
same with assignment
  int8_t a; a = 0xff; a == -1
overflow is handled as expected
  (uint8_t a = 255; a++; a == 0; uint8_t b = 0; b--; b == 255)
clipping is handled as expected
  (uint16_t a = 0x123; uint8_t b = a; b == 0x23)
giving non 0/1 value to boolean makes it 0/1
  (bool a = 2; a == 1)
giving out-of-range vaue to function parameter clips it in range
  (void test(uint8_t a) {...}; test(255 + 1); a == 0 within test)
int is at least 32 bits
shifting into sign bit makes value negative
  int a = ((int16_t) (0x1fff << 3)) >> 3; a == -1
*/

static struct {
  // audio
  int audioFrequency;
  int16_t* audioBuffer;
  // paths
  char* prefPath;
  char* pathSeparator;
  // snes, timing
  Snes* snes;
  float wantedFrames;
  int wantedSamples;
  // loaded rom
  bool loaded;
  char* romName;
  char* savePath;
  char* statePath;
} glb = {};

static uint8_t* readFile(const char* name, int* length);
static void loadRom(const char* path);
static void closeRom(void);
static void setPaths(const char* path);
static void setTitle(const char* path);
static bool checkExtention(const char* name, bool forZip);
static void playAudio(void);
static void renderScreen(void);
static void handleInput(int keyCode, bool pressed);

int main(int argc, char** argv) {

	for (unsigned long i=0; i<640*480; i++) screen_large_buffer[i] = 0;
	for (unsigned long i=0; i<320*240; i++) screen_small_buffer[i] = 0;

	char size_buffer[3];
	int size_file = open("/sys/class/graphics/fb0/virtual_size", O_RDONLY);
	read(size_file, &size_buffer, 3);
	close(size_file);

	if (size_buffer[0] == '3' && size_buffer[1] == '2' && size_buffer[2] == '0')
	{
		screen_handheld = 1;
	}
	else
	{
		screen_handheld = 0;
	}

	int buttons_file = 0;
	char buttons_buffer[13] = { '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0' };

	int sound_fragment = 0x00040009; // 4 blocks, each is 2^A = 2^10 = 1024
	int sound_stereo = 0;
	int sound_format = AFMT_S16_LE; // AFMT_U8;
	int sound_speed = (unsigned int)(((61543/2)/frame_divider)); // calculations: 61542.17 = 1024 * 60.0988 + 1

	sound_file = open("/dev/dsp", O_WRONLY);

	ioctl(sound_file, SNDCTL_DSP_SETFRAGMENT, &sound_fragment); // needed to stop the drift
	ioctl(sound_file, SNDCTL_DSP_STEREO, &sound_stereo);
	ioctl(sound_file, SNDCTL_DSP_SETFMT, &sound_format);
	ioctl(sound_file, SNDCTL_DSP_SPEED, &sound_speed);


  // init snes, load rom
  glb.snes = snes_init();
  glb.loaded = false;
  glb.romName = NULL;
  glb.savePath = NULL;
  glb.statePath = NULL;

  if(argc >= 2) {
    loadRom(argv[1]);
  } else {
    puts("No rom loaded");
  }

  bool running = true;

  while(running){

		frame_counter++;
		if (frame_counter >= frame_divider) frame_counter = 0;

		snes_setButtonState(glb.snes, 1, 0, false);
		snes_setButtonState(glb.snes, 1, 1, false);
		snes_setButtonState(glb.snes, 1, 2, false);
		snes_setButtonState(glb.snes, 1, 3, false);
		snes_setButtonState(glb.snes, 1, 4, false);
		snes_setButtonState(glb.snes, 1, 5, false);
		snes_setButtonState(glb.snes, 1, 6, false);
		snes_setButtonState(glb.snes, 1, 7, false);
		snes_setButtonState(glb.snes, 1, 8, false);
		snes_setButtonState(glb.snes, 1, 9, false);
		snes_setButtonState(glb.snes, 1, 10, false);
		snes_setButtonState(glb.snes, 1, 11, false);

		buttons_file = open("/home/username/LakeSnes/keyboard.val", O_RDONLY);
		read(buttons_file, &buttons_buffer, 13);
		close(buttons_file);

		// get key inputs
		if (buttons_buffer[0] != '0') running = false; // exit
		if (buttons_buffer[1] != '0') snes_setButtonState(glb.snes, 1, 4, true);
		if (buttons_buffer[2] != '0') snes_setButtonState(glb.snes, 1, 5, true);
		if (buttons_buffer[3] != '0') snes_setButtonState(glb.snes, 1, 6, true);
		if (buttons_buffer[4] != '0') snes_setButtonState(glb.snes, 1, 7, true);
		if (buttons_buffer[5] != '0') snes_setButtonState(glb.snes, 1, 2, true);
		if (buttons_buffer[6] != '0') snes_setButtonState(glb.snes, 1, 3, true);
		if (buttons_buffer[7] != '0') snes_setButtonState(glb.snes, 1, 8, true);
		if (buttons_buffer[8] != '0') snes_setButtonState(glb.snes, 1, 0, true);
		if (buttons_buffer[9] != '0') snes_setButtonState(glb.snes, 1, 9, true);
		if (buttons_buffer[10] != '0') snes_setButtonState(glb.snes, 1, 1, true);
		if (buttons_buffer[11] != '0') snes_setButtonState(glb.snes, 1, 10, true);
		if (buttons_buffer[12] != '0') snes_setButtonState(glb.snes, 1, 11, true);

		buttons_file = open("/home/username/LakeSnes/joystick.val", O_RDONLY);
		read(buttons_file, &buttons_buffer, 13);
		close(buttons_file);

		// get joy inputs
		if (buttons_buffer[0] != '0') running = false; // exit
		if (buttons_buffer[1] != '0') snes_setButtonState(glb.snes, 1, 4, true);
		if (buttons_buffer[2] != '0') snes_setButtonState(glb.snes, 1, 5, true);
		if (buttons_buffer[3] != '0') snes_setButtonState(glb.snes, 1, 6, true);
		if (buttons_buffer[4] != '0') snes_setButtonState(glb.snes, 1, 7, true);
		if (buttons_buffer[5] != '0') snes_setButtonState(glb.snes, 1, 2, true);
		if (buttons_buffer[6] != '0') snes_setButtonState(glb.snes, 1, 3, true);
		if (buttons_buffer[7] != '0') snes_setButtonState(glb.snes, 1, 8, true);
		if (buttons_buffer[8] != '0') snes_setButtonState(glb.snes, 1, 0, true);
		if (buttons_buffer[9] != '0') snes_setButtonState(glb.snes, 1, 9, true);
		if (buttons_buffer[10] != '0') snes_setButtonState(glb.snes, 1, 1, true);
		if (buttons_buffer[11] != '0') snes_setButtonState(glb.snes, 1, 10, true);
		if (buttons_buffer[12] != '0') snes_setButtonState(glb.snes, 1, 11, true);

        snes_runFrame(glb.snes);

		playAudio();

		if (frame_counter == 0)
		{
			if (sound_enable > 0)
			{
				// write audio to /dev/fb0
				write(sound_file, &sound_array, 1024); // AUDIO_SAMPLES
			}

       		renderScreen();

			while (clock() < previous_clock + 16667 * frame_divider) { }
			previous_clock = clock();
		}
  }

  // close rom (saves battery)
  closeRom();

  // free snes
  snes_free(glb.snes);
  free(glb.audioBuffer);
  if(glb.romName) free(glb.romName);
  if(glb.savePath) free(glb.savePath);
  if(glb.statePath) free(glb.statePath);
  return 0;
}

static void playAudio() {

	if (sound_enable > 0)
	{
		snes_setSamples(glb.snes, (unsigned short *)(sound_array + (512 * ((frame_counter-1)%frame_divider)) / frame_divider), 512 / frame_divider);
	}
}

static void renderScreen() {

	unsigned short color = 0;

	if (screen_handheld == 0)
	{
		for (int y=0; y<224; y++)
		{
			for (int x=0; x<256; x++)
			{
				color = (unsigned short)(0x0000 |
					((glb.snes->ppu->pixelBuffer[y * 256 * 4 + x * 4 + 0] & 0xF8) >> 3) |
					((glb.snes->ppu->pixelBuffer[y * 256 * 4 + x * 4 + 1] & 0xFC) << 3) |
					((glb.snes->ppu->pixelBuffer[y * 256 * 4 + x * 4 + 2] & 0xF8) << 8));

				screen_large_buffer[(y) * 640 * 2 + (x) * 2] = (unsigned short)color;
				screen_large_buffer[(y) * 640 * 2 + (x) * 2 + 1] = (unsigned short)color;
				screen_large_buffer[(y) * 640 * 2 + 640 + (x) * 2] = (unsigned short)color;
				screen_large_buffer[(y) * 640 * 2 + 640 + (x) * 2 + 1] = (unsigned short)color;
			}
		}

		screen_file = open("/dev/fb0", O_RDWR);
		write(screen_file, &screen_large_buffer, 640*480*2);
		close(screen_file);
	}
	else
	{
		for (int y=0; y<224; y++)
		{
			for (int x=0; x<256; x++)
			{
				color = (unsigned short)(0x0000 |
					((glb.snes->ppu->pixelBuffer[y * 256 * 4 + x * 4 + 0] & 0xF8) >> 3) |
					((glb.snes->ppu->pixelBuffer[y * 256 * 4 + x * 4 + 1] & 0xFC) << 3) |
					((glb.snes->ppu->pixelBuffer[y * 256 * 4 + x * 4 + 2] & 0xF8) << 8));

				screen_small_buffer[(y) * 320 + (x)] = (unsigned short)color;
			}
		}

		screen_file = open("/dev/fb0", O_RDWR);
		write(screen_file, &screen_small_buffer, 320*240*2);
		close(screen_file);
	}
}

static bool checkExtention(const char* name, bool forZip) {
  if(name == NULL) return false;
  int length = strlen(name);
  if(length < 4) return false;
  if(forZip) {
    if(strcasecmp(name + length - 4, ".zip") == 0) return true;
  } else {
    if(strcasecmp(name + length - 4, ".smc") == 0) return true;
    if(strcasecmp(name + length - 4, ".sfc") == 0) return true;
  }
  return false;
}

static void loadRom(const char* path) {
  // zip library from https://github.com/kuba--/zip
  int length = 0;
  uint8_t* file = NULL;
  if(checkExtention(path, true)) {
    struct zip_t* zip = zip_open(path, 0, 'r');
    if(zip != NULL) {
      int entries = zip_total_entries(zip);
      for(int i = 0; i < entries; i++) {
        zip_entry_openbyindex(zip, i);
        const char* zipFilename = zip_entry_name(zip);
        if(checkExtention(zipFilename, false)) {
          printf("Read \"%s\" from zip\n", zipFilename);
          size_t size = 0;
          zip_entry_read(zip, (void**) &file, &size);
          length = (int) size;
          break;
        }
        zip_entry_close(zip);
      }
      zip_close(zip);
    }
  } else {
    file = readFile(path, &length);
  }
  if(file == NULL) {
    printf("Failed to read file '%s'\n", path);
    return;
  }
  // close currently loaded rom (saves battery)
  closeRom();

  // load new rom
  if(snes_loadRom(glb.snes, file, length)) {

    // get rom name and paths, set title
    setPaths(path);
    setTitle(glb.romName);

    // set wantedFrames and wantedSamples
    glb.wantedFrames = 1.0 / (glb.snes->palTiming ? 50.0 : 60.0);
    glb.wantedSamples = glb.audioFrequency / (glb.snes->palTiming ? 50 : 60);
    glb.loaded = true;
    // load battery for loaded rom
    int size = 0;
    uint8_t* saveData = readFile(glb.savePath, &size);
    if(saveData != NULL) {
      if(snes_loadBattery(glb.snes, saveData, size)) {
        puts("Loaded battery data");
      } else {
        puts("Failed to load battery data");
      }
      free(saveData);
    }
  } // else, rom load failed, old rom still loaded

  free(file);
}

static void closeRom() {
  if(!glb.loaded) return;
  int size = snes_saveBattery(glb.snes, NULL);
  if(size > 0) {
    uint8_t* saveData = malloc(size);
    snes_saveBattery(glb.snes, saveData);
    FILE* f = fopen(glb.savePath, "wb");
    if(f != NULL) {
      fwrite(saveData, size, 1, f);
      fclose(f);
      puts("Saved battery data");
    } else {
      puts("Failed to save battery data");
    }
    free(saveData);
  }
}

static void setPaths(const char* path) {
  // get rom name
  if(glb.romName) free(glb.romName);
  const char* filename = NULL; //strrchr(path, glb.pathSeparator[0]); // get last occurence of '/' or '\'
  if(filename == NULL) {
    filename = path;
  } else {
    filename += 1; // skip past '/' or '\' itself
  }
  glb.romName = malloc(strlen(filename) + 1); // +1 for '\0'
  strcpy(glb.romName, filename);

/*
  // get extension length
  const char* extStart = strrchr(glb.romName, '.'); // last occurence of '.'
  int extLen = extStart == NULL ? 0 : strlen(extStart);
  // get save name
  if(glb.savePath) free(glb.savePath);
  glb.savePath = malloc(strlen(glb.prefPath) + strlen(glb.romName) + 11); // "saves/" (6) + ".srm" (4) + '\0'
  strcpy(glb.savePath, glb.prefPath);
  strcat(glb.savePath, "saves");
  strcat(glb.savePath, glb.pathSeparator);
  strncat(glb.savePath, glb.romName, strlen(glb.romName) - extLen); // cut off extension
  strcat(glb.savePath, ".srm");
  // get state name
  if(glb.statePath) free(glb.statePath);
  glb.statePath = malloc(strlen(glb.prefPath) + strlen(glb.romName) + 12); // "states/" (7) + ".lss" (4) + '\0'
  strcpy(glb.statePath, glb.prefPath);
  strcat(glb.statePath, "states");
  strcat(glb.statePath, glb.pathSeparator);
  strncat(glb.statePath, glb.romName, strlen(glb.romName) - extLen); // cut off extension
  strcat(glb.statePath, ".lss");
*/
}

static void setTitle(const char* romName) {

}

static uint8_t* readFile(const char* name, int* length) {
  FILE* f = fopen(name, "rb");
  if(f == NULL) return NULL;
  fseek(f, 0, SEEK_END);
  int size = ftell(f);
  rewind(f);
  uint8_t* buffer = malloc(size);
  if(fread(buffer, size, 1, f) != 1) {
    fclose(f);
    return NULL;
  }
  fclose(f);
  *length = size;
  return buffer;
}
