/**
 * MIT License
 * Copyright (c) 2018-2023 Mahyar Koshkouei
 *
 * An example of using the peanut_gb.h library. This example application uses
 * SDL2 to draw the screen and get input.
 */

#include <stdint.h>
#include <stdlib.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <time.h>

unsigned char cart_rom[0x800000]; // 8 MB max
unsigned char cart_ram[32768]; // used for NES, GB, and SMS

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
unsigned short screen_buffer[SCREEN_WIDTH*SCREEN_HEIGHT];

#define SCREEN_LARGE_WIDTH 640
#define SCREEN_LARGE_HEIGHT 480
unsigned short screen_large_buffer[SCREEN_LARGE_WIDTH*SCREEN_LARGE_HEIGHT];

#define AUDIO_LENGTH 8192
unsigned char audio_buffer[AUDIO_LENGTH];
unsigned char audio_enable = 1;

unsigned char speed_limiter = 1;

unsigned char turbo_state = 0;
unsigned long turbo_counter = 0;
unsigned char turbo_a = 0;
unsigned char turbo_b = 0;

#include "minigb_apu.h"
#include "minigb_apu.c"

#include "peanut_gbc.h" // color game boy version


struct priv_t
{
	uint8_t dummy;
};


uint8_t boot_rom[256];
uint16_t selected_palette_lcd[3][7];

unsigned char reset_check = 0;
unsigned char palette_num = 0;

unsigned char rom_in_sqi = 0; // change to read from sqi instead

// Returns a byte from the ROM file at the given address.
uint8_t gb_rom_read(struct gb_s *gb, const uint_fast32_t addr)
{
	return cart_rom[addr];
}

uint8_t gb_boot_rom_read(struct gb_s *gb, const uint_fast16_t addr)
{
	return boot_rom[addr];
}

// Returns a byte from the cartridge RAM at the given address.
uint8_t gb_cart_ram_read(struct gb_s *gb, const uint_fast32_t addr)
{
	return cart_ram[(addr&0x7FFF)];
}

// Writes a given byte to the cartridge RAM at the given address.
void gb_cart_ram_write(struct gb_s *gb, const uint_fast32_t addr,
		       const uint8_t val)
{
	cart_ram[(addr&0x7FFF)] = val;
}

unsigned char gb_write_cart_ram_file(const char *filename)
{
	FILE *output = NULL;

	output = fopen(filename, "wb");
	if (!output) return 0;

	for (int i=0; i<32768; i++)
	{
		fprintf(output, "%c", cart_ram[i]);
	}

	fclose(output);

	return 1;
}

unsigned char gb_read_cart_ram_file(const char *filename)
{
	FILE *input = NULL;

	input = fopen(filename, "rb");
	if (!input) return 0;

	for (int i=0; i<32768; i++)
	{
		fscanf(input, "%c", &cart_ram[i]);
	}

	fclose(input);

	return 1;
}

/**
 * Handles an error reported by the emulator. The emulator context may be used
 * to better understand why the error given in gb_err was reported.
 */
void gb_error(struct gb_s *gb, const enum gb_error_e gb_err, const uint16_t addr)
{
/*
	const char* gb_err_str[GB_INVALID_MAX] = {
		"UNKNOWN\\",
		"INVALID OPCODE\\",
		"INVALID READ\\",
		"INVALID WRITE\\",
		"HALT FOREVER\\",
		"INVALID_SIZE\\"
	};
	
	uint8_t instr_byte;

	instr_byte = __gb_read(gb, addr);
	
	unsigned long long_addr = addr;

	if(addr >= 0x4000 && addr < 0x8000)
	{
		long_addr = (uint32_t)addr * (uint32_t)gb->selected_rom_bank;
	}
*/
	printf("Error\n");

	while (1) { }
}

/**
 * Automatically assigns a colour palette to the game using a given game
 * checksum.
 * TODO: Not all checksums are programmed in yet because I'm lazy.
 */
const unsigned short preset_palette_double[16][21] = {
	{0xE718,0xA510,0x8410,0x6308,0x4208,0x2100,0x0000,0xE718,0xA510,0x8410,0x6308,0x4208,0x2100,0x0000,0xE718,0xA510,0x8410,0x6308,0x4208,0x2100,0x0000}, // greyscale
	{0x6400,0x4300,0x4308,0x2208,0x2208,0x2200,0x2200,0x6400,0x4300,0x4308,0x2208,0x2208,0x2200,0x2200,0x6400,0x4300,0x4308,0x2208,0x2208,0x2200,0x2200}, // gb original
	{0xC610,0xA508,0x8408,0x6300,0x4200,0x2100,0x0000,0xC610,0xA508,0x8408,0x6300,0x4200,0x2100,0x0000,0xC610,0xA508,0x8408,0x6300,0x4200,0x2100,0x0000}, // gb pocket
	{0x0510,0x0408,0x0408,0x0308,0x0308,0x0200,0x0200,0x0510,0x0408,0x0408,0x0308,0x0308,0x0200,0x0200,0x0510,0x0408,0x0408,0x0308,0x0308,0x0200,0x0200}, // gb light
	{0xE718,0x8708,0x4700,0x8400,0xE200,0x6100,0x0000,0xE718,0x8708,0x4700,0x8400,0xE200,0x6100,0x0000,0xE718,0x8708,0x4700,0x8400,0xE200,0x6100,0x0000},
	{0xE718,0xE708,0xE700,0xE300,0xE000,0x6000,0x0000,0xE718,0xE708,0xE700,0xE300,0xE000,0x6000,0x0000,0xE718,0xE708,0xE700,0xE300,0xE000,0x6000,0x0000},
	{0xE718,0xE610,0xE508,0xA300,0x8100,0x4000,0x0000,0xE718,0xE610,0xE508,0xA300,0x8100,0x4000,0x0000,0xE718,0xE610,0xE508,0xA300,0x8100,0x4000,0x0000},
	{0x0000,0x0208,0x0410,0x6508,0xE600,0xE608,0xE718,0x0000,0x0208,0x0410,0x6508,0xE600,0xE608,0xE718,0x0000,0x0208,0x0410,0x6508,0xE600,0xE608,0xE718},
	{0xE718,0xC610,0xA510,0x6308,0x4208,0x2100,0x0000,0xE718,0xC610,0xA510,0x6308,0x4208,0x2100,0x0000,0xE718,0xC610,0xA510,0x6308,0x4208,0x2100,0x0000},
	{0xE710,0xE510,0xE410,0xA410,0x8418,0x4208,0x0000,0xE710,0xE510,0xE410,0xA410,0x8418,0x4208,0x0000,0xE710,0xE510,0xE410,0xA410,0x8418,0x4208,0x0000},
	{0xE718,0xE610,0xE508,0xA300,0x8100,0x4000,0x0000,0xE718,0xE610,0xE508,0xA300,0x8100,0x4000,0x0000,0xE718,0xC510,0xC410,0xA308,0x8300,0x6200,0x4100},
	{0xE718,0xE510,0xE410,0xA208,0x8100,0x4000,0x0000,0xE718,0xE510,0xE410,0xA208,0x8100,0x4000,0x0000,0xE718,0xA708,0x6700,0x2508,0x0318,0x0108,0x0000},
	{0xE718,0xE510,0xE410,0xA208,0x8100,0x4000,0x0000,0xE718,0xE610,0xE508,0xA300,0x8100,0x4000,0x0000,0xE718,0xA518,0x8418,0x6310,0x4210,0x2108,0x0000},
	{0xE718,0xA708,0x6700,0x2500,0x0400,0x0200,0x0000,0xE718,0xA618,0x6518,0x2218,0x0018,0x0008,0x0000,0xE718,0xE510,0xE410,0xA208,0x8100,0x4000,0x0000},
	{0xE718,0xE510,0xE410,0xA208,0x8100,0x4000,0x0000,0xE718,0xA708,0x6700,0x2500,0x0400,0x0200,0x0000,0xE718,0xA618,0x6518,0x2218,0x0018,0x0008,0x0000},
	{0xE718,0xA618,0x6518,0x2218,0x0018,0x0008,0x0000,0xE718,0xA708,0x6700,0x2500,0x0400,0x0200,0x0000,0xE718,0xE708,0xE700,0xA400,0x6200,0x2100,0x0000}
};

void auto_assign_palette(uint8_t game_checksum, unsigned char val)
{	
	for (int i=0; i<3; i++)
	{
		for (int j=0; j<7; j++)
		{
			selected_palette_lcd[i][j] = preset_palette_double[val][i*7+j];
		}
	}
}


#if ENABLE_LCD
/**
 * Draws scanline into framebuffer.
 */
void lcd_draw_line(struct gb_s *gb, 
		//const uint8_t pixels1[160],
		//const uint8_t pixels2[160],
		const uint_fast8_t line
	)
{	
    if (gb->cgb.cgbMode) // CGB
	{
		unsigned long comp_red[2][2], comp_green[2][2], comp_blue[2][2];
		
		unsigned short comp = 0;

		if (scanline_scaled == 0)
		{
			if (scanline_handheld == 0)
			{
				for(unsigned int x = 0; x < LCD_WIDTH; x++)
				{
					comp = ((gb->cgb.fixPalette[scanline_pixels1[x]] & 0x7FE0) << 1) | (gb->cgb.fixPalette[scanline_pixels1[x]] & 0x001F);

					screen_large_buffer[(x*2) + (line*2) * SCREEN_LARGE_WIDTH] = comp;
					screen_large_buffer[(x*2+1) + (line*2) * SCREEN_LARGE_WIDTH] = comp;

					screen_large_buffer[(x*2) + (line*2+1) * SCREEN_LARGE_WIDTH] = comp;
					screen_large_buffer[(x*2+1) + (line*2+1) * SCREEN_LARGE_WIDTH] = comp;
				}
			}
			else if (scanline_handheld == 1)
			{
				for(unsigned int x = 0; x < LCD_WIDTH; x++)
				{
					screen_buffer[(x) + (line) * SCREEN_WIDTH] = ((gb->cgb.fixPalette[scanline_pixels1[x]] & 0x7FE0) << 1) | (gb->cgb.fixPalette[scanline_pixels1[x]] & 0x001F);
				}
			}
		}
		else
		{
			if (scanline_handheld == 0)
			{
				for(unsigned int x = 0; x < LCD_WIDTH; x++)
				{
					comp = ((gb->cgb.fixPalette[scanline_pixels1[x]] & 0x7FE0) << 1) | (gb->cgb.fixPalette[scanline_pixels1[x]] & 0x001F);

					screen_large_buffer[(x*3) + (line*3) * SCREEN_LARGE_WIDTH] = comp;
					screen_large_buffer[(x*3+1) + (line*3) * SCREEN_LARGE_WIDTH] = comp;
					screen_large_buffer[(x*3+2) + (line*3) * SCREEN_LARGE_WIDTH] = comp;

					screen_large_buffer[(x*3) + (line*3+1) * SCREEN_LARGE_WIDTH] = comp;
					screen_large_buffer[(x*3+1) + (line*3+1) * SCREEN_LARGE_WIDTH] = comp;
					screen_large_buffer[(x*3+2) + (line*3+1) * SCREEN_LARGE_WIDTH] = comp;

					screen_large_buffer[(x*3) + (line*3+2) * SCREEN_LARGE_WIDTH] = comp;
					screen_large_buffer[(x*3+1) + (line*3+2) * SCREEN_LARGE_WIDTH] = comp;
					screen_large_buffer[(x*3+2) + (line*3+2) * SCREEN_LARGE_WIDTH] = comp;					
				}
			}
			else if (scanline_handheld == 1)
			{
				unsigned long blend_red[5], blend_green[5], blend_blue[5];

				unsigned short pos_x = 8;
				unsigned short pos_y = 12 + scanline_count;
				
				for (unsigned int x = 0; x < LCD_WIDTH; x+=2)
				{
					comp_red[0][0] = ((gb->cgb.fixPalette[scanline_pixels1[x]] & 0x7C00) << 1);
					comp_green[0][0] = ((gb->cgb.fixPalette[scanline_pixels1[x]] & 0x03E0) << 1);
					comp_blue[0][0] = ((gb->cgb.fixPalette[scanline_pixels1[x]] & 0x001F));
					
					comp_red[1][0] = ((gb->cgb.fixPalette[scanline_pixels1[x+1]] & 0x7C00) << 1);
					comp_green[1][0] = ((gb->cgb.fixPalette[scanline_pixels1[x+1]] & 0x03E0) << 1);
					comp_blue[1][0] = ((gb->cgb.fixPalette[scanline_pixels1[x+1]] & 0x001F));
					
					comp_red[0][1] = ((gb->cgb.fixPalette[scanline_pixels2[x]] & 0x7C00) << 1);
					comp_green[0][1] = ((gb->cgb.fixPalette[scanline_pixels2[x]] & 0x03E0) << 1);
					comp_blue[0][1] = ((gb->cgb.fixPalette[scanline_pixels2[x]] & 0x001F));
					
					comp_red[1][1] = ((gb->cgb.fixPalette[scanline_pixels2[x+1]] & 0x7C00) << 1);
					comp_green[1][1] = ((gb->cgb.fixPalette[scanline_pixels2[x+1]] & 0x03E0) << 1);
					comp_blue[1][1] = ((gb->cgb.fixPalette[scanline_pixels2[x+1]] & 0x001F));

					blend_red[0] = (((comp_red[0][0] + comp_red[1][0]) >> 1) & 0xF800);
					blend_green[0] = (((comp_green[0][0] + comp_green[1][0]) >> 1) & 0x07E0);
					blend_blue[0] = (((comp_blue[0][0] + comp_blue[1][0]) >> 1) & 0x001F);
					
					blend_red[1] = (((comp_red[0][1] + comp_red[1][1]) >> 1) & 0xF800);
					blend_green[1] = (((comp_green[0][1] + comp_green[1][1]) >> 1) & 0x07E0);
					blend_blue[1] = (((comp_blue[0][1] + comp_blue[1][1]) >> 1) & 0x001F);
					
					blend_red[2] = (((comp_red[0][0] + comp_red[0][1]) >> 1) & 0xF800);
					blend_green[2] = (((comp_green[0][0] + comp_green[0][1]) >> 1) & 0x07E0);
					blend_blue[2] = (((comp_blue[0][0] + comp_blue[0][1]) >> 1) & 0x001F);
					
					blend_red[3] = (((comp_red[1][0] + comp_red[1][1]) >> 1) & 0xF800);
					blend_green[3] = (((comp_green[1][0] + comp_green[1][1]) >> 1) & 0x07E0);
					blend_blue[3] = (((comp_blue[1][0] + comp_blue[1][1]) >> 1) & 0x001F);
					
					blend_red[4] = (((blend_red[0] + blend_red[1] + blend_red[2] + blend_red[3]) >> 2) & 0xF800);
					blend_green[4] = (((blend_green[0] + blend_green[1] + blend_green[2] + blend_green[3]) >> 2) & 0x07E0);
					blend_blue[4] = (((blend_blue[0] + blend_blue[1] + blend_blue[2] + blend_blue[3]) >> 2) & 0x001F);
					
					screen_buffer[(pos_x) + (pos_y) * SCREEN_WIDTH] = (unsigned short)(comp_red[0][0] | comp_green[0][0] | comp_blue[0][0]);
					screen_buffer[(pos_x) + (pos_y+1) * SCREEN_WIDTH] = (unsigned short)(blend_red[2] | blend_green[2] | blend_blue[2]);
					screen_buffer[(pos_x) + (pos_y+2) * SCREEN_WIDTH] = (unsigned short)(comp_red[0][1] | comp_green[0][1] | comp_blue[0][1]);

					pos_x++;

					screen_buffer[(pos_x) + (pos_y) * SCREEN_WIDTH] = (unsigned short)(blend_red[0] | blend_green[0] | blend_blue[0]);
					screen_buffer[(pos_x) + (pos_y+1) * SCREEN_WIDTH] = (unsigned short)(blend_red[4] | blend_green[4] | blend_blue[4]);
					screen_buffer[(pos_x) + (pos_y+2) * SCREEN_WIDTH] = (unsigned short)(blend_red[1] | blend_green[1] | blend_blue[1]);

					pos_x++;

					screen_buffer[(pos_x) + (pos_y) * SCREEN_WIDTH] = (unsigned short)(comp_red[1][0] | comp_green[1][0] | comp_blue[1][0]);
					screen_buffer[(pos_x) + (pos_y+1) * SCREEN_WIDTH] = (unsigned short)(blend_red[3] | blend_green[3] | blend_blue[3]);
					screen_buffer[(pos_x) + (pos_y+2) * SCREEN_WIDTH] = (unsigned short)(comp_red[1][1] | comp_green[1][1] | comp_blue[1][1]);

					pos_x++;
				}
			}
		}
	}
	else // DMG
	{
		unsigned short comp = 0;

		if (scanline_scaled == 0)
		{
			if (scanline_handheld == 1)
			{
				for(unsigned int x = 0; x < LCD_WIDTH; x++)
				{
					comp = selected_palette_lcd[(scanline_pixels1[(x)] & LCD_PALETTE_ALL) >> 4][((scanline_pixels1[(x)] & 3)<<1)];

					screen_large_buffer[(x*2) + (line*2) * SCREEN_LARGE_WIDTH] = comp;
					screen_large_buffer[(x*2+1) + (line*2) * SCREEN_LARGE_WIDTH] = comp;

					screen_large_buffer[(x*2) + (line*2+1) * SCREEN_LARGE_WIDTH] = comp;
					screen_large_buffer[(x*2+1) + (line*2+1) * SCREEN_LARGE_WIDTH] = comp;
				}
			}
			else if (scanline_handheld == 1)
			{
				for(unsigned int x = 0; x < LCD_WIDTH; x++)
				{
					screen_buffer[(x) + (line) * SCREEN_WIDTH] = selected_palette_lcd[(scanline_pixels1[(x)] & LCD_PALETTE_ALL) >> 4][((scanline_pixels1[(x)] & 3)<<1)];
				}
			}
		}
		else
		{
			if (scanline_handheld == 0)
			{
				for(unsigned int x = 0; x < LCD_WIDTH; x++)
				{
					comp = selected_palette_lcd[(scanline_pixels1[(x)] & LCD_PALETTE_ALL) >> 4][((scanline_pixels1[(x)] & 3)<<1)];

					screen_large_buffer[(x*3) + (line*3) * SCREEN_LARGE_WIDTH] = comp;
					screen_large_buffer[(x*3+1) + (line*3) * SCREEN_LARGE_WIDTH] = comp;
					screen_large_buffer[(x*3+2) + (line*3) * SCREEN_LARGE_WIDTH] = comp;

					screen_large_buffer[(x*3) + (line*3+1) * SCREEN_LARGE_WIDTH] = comp;
					screen_large_buffer[(x*3+1) + (line*3+1) * SCREEN_LARGE_WIDTH] = comp;
					screen_large_buffer[(x*3+2) + (line*3+1) * SCREEN_LARGE_WIDTH] = comp;

					screen_large_buffer[(x*3) + (line*3+2) * SCREEN_LARGE_WIDTH] = comp;
					screen_large_buffer[(x*3+1) + (line*3+2) * SCREEN_LARGE_WIDTH] = comp;
					screen_large_buffer[(x*3+2) + (line*3+2) * SCREEN_LARGE_WIDTH] = comp;					
				}
			}
			else if (scanline_handheld == 1)
			{
				if (palette_num < 4) // greyscale
				{
					unsigned char grid[3][3];

					unsigned short pos_x = 8;
					unsigned short pos_y = 12 + scanline_count;

					unsigned char pal[2][2];

					for (unsigned int x = 0; x < LCD_WIDTH; x+=2)
					{
						grid[0][0] = ((scanline_pixels1[x] & 3) << 1);
						grid[2][0] = ((scanline_pixels1[x+1] & 3) << 1);
						grid[1][0] = ((grid[0][0] + grid[2][0]) >> 1);

						grid[0][2] = ((scanline_pixels2[x] & 3) << 1);
						grid[2][2] = ((scanline_pixels2[x+1] & 3) << 1);
						grid[1][2] = ((grid[0][2] + grid[2][2]) >> 1);

						grid[0][1] = ((grid[0][0] + grid[0][2]) >> 1);
						grid[1][1] = ((grid[1][0] + grid[1][2]) >> 1);
						grid[2][1] = ((grid[2][0] + grid[2][2]) >> 1);

						pal[0][0] = ((scanline_pixels1[x]&LCD_PALETTE_ALL) >> 4);
						pal[1][0] = ((scanline_pixels1[x+1]&LCD_PALETTE_ALL) >> 4);
						pal[0][1] = ((scanline_pixels2[x]&LCD_PALETTE_ALL) >> 4);
						pal[1][1] = ((scanline_pixels2[x+1]&LCD_PALETTE_ALL) >> 4);

						screen_buffer[(pos_x) + (pos_y) * SCREEN_WIDTH] = selected_palette_lcd[pal[0][0]][grid[0][0]];
						screen_buffer[(pos_x) + (pos_y+1) * SCREEN_WIDTH] = selected_palette_lcd[pal[0][0]][grid[0][1]];
						screen_buffer[(pos_x) + (pos_y+2) * SCREEN_WIDTH] = selected_palette_lcd[pal[1][0]][grid[0][2]];

						pos_x++;

						screen_buffer[(pos_x) + (pos_y) * SCREEN_WIDTH] = selected_palette_lcd[pal[0][0]][grid[1][0]];
						screen_buffer[(pos_x) + (pos_y+1) * SCREEN_WIDTH] = selected_palette_lcd[pal[0][0]][grid[1][1]];
						screen_buffer[(pos_x) + (pos_y+2) * SCREEN_WIDTH] = selected_palette_lcd[pal[1][0]][grid[1][2]];

						pos_x++;

						screen_buffer[(pos_x) + (pos_y) * SCREEN_WIDTH] = selected_palette_lcd[pal[0][1]][grid[2][0]];
						screen_buffer[(pos_x) + (pos_y+1) * SCREEN_WIDTH] = selected_palette_lcd[pal[0][1]][grid[2][1]];
						screen_buffer[(pos_x) + (pos_y+2) * SCREEN_WIDTH] = selected_palette_lcd[pal[1][1]][grid[2][2]];

						pos_x++;
					}
				}
				else // GBC palettes for DMG
				{
					unsigned long orig_pix[2][2];
					
					unsigned long comp_red[2][2], comp_green[2][2], comp_blue[2][2];
					
					unsigned long blend_red[5], blend_green[5], blend_blue[5];

					unsigned short pos_x = 8;
					unsigned short pos_y = 12 + scanline_count;

					for (unsigned int x = 0; x < LCD_WIDTH; x+=2)
					{
						orig_pix[0][0] = selected_palette_lcd[(scanline_pixels1[(x)] & LCD_PALETTE_ALL) >> 4][((scanline_pixels1[(x)] & 3)<<1)];
						orig_pix[1][0] = selected_palette_lcd[(scanline_pixels1[(x+1)] & LCD_PALETTE_ALL) >> 4][((scanline_pixels1[(x+1)] & 3)<<1)];
						orig_pix[0][1] = selected_palette_lcd[(scanline_pixels1[(x)] & LCD_PALETTE_ALL) >> 4][((scanline_pixels2[(x)] & 3)<<1)];
						orig_pix[1][1] = selected_palette_lcd[(scanline_pixels1[(x+1)] & LCD_PALETTE_ALL) >> 4][((scanline_pixels2[(x+1)] & 3)<<1)];
						
						comp_red[0][0] = (orig_pix[0][0] & 0x001C);
						comp_green[0][0] = (orig_pix[0][0] & 0x0700);
						comp_blue[0][0] = (orig_pix[0][0] & 0xC000);

						comp_red[1][0] = (orig_pix[1][0] & 0x001C);
						comp_green[1][0] = (orig_pix[1][0] & 0x0700);
						comp_blue[1][0] = (orig_pix[1][0] & 0xC000);
						
						comp_red[0][1] = (orig_pix[0][1] & 0x001C);
						comp_green[0][1] = (orig_pix[0][1] & 0x0700);
						comp_blue[0][1] = (orig_pix[0][1] & 0xC000);
						
						comp_red[1][1] = (orig_pix[1][1] & 0x001C);
						comp_green[1][1] = (orig_pix[1][1] & 0x0700);
						comp_blue[1][1] = (orig_pix[1][1] & 0xC000);

						blend_red[0] = (((comp_red[0][0] + comp_red[1][0]) >> 1) & 0x001C);
						blend_green[0] = (((comp_green[0][0] + comp_green[1][0]) >> 1) & 0x0700);
						blend_blue[0] = (((comp_blue[0][0] + comp_blue[1][0]) >> 1) & 0xC000);

						blend_red[1] = (((comp_red[0][1] + comp_red[1][1]) >> 1) & 0x001C);
						blend_green[1] = (((comp_green[0][1] + comp_green[1][1]) >> 1) & 0x0700);
						blend_blue[1] = (((comp_blue[0][1] + comp_blue[1][1]) >> 1) & 0xC000);

						blend_red[2] = (((comp_red[0][0] + comp_red[0][1]) >> 1) & 0x001C);
						blend_green[2] = (((comp_green[0][0] + comp_green[0][1]) >> 1) & 0x0700);
						blend_blue[2] = (((comp_blue[0][0] + comp_blue[0][1]) >> 1) & 0xC000);

						blend_red[3] = (((comp_red[1][0] + comp_red[1][1]) >> 1) & 0x001C);
						blend_green[3] = (((comp_green[1][0] + comp_green[1][1]) >> 1) & 0x0700);
						blend_blue[3] = (((comp_blue[1][0] + comp_blue[1][1]) >> 1) & 0xC000);

						blend_red[4] = (((blend_red[0] + blend_red[1] + blend_red[2] + blend_red[3]) >> 2) & 0x001C);
						blend_green[4] = (((blend_green[0] + blend_green[1] + blend_green[2] + blend_green[3]) >> 2) & 0x0700);
						blend_blue[4] = (((blend_blue[0] + blend_blue[1] + blend_blue[2] + blend_blue[3]) >> 2) & 0xC000);

						screen_buffer[(pos_x) + (pos_y) * SCREEN_WIDTH] = (unsigned short)(comp_red[0][0] | comp_green[0][0] | comp_blue[0][0]);
						screen_buffer[(pos_x) + (pos_y+1) * SCREEN_WIDTH] = (unsigned short)(blend_red[2] | blend_green[2] | blend_blue[2]);
						screen_buffer[(pos_x) + (pos_y+2) * SCREEN_WIDTH] = (unsigned short)(comp_red[0][1] | comp_green[0][1] | comp_blue[0][1]);

						pos_x++;

						screen_buffer[(pos_x) + (pos_y) * SCREEN_WIDTH] = (unsigned short)(blend_red[0] | blend_green[0] | blend_blue[0]);
						screen_buffer[(pos_x) + (pos_y+1) * SCREEN_WIDTH] = (unsigned short)(blend_red[4] | blend_green[4] | blend_blue[4]);
						screen_buffer[(pos_x) + (pos_y+2) * SCREEN_WIDTH] = (unsigned short)(blend_red[1] | blend_green[1] | blend_blue[1]);

						pos_x++;

						screen_buffer[(pos_x) + (pos_y) * SCREEN_WIDTH] = (unsigned short)(comp_red[1][0] | comp_green[1][0] | comp_blue[1][0]);
						screen_buffer[(pos_x) + (pos_y+1) * SCREEN_WIDTH] = (unsigned short)(blend_red[3] | blend_green[3] | blend_blue[3]);
						screen_buffer[(pos_x) + (pos_y+2) * SCREEN_WIDTH] = (unsigned short)(comp_red[1][1] | comp_green[1][1] | comp_blue[1][1]);

						pos_x++;
					}
				}
			}
		}
	}
}
#endif

// DMG: core = 0
// GBC: core = 1
int PeanutGB(unsigned char core, const char *keyboard_name, const char *joystick_name)
{	
	int buttons_file = 0;

	char buttons_buffer[13] = { '0', '0', '0', '0', '0', '0', '0', '0', '0', 
		'0', '0', '0', '0' };

	for (unsigned int i=0; i<SCREEN_WIDTH*SCREEN_HEIGHT; i++)
	{
		screen_buffer[i] = 0x0000;
	}

	for (unsigned int i=0; i<SCREEN_LARGE_WIDTH*SCREEN_LARGE_HEIGHT; i++)
	{
		screen_large_buffer[i] = 0x0000;
	}

	int screen_file = 0;

	int sound_file = 0;
	int sound_fragment = 0x0004000A; // 4 blocks, each is 2^10 = 1024
	int sound_stereo = 0;
	int sound_format = AFMT_S8; // AFMT_U8;
	int sound_speed = AUDIO_SAMPLE_RATE; // calculations: 61162 = 1024 * (4194304 / 70224) + 1

	sound_file = open("/dev/dsp", O_WRONLY);

	ioctl(sound_file, SNDCTL_DSP_SETFRAGMENT, &sound_fragment); // needed to stop the drift
	ioctl(sound_file, SNDCTL_DSP_STEREO, &sound_stereo);
	ioctl(sound_file, SNDCTL_DSP_SETFMT, &sound_format);
	ioctl(sound_file, SNDCTL_DSP_SPEED, &sound_speed);

	struct gb_s gb;
	struct priv_t priv;

	enum gb_init_error_e gb_ret;
	int ret = EXIT_SUCCESS;
	
	/* TODO: Sanity check input GB file. */

	/* Initialise emulator context. */
	gb_ret = gb_init(&gb, &gb_rom_read, &gb_cart_ram_read, &gb_cart_ram_write,
			 &gb_error, &priv);
	
	if (core == 0) gb.cgb.cgbMode = 0; // run as DMG Gameboy

	switch(gb_ret)
	{
	case GB_INIT_NO_ERROR:
		break;

	case GB_INIT_CARTRIDGE_UNSUPPORTED:
		printf("Unsupported Cart\n");
		ret = EXIT_FAILURE;
		while (1) { }

	case GB_INIT_INVALID_CHECKSUM:
		printf("Invalid Checksum\n");
		ret = EXIT_FAILURE;
		while (1) { }

	default:
		printf("Unknown Error %04X\n", gb_ret);
		ret = EXIT_FAILURE;
		while (1) { }
	}

/*
	result = f_open(&file, "/DMG-BOOT.BIN", FA_READ);
	if (result == 0)
	{	
		for (unsigned int i=0; i<256; i++)
		{
			while (f_read(&file, &buffer, 1, &bytes) != 0) { }

			boot_rom[i] = buffer[0];
		}
		
		while (f_sync(&file) != 0) { }
		while (f_close(&file) != 0) { }

		gb_set_boot_rom(&gb, gb_boot_rom_read);
		gb_reset(&gb);
	}
	else
	{
		SendString("Could not find DMG-BOOT.BIN file\n\r\\");
	}
*/
	// instead of using DMG-BOOT above, just do this instead	
	gb_set_boot_rom(&gb, gb_boot_rom_read);
	gb_reset(&gb);

	/* Set the RTC of the game cartridge. Only used by games that support it. */
	{
		time_t rawtime;
		time(&rawtime);
#ifdef _POSIX_C_SOURCE
		struct tm timeinfo;
		localtime_r(&rawtime, &timeinfo);
#else
		struct tm *timeinfo;
		timeinfo = localtime(&rawtime);
#endif

		/* You could potentially force the game to allow the player to
		 * reset the time by setting the RTC to invalid values.
		 *
		 * Using memset(&gb->cart_rtc, 0xFF, sizeof(gb->cart_rtc)) for
		 * example causes Pokemon Gold/Silver to say "TIME NOT SET",
		 * allowing the player to set the time without having some dumb
		 * password.
		 *
		 * The memset has to be done directly to gb->cart_rtc because
		 * gb_set_rtc() processes the input values, which may cause
		 * games to not detect invalid values.
		 */

		/* Set RTC. Only games that specify support for RTC will use
		 * these values. */
#ifdef _POSIX_C_SOURCE
		gb_set_rtc(&gb, &timeinfo);
#else
		gb_set_rtc(&gb, timeinfo);
#endif
	}

#if ENABLE_LCD
	gb_init_lcd(&gb, &lcd_draw_line);
#endif

	auto_assign_palette(gb_colour_hash(&gb), palette_num); // default
	
	unsigned long previous_clock = 0;

	unsigned char infinite_loop = 1;

	unsigned char left_bumper = 0;

	while (infinite_loop > 0)
	{
		gb.direct.joypad |= JOYPAD_UP;
		gb.direct.joypad |= JOYPAD_DOWN;
		gb.direct.joypad |= JOYPAD_LEFT;
		gb.direct.joypad |= JOYPAD_RIGHT;
		gb.direct.joypad |= JOYPAD_SELECT;
		gb.direct.joypad |= JOYPAD_START;
		gb.direct.joypad |= JOYPAD_A;
		gb.direct.joypad |= JOYPAD_B;

		speed_limiter = 1; // normal speed by default

		left_bumper = 0;

		turbo_a = 0;
		turbo_b = 0;

		buttons_file = open(keyboard_name, O_RDONLY);
		read(buttons_file, &buttons_buffer, 13);
		close(buttons_file);

		// get key inputs
		if (buttons_buffer[0] != '0') infinite_loop = 0; // exit
		if (buttons_buffer[1] != '0') gb.direct.joypad &= ~JOYPAD_UP;
		if (buttons_buffer[2] != '0') gb.direct.joypad &= ~JOYPAD_DOWN;
		if (buttons_buffer[3] != '0') gb.direct.joypad &= ~JOYPAD_LEFT;
		if (buttons_buffer[4] != '0') gb.direct.joypad &= ~JOYPAD_RIGHT;
		if (buttons_buffer[5] != '0') gb.direct.joypad &= ~JOYPAD_SELECT;
		if (buttons_buffer[6] != '0') gb.direct.joypad &= ~JOYPAD_START;
		if (buttons_buffer[7] != '0') gb.direct.joypad &= ~JOYPAD_A;
		if (buttons_buffer[8] != '0') gb.direct.joypad &= ~JOYPAD_B;

		if (buttons_buffer[9] != '0') turbo_a = 1;
		if (buttons_buffer[10] != '0') turbo_b = 1;

		if (buttons_buffer[11] != '0')
		{
			left_bumper = 1;

			if (buttons_buffer[7] != '0')
			{
				gb_write_cart_ram_file("cart_ram.sav");
			}

			if (buttons_buffer[8] != '0')
			{
				gb_read_cart_ram_file("cart_ram.sav");
				gb_reset(&gb);
			}
		}

		if (buttons_buffer[12] != '0')
		{
			speed_limiter = 0;
		}

		buttons_file = open(joystick_name, O_RDONLY);
		read(buttons_file, &buttons_buffer, 13);
		close(buttons_file);

		// get joy inputs
		if (buttons_buffer[0] != '0') infinite_loop = 0; // exit
		if (buttons_buffer[1] != '0') gb.direct.joypad &= ~JOYPAD_UP;
		if (buttons_buffer[2] != '0') gb.direct.joypad &= ~JOYPAD_DOWN;
		if (buttons_buffer[3] != '0') gb.direct.joypad &= ~JOYPAD_LEFT;
		if (buttons_buffer[4] != '0') gb.direct.joypad &= ~JOYPAD_RIGHT;
		if (buttons_buffer[5] != '0') gb.direct.joypad &= ~JOYPAD_SELECT;
		if (buttons_buffer[6] != '0') gb.direct.joypad &= ~JOYPAD_START;
		if (buttons_buffer[7] != '0') gb.direct.joypad &= ~JOYPAD_A;
		if (buttons_buffer[8] != '0') gb.direct.joypad &= ~JOYPAD_B;

		if (buttons_buffer[9] != '0') turbo_a = 1;
		if (buttons_buffer[10] != '0') turbo_b = 1;

		if (buttons_buffer[11] != '0')
		{
			left_bumper = 1;

			if (buttons_buffer[7] != '0')
			{
				gb_write_cart_ram_file("cart_ram.sav");
			}

			if (buttons_buffer[8] != '0')
			{
				gb_read_cart_ram_file("cart_ram.sav");
				gb_reset(&gb);
			}
		}

		if (buttons_buffer[12] != '0')
		{
			speed_limiter = 0;
		}

		turbo_counter++;

		if (turbo_counter >= 6)
		{
			turbo_counter = 0;
			turbo_state = 1 - turbo_state;
		}

		if (turbo_a > 0)
		{
			if (turbo_state > 0)
			{
				gb.direct.joypad &= ~JOYPAD_A;
			}
			else
			{
				gb.direct.joypad |= JOYPAD_A;
			}
		}

		if (turbo_b > 0)
		{
			if (turbo_state > 0)
			{
				gb.direct.joypad &= ~JOYPAD_B;
			}
			else
			{
				gb.direct.joypad |= JOYPAD_B;
			}
		}

		if (left_bumper > 0)
		{
			gb.direct.joypad |= JOYPAD_UP;
			gb.direct.joypad |= JOYPAD_DOWN;
			gb.direct.joypad |= JOYPAD_LEFT;
			gb.direct.joypad |= JOYPAD_RIGHT;
			gb.direct.joypad |= JOYPAD_SELECT;
			gb.direct.joypad |= JOYPAD_START;
			gb.direct.joypad |= JOYPAD_A;
			gb.direct.joypad |= JOYPAD_B;
		}

		/* Execute CPU cycles until the screen has to be redrawn. */
		gb_run_frame(&gb);

		if (scanline_handheld == 0)
		{
			screen_file = open("/dev/fb0", O_RDWR);
			write(screen_file, &screen_large_buffer, SCREEN_LARGE_WIDTH*SCREEN_LARGE_HEIGHT*2);
			close(screen_file);
		}
		else if (scanline_handheld == 1)
		{
			screen_file = open("/dev/fb0", O_RDWR);
			write(screen_file, &screen_buffer, SCREEN_WIDTH*SCREEN_HEIGHT*2);
			close(screen_file);
		}

		// delay
		if (speed_limiter > 0)
		{
			while (clock() < previous_clock + 16667) { }
			previous_clock = clock();

#ifdef ENABLE_SOUND
			if (audio_enable > 0)
			{
				audio_callback(&gb, (uint8_t *)&audio_buffer);
				write(sound_file, &audio_buffer, 1024); // AUDIO_SAMPLES
			}
#endif
		}
	}

	close(sound_file);

	return ret;
}


				
