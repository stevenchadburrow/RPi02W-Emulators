
// PICnes.c

// An NES emulator designed for the PIC32MZ
// Yet easy to implement on other platforms
// Written by: Professor Steven Chad Burrow


// if using for other platforms, adjust variable types here
// 'volatile' seems to keep it from getting general exception errors
// but it also slows down the whole system!


#include <stdio.h>
#include <sys/ioctl.h>
#include <linux/kd.h>
#include <termios.h>

#include <stdint.h>
#include <stdlib.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <time.h>


unsigned char nes_running = 1; // when 0, exits game

unsigned char cart_rom[0x1000000]; // 1 MB max?
unsigned char sys_ram[8192]; // used for NES
unsigned char cart_ram[32768]; // used for NES
unsigned char ext_ram[32768]; // used for NES


// video
unsigned short screen_large_buffer[(640+32)*480]; // add some extra at the bottom for drawing in v-sync area, etc.
unsigned short screen_small_buffer[(320+16)*240]; 
unsigned char screen_handheld = 0; // 0 = HDMI, 1 = LCD
int screen_file = 0;

int buttons_file = 0;
char buttons_buffer[13] = { '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0' };

#define AUDIO_LEN 2048

// audio
unsigned char audio_buffer[AUDIO_LEN];
unsigned char audio_array[1024];
int audio_file = 0;
unsigned int audio_read = 0;
unsigned int audio_write = 0;
unsigned int audio_enable = 0;


unsigned long prg_offset = 0x00000000; // offsets in cart_rom
unsigned long chr_offset = 0x00000000;
unsigned long end_offset = 0x00000000;

volatile unsigned char pal_ram[32]; // special palette ram inside of ppu
volatile unsigned char oam_ram[256]; // special sprite ram inside of ppu 

volatile unsigned char cart_header[16]; // copy of ROM header for quicker access

volatile unsigned char *cpu_ram; // only cpu ram from 0x0000 to 0x07FF
volatile unsigned char *ppu_ram; // ppu ram from 0x2000 to 0x2FFF (halved, mirrored)
volatile unsigned char *prg_ram; // cpu ram from 0x6000 to 0x7FFF (if used)
volatile unsigned char *chr_ram; // ppu ram from 0x0000 to 0x1FFF (if used)

volatile unsigned char *cart_bottom; // copy of ROM bottom bank for quicker access

unsigned long nes_hack_vsync_flag = 0; // change this accordingly
unsigned long nes_hack_sprite_priority = 0; // change this accordingly

unsigned long nes_border_shrink = 0; // 0 through 3 really

unsigned long nes_loop_option = 0; // 0 through 4 really
unsigned long nes_loop_max = 0;
unsigned long nes_loop_detect = 0;
unsigned long nes_loop_count = 0;
unsigned long nes_loop_cycles = 0;
unsigned long nes_loop_read = 0;
unsigned long nes_loop_write = 0;
unsigned long nes_loop_branch = 0;
unsigned long nes_loop_reg_a = 0;
unsigned long nes_loop_reg_x = 0;
unsigned long nes_loop_reg_y = 0;
unsigned long nes_loop_halt = 0;

unsigned long nes_timer_flag = 0;
unsigned long nes_init_flag = 0;
unsigned long nes_reset_flag = 0;
unsigned long nes_audio_flag = 1;
unsigned long nes_pixel_location = 0;

unsigned long cpu_current_cycles = 0;
unsigned long cpu_dma_cycles = 0;

unsigned long ppu_frame_cycles = 0;
unsigned long ppu_scanline_cycles = 0;
unsigned long ppu_tile_cycles = 0;

unsigned long ppu_frame_count = 0;
signed long ppu_scanline_count = 0; // needs to be signed
unsigned long ppu_tile_count = 0;

unsigned long apu_sample_cycles = 0;
unsigned long apu_sample_cycles_ext = 0;

unsigned long map_number = 0x0000;

unsigned long map_unrom_bank = 0x0000;
unsigned long map_cnrom_bank = 0x0000;
unsigned long map_anrom_bank = 0x0000;

unsigned long map_mmc1_ready = 0x0000;
unsigned long map_mmc1_shift = 0x0000;
unsigned long map_mmc1_count = 0x0000;
unsigned long map_mmc1_prg_mode = 0x0003; // should be 0x0003
unsigned long map_mmc1_chr_mode = 0x0000;
unsigned long map_mmc1_chr_bank_0 = 0x0010; // leave at 0x0010 for large PRG_ROM
unsigned long map_mmc1_chr_bank_1 = 0x0000;
unsigned long map_mmc1_prg_bank = 0x0000;
unsigned long map_mmc1_ram = 0x0000;

unsigned long map_mmc3_bank_next = 0x0000;
unsigned long map_mmc3_prg_mode = 0x0000;
unsigned long map_mmc3_chr_mode = 0x0000;
unsigned long map_mmc3_bank_r0 = 0x0000;
unsigned long map_mmc3_bank_r1 = 0x0000;
unsigned long map_mmc3_bank_r2 = 0x0000;
unsigned long map_mmc3_bank_r3 = 0x0000;
unsigned long map_mmc3_bank_r4 = 0x0000;
unsigned long map_mmc3_bank_r5 = 0x0000;
unsigned long map_mmc3_bank_r6 = 0x0000;
unsigned long map_mmc3_bank_r7 = 0x0000;
unsigned long map_mmc3_ram = 0x0000;
unsigned long map_mmc3_irq_latch = 0x007F;
unsigned long map_mmc3_irq_counter = 0x007F; // start at something above 0??
unsigned long map_mmc3_irq_enable = 0x0000;
unsigned long map_mmc3_irq_previous = 0x0000;
unsigned long map_mmc3_irq_interrupt = 0x0000;
unsigned long map_mmc3_irq_reload = 0x0000;
unsigned long map_mmc3_irq_a12 = 0x0000;
unsigned long map_mmc3_irq_delay = 0x0010; // 0x0010 or 0x0028 // play with these values
unsigned long map_mmc3_irq_shift = 0x0001; // 0x0001 or 0x0003 // play with these values

unsigned long cpu_reg_a = 0x0000, cpu_reg_x = 0x0000, cpu_reg_y = 0x0000, cpu_reg_s = 0x00FD;
unsigned long cpu_flag_c = 0x0000, cpu_flag_z = 0x0000, cpu_flag_v = 0x0000, cpu_flag_n = 0x0000;
unsigned long cpu_flag_d = 0x0000, cpu_flag_b = 0x0000, cpu_flag_i = 0x0001; // needs to be 0x0001
unsigned long cpu_reg_pc = 0xFFFC;

unsigned long cpu_temp_opcode = 0x0000, cpu_temp_memory = 0x0000, cpu_temp_address = 0x0000; 
unsigned long cpu_temp_result = 0x0000, cpu_temp_cycles = 0x0000;

unsigned long cpu_status_r = 0x0000;

unsigned long ppu_reg_v = 0x0000, ppu_reg_t = 0x0000, ppu_reg_w = 0x0000;
unsigned long ppu_reg_a = 0x0000, ppu_reg_b = 0x0000;
unsigned long ppu_reg_x = 0x0000;

unsigned long ppu_flag_e = 0x0000, ppu_flag_p = 0x0000, ppu_flag_h = 0x0000;
unsigned long ppu_flag_b = 0x0000, ppu_flag_s = 0x0000, ppu_flag_i = 0x0000;
unsigned long ppu_flag_v = 0x0000, ppu_flag_0 = 0x0000, ppu_flag_o = 0x0000;

unsigned long ppu_flag_g = 0x0000, ppu_flag_lb = 0x0000, ppu_flag_ls = 0x0000;
unsigned long ppu_flag_eb = 0x0000, ppu_flag_es = 0x0000;

unsigned long ppu_status_0 = 0x0000;
unsigned long ppu_status_v = 0x0000;
signed long ppu_status_s = 0x0000; // needs to be signed
signed long ppu_status_d = 0x0000;
unsigned long ppu_status_m = 0x0000;

unsigned long ctl_flag_s = 0x0000;
unsigned long ctl_value_1 = 0x0000, ctl_value_2 = 0x0000;
unsigned long ctl_latch_1 = 0x0000, ctl_latch_2 = 0x0000;

unsigned short apu_pulse_1_d = 0x0000, apu_pulse_2_d = 0x0000;
unsigned short apu_pulse_1_u = 0x0000, apu_pulse_2_u = 0x0000;
unsigned short apu_pulse_1_i = 0x0000, apu_pulse_2_i = 0x0000;
unsigned short apu_pulse_1_c = 0x0000, apu_pulse_2_c = 0x0000;
unsigned short apu_pulse_1_v = 0x0000, apu_pulse_2_v = 0x0000;
unsigned short apu_pulse_1_m = 0x0000, apu_pulse_2_m = 0x0000;
unsigned short apu_pulse_1_r = 0x0000, apu_pulse_2_r = 0x0000;
unsigned short apu_pulse_1_s = 0x0000, apu_pulse_2_s = 0x0000;
unsigned short apu_pulse_1_a = 0x0000, apu_pulse_2_a = 0x0000;
unsigned short apu_pulse_1_n = 0x0000, apu_pulse_2_n = 0x0000;
unsigned short apu_pulse_1_p = 0x0000, apu_pulse_2_p = 0x0000;
unsigned short apu_pulse_1_w = 0x0000, apu_pulse_2_w = 0x0000;
unsigned short apu_pulse_1_e = 0x0000, apu_pulse_2_e = 0x0000;
unsigned short apu_pulse_1_t = 0x0000, apu_pulse_2_t = 0x0000;
unsigned short apu_pulse_1_k = 0x0000, apu_pulse_2_k = 0x0000;
unsigned short apu_pulse_1_l = 0x0000, apu_pulse_2_l = 0x0000;
unsigned short apu_pulse_1_o = 0x0000, apu_pulse_2_o = 0x0000;

unsigned short apu_triangle_c = 0x0000;
unsigned short apu_triangle_r = 0x0000;
unsigned short apu_triangle_v = 0x0000;
unsigned short apu_triangle_f = 0x0000;
unsigned short apu_triangle_h = 0x0000;
unsigned short apu_triangle_q = 0x0000;
unsigned short apu_triangle_t = 0x0000;
unsigned short apu_triangle_k = 0x0000;
unsigned short apu_triangle_l = 0x0000;
unsigned short apu_triangle_p = 0x0000;
unsigned short apu_triangle_d = 0x0000;
unsigned short apu_triangle_o = 0x0000;

unsigned short apu_noise_i = 0x0000;
unsigned short apu_noise_c = 0x0000;
unsigned short apu_noise_v = 0x0000;
unsigned short apu_noise_m = 0x0000;
unsigned short apu_noise_r = 0x0000;
unsigned short apu_noise_s = 0x0001; // must be set to 0x0001
unsigned short apu_noise_x = 0x0000;
unsigned short apu_noise_d = 0x0000;
unsigned short apu_noise_t = 0x0000;
unsigned short apu_noise_k = 0x0000;
unsigned short apu_noise_l = 0x0000;
unsigned short apu_noise_o = 0x0000;

unsigned short apu_dmc_i = 0x0000;
unsigned short apu_dmc_l = 0x0000;
unsigned short apu_dmc_r = 0x0000;
unsigned short apu_dmc_k = 0x0000;
unsigned short apu_dmc_d = 0x0000;
unsigned short apu_dmc_a = 0x0000;
unsigned short apu_dmc_s = 0x0000;
unsigned short apu_dmc_b = 0x0000;
unsigned short apu_dmc_t = 0x0000;
unsigned short apu_dmc_o = 0x0000;

unsigned short apu_flag_i = 0x0000;
unsigned short apu_flag_f = 0x0000;
unsigned short apu_flag_d = 0x0000;
unsigned short apu_flag_n = 0x0000;
unsigned short apu_flag_t = 0x0000;
unsigned short apu_flag_2 = 0x0000;
unsigned short apu_flag_1 = 0x0000;
unsigned short apu_flag_m = 0x0000;
unsigned short apu_flag_b = 0x0000; // was 0x0001

unsigned short apu_counter_q = 0x0000;
unsigned short apu_counter_s = 0x0000;
unsigned short apu_mixer_output = 0x0000;

volatile unsigned long nes_pixel_loc = 0;
volatile unsigned short nes_pixel_val = 0;


volatile unsigned short nes_palette[64] = {
/*
	// RGB
	0x430C,0xC000,0x8000,0x8108,0x8010,0x0014,0x0014,0x0010,
	0x0108,0x0300,0x0300,0x0200,0x4200,0x0000,0x0000,0x0000,
	0x8514,0xC300,0xC200,0xC20C,0xC018,0x401C,0x011C,0x021C,
	0x0314,0x0500,0x0500,0x4500,0x8400,0x0000,0x0000,0x0000,
	0xC71C,0xC504,0xC40C,0xC310,0xC31C,0x821C,0x431C,0x451C,
	0x051C,0x0714,0x4608,0x8708,0xC700,0x430C,0x0000,0x0000,
	0xC71C,0xC714,0xC514,0xC518,0xC51C,0xC51C,0x861C,0x871C,
	0x461C,0x4718,0x8714,0xC714,0xC700,0xC61C,0x0000,0x0000,
*/
	// BGR
	0x6308,0x0018,0x0010,0x4110,0x8010,0xA000,0xA000,0x8000,
	0x4100,0x0300,0x0300,0x0200,0x0208,0x0000,0x0000,0x0000,
	0xA510,0x0318,0x0218,0x6218,0xC018,0xE008,0xE100,0xE200,
	0xA300,0x0500,0x0500,0x0508,0x0410,0x0000,0x0000,0x0000,
	0xE718,0x2518,0x6418,0x8318,0xE318,0xE210,0xE308,0xE508,
	0xE500,0xA700,0x4608,0x4710,0x0718,0x6308,0x0000,0x0000,
	0xE718,0xA718,0xA518,0xC518,0xE518,0xE518,0xE610,0xE710,
	0xE608,0xC708,0xA710,0xA718,0x0718,0xE618,0x0000,0x0000,

};

volatile unsigned char apu_length[32] = {
	 10, 254,  20,   2,  40,   4,  80,   6, 160,   8,  60,  10,  14,  12,  26,  14,
	 12,  16,  24,  18,  48,  20,  96,  22, 192,  24,  72,  26,  16,  28,  32,  30
};

volatile unsigned char apu_duty[32] = {
	0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00,
	0xFF, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

volatile unsigned short apu_rate[16] = {
	428, 380, 340, 320, 286, 254, 226, 214, 190, 160, 142, 128, 106,  84,  72,  54
};

volatile unsigned short apu_period[16] = {
	4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068
};



// change for platform
void nes_error(unsigned char code)
{			
	printf("NES Error %02X\n", code);
}

/*
unsigned char nes_save(char *filename)
{
	// Global variables
	FIL file; // File handle for the file we open
	DIR dir; // Directory information for the current directory
	FATFS fso; // File System Object for the file system we are reading from
	
	//SendString("Initializing disk\n\r\\");
	
	// Wait for the disk to initialise
    while(disk_initialize(0));
    // Mount the disk
    f_mount(&fso, "", 0);
    // Change dir to the root directory
    f_chdir("/");
    // Open the directory
    f_opendir(&dir, ".");
 
	unsigned char buffer[1];
	unsigned int bytes;
	unsigned int result;
	unsigned char flag;
	
	result = f_open(&file, filename, FA_CREATE_ALWAYS | FA_WRITE);
	if (result == 0)
	{		
		for (unsigned int i=0; i<8192; i++)
		{
			buffer[0] = prg_ram[i];
			
			while (f_write(&file, buffer, 1, &bytes) != 0) { }
		}
		
		while (f_sync(&file) != 0) { }
		while (f_close(&file) != 0) { }
		
		//SendString("Wrote cart ram to file\n\r\\");
		
		flag = 1;
	}
	else
	{
		//SendString("Could not write cart ram to file\n\r\\");
		
		flag = 0;
		
		nes_error(0x00);
	}	
	
	return flag;
}

unsigned char nes_load(char *filename)
{
	// Global variables
	FIL file; // File handle for the file we open
	DIR dir; // Directory information for the current directory
	FATFS fso; // File System Object for the file system we are reading from
	
	//SendString("Initializing disk\n\r\\");
	
	// Wait for the disk to initialise
    while(disk_initialize(0));
    // Mount the disk
    f_mount(&fso, "", 0);
    // Change dir to the root directory
    f_chdir("/");
    // Open the directory
    f_opendir(&dir, ".");
 
	unsigned char buffer[1];
	unsigned int bytes;
	unsigned int result;
	unsigned char flag;
	
	result = f_open(&file, filename, FA_READ);
	if (result == 0)
	{		
		for (unsigned int i=0; i<8192; i++)
		{
			while (f_read(&file, &buffer[0], 1, &bytes) != 0) { } // MUST READ ONE BYTE AT A TIME!!!
			
			prg_ram[i] = buffer[0];
		}
		
		while (f_sync(&file) != 0) { }
		while (f_close(&file) != 0) { }
		
		//SendString("Read cart ram from file\n\r\\");
		
		flag = 1;
	}
	else
	{		
		//SendString("Could not read cart ram from file\n\r\\");
		
		flag = 0;
		
		nes_error(0x00);
	}	
	
	return flag;
}


// change for platform
unsigned char nes_burn(char *directory, char *filename)
{
	//sqi_write(directory, filename);
	
	for (unsigned long i=0x1D100000; i<0x1D200000; i+=0x00001000) // pages are 0x1000
	{
		NVMErasePage(i);
	}
	
	// Global variables
	FIL file; // File handle for the file we open
	DIR dir; // Directory information for the current directory
	FATFS fso; // File System Object for the file system we are reading from
	
	unsigned char buffer[4][1];
	unsigned long word[4];
	unsigned int bytes;
	unsigned int result;
	unsigned char flag;
	unsigned long addr;
	
	//SendString("Initializing disk\n\r\\");
	
	// Wait for the disk to initialise
	while(disk_initialize(0));
	// Mount the disk
	f_mount(&fso, "", 0);
	// Change dir to the root directory
	f_chdir(directory);
	// Open the directory
	f_opendir(&dir, ".");
	
	//SendString("Attempting to read\n\r\\");

	result = f_open(&file, filename, FA_READ);
	if (result == 0)
	{	
		for (addr=0; addr<0x00100000; addr+=16) // up to 1MB
		{	
			for (unsigned int pos=0; pos<4; pos++)
			{
				while (f_read(&file, &buffer[0], 1, &bytes) != 0) { } // MUST READ ONE BYTE AT A TIME!!!
				while (f_read(&file, &buffer[1], 1, &bytes) != 0) { } // MUST READ ONE BYTE AT A TIME!!!
				while (f_read(&file, &buffer[2], 1, &bytes) != 0) { } // MUST READ ONE BYTE AT A TIME!!!
				while (f_read(&file, &buffer[3], 1, &bytes) != 0) { } // MUST READ ONE BYTE AT A TIME!!!

				word[pos] = (buffer[3][0] << 24) + (buffer[2][0] << 16) + (buffer[1][0] << 8) + (buffer[0][0]);
			}
			
			if (bytes > 0) 
			{
				NVMWriteQuadWord(addr+0x1D100000, word[0], word[1], word[2], word[3]);
			}
			else break;	
		}

		while (f_sync(&file) != 0) { }
		while (f_close(&file) != 0) { }

		flag = 1;
		
		//SendString("Read successful\n\r\\");
	}
	else
	{
		//SendString("Read failure\n\r\\");
		
		flag = 0;
		
		nes_error(0x00);
	}
	
	
	DelayMS(1000);
	
	// soft reset system
	SYSKEY = 0x0; // reset
	SYSKEY = 0xAA996655; // unlock key #1
	SYSKEY = 0x556699AA; // unlock key #2
	RSWRST = 1; // set bit to reset of system
	SYSKEY = 0x0; // re-lock
	RSWRST; // read from register to reset
	while (1) { } // wait until reset occurs
	
	//return flag; // never gets returned
	return 0;
}

void nes_short_save(FIL *file, unsigned short val)
{
	unsigned int bytes;
	unsigned char buffer[0];
	
	buffer[0] = (unsigned char)((val & 0xFF00) >> 8);
	while (f_write(file, buffer, 1, &bytes) != 0) { }
	buffer[0] = (unsigned char)(val & 0x00FF);
	while (f_write(file, buffer, 1, &bytes) != 0) { }
}

void nes_long_save(FIL *file, unsigned long val)
{
	unsigned int bytes;
	unsigned char buffer[0];
	
	buffer[0] = (unsigned char)((val & 0xFF000000) >> 24);
	while (f_write(file, buffer, 1, &bytes) != 0) { }
	buffer[0] = (unsigned char)((val & 0x00FF0000) >> 16);
	while (f_write(file, buffer, 1, &bytes) != 0) { }
	buffer[0] = (unsigned char)((val & 0x0000FF00) >> 8);
	while (f_write(file, buffer, 1, &bytes) != 0) { }
	buffer[0] = (unsigned char)(val & 0x000000FF);
	while (f_write(file, buffer, 1, &bytes) != 0) { }
}

unsigned short nes_short_load(FIL *file)
{
	unsigned short val = 0x0000;
	
	unsigned int bytes;
	unsigned char buffer[0];
	
	while (f_read(file, &buffer[0], 1, &bytes) != 0) { } // MUST READ ONE BYTE AT A TIME!!!
	val = (val | ((unsigned short)buffer[0] << 8));
	while (f_read(file, &buffer[0], 1, &bytes) != 0) { } // MUST READ ONE BYTE AT A TIME!!!
	val = (val | (unsigned short)buffer[0]);
	
	return val;
}

unsigned long nes_long_load(FIL *file)
{
	unsigned long val = 0x00000000;
	
	unsigned int bytes;
	unsigned char buffer[0];
	
	while (f_read(file, &buffer[0], 1, &bytes) != 0) { } // MUST READ ONE BYTE AT A TIME!!!
	val = (val | ((unsigned long)buffer[0] << 24));
	while (f_read(file, &buffer[0], 1, &bytes) != 0) { } // MUST READ ONE BYTE AT A TIME!!!
	val = (val | ((unsigned long)buffer[0] << 16));
	while (f_read(file, &buffer[0], 1, &bytes) != 0) { } // MUST READ ONE BYTE AT A TIME!!!
	val = (val | ((unsigned long)buffer[0] << 8));
	while (f_read(file, &buffer[0], 1, &bytes) != 0) { } // MUST READ ONE BYTE AT A TIME!!!
	val = (val | (unsigned long)buffer[0]);
	
	return val;
}

unsigned char nes_state_save(char *filename)
{
	// Global variables
	FIL file; // File handle for the file we open
	DIR dir; // Directory information for the current directory
	FATFS fso; // File System Object for the file system we are reading from
	
	//SendString("Initializing disk\n\r\\");
	
	// Wait for the disk to initialise
    while(disk_initialize(0));
    // Mount the disk
    f_mount(&fso, "", 0);
    // Change dir to the root directory
    f_chdir("/");
    // Open the directory
    f_opendir(&dir, ".");
 
	unsigned char buffer[1];
	unsigned int bytes;
	unsigned int result;
	unsigned char flag;
	
	result = f_open(&file, filename, FA_CREATE_ALWAYS | FA_WRITE);
	if (result == 0)
	{		
		for (unsigned int i=0; i<2048; i++)
		{
			buffer[0] = cpu_ram[i];
			
			while (f_write(&file, buffer, 1, &bytes) != 0) { }
		}
		
		for (unsigned int i=0; i<2048; i++)
		{
			buffer[0] = ppu_ram[i];
			
			while (f_write(&file, buffer, 1, &bytes) != 0) { }
		}
		
		for (unsigned int i=0; i<8192; i++)
		{
			buffer[0] = prg_ram[i];
			
			while (f_write(&file, buffer, 1, &bytes) != 0) { }
		}
		
		for (unsigned int i=0; i<8192; i++)
		{
			buffer[0] = chr_ram[i];
			
			while (f_write(&file, buffer, 1, &bytes) != 0) { }
		}
		
		for (unsigned int i=0; i<256; i++)
		{
			buffer[0] = oam_ram[i];
			
			while (f_write(&file, buffer, 1, &bytes) != 0) { }
		}
		
		for (unsigned int i=0; i<32; i++)
		{
			buffer[0] = pal_ram[i];
			
			while (f_write(&file, buffer, 1, &bytes) != 0) { }
		}
		
		nes_long_save(&file, nes_hack_sprite_priority);

		nes_long_save(&file, nes_init_flag);
		nes_long_save(&file, nes_reset_flag);
		nes_long_save(&file, nes_audio_flag);
		nes_long_save(&file, nes_pixel_location);

		nes_long_save(&file, interrupt_count);
		
		nes_long_save(&file, cpu_current_cycles);
		nes_long_save(&file, cpu_dma_cycles);

		nes_long_save(&file, ppu_frame_cycles);
		nes_long_save(&file, ppu_scanline_cycles);
		nes_long_save(&file, ppu_tile_cycles);

		nes_long_save(&file, ppu_frame_count);
		nes_long_save(&file, (unsigned long)ppu_scanline_count);
		nes_long_save(&file, ppu_tile_count);

		nes_long_save(&file, apu_sample_cycles);

		nes_long_save(&file, map_number);
		
		nes_long_save(&file, map_unrom_bank);
		nes_long_save(&file, map_cnrom_bank);
		nes_long_save(&file, map_anrom_bank);

		nes_long_save(&file, map_mmc1_ready);
		nes_long_save(&file, map_mmc1_shift);
		nes_long_save(&file, map_mmc1_count);
		nes_long_save(&file, map_mmc1_prg_mode);
		nes_long_save(&file, map_mmc1_chr_mode);
		nes_long_save(&file, map_mmc1_chr_bank_0);
		nes_long_save(&file, map_mmc1_chr_bank_1);
		nes_long_save(&file, map_mmc1_prg_bank);
		nes_long_save(&file, map_mmc1_ram);
		
		nes_long_save(&file, map_mmc3_bank_next);
		nes_long_save(&file, map_mmc3_prg_mode);
		nes_long_save(&file, map_mmc3_chr_mode);
		nes_long_save(&file, map_mmc3_bank_r0);
		nes_long_save(&file, map_mmc3_bank_r1);
		nes_long_save(&file, map_mmc3_bank_r2);
		nes_long_save(&file, map_mmc3_bank_r3);
		nes_long_save(&file, map_mmc3_bank_r4);
		nes_long_save(&file, map_mmc3_bank_r5);
		nes_long_save(&file, map_mmc3_bank_r6);
		nes_long_save(&file, map_mmc3_bank_r7);
		nes_long_save(&file, map_mmc3_ram);
		nes_long_save(&file, map_mmc3_irq_latch);
		nes_long_save(&file, map_mmc3_irq_counter);
		nes_long_save(&file, map_mmc3_irq_enable);
		nes_long_save(&file, map_mmc3_irq_previous);
		nes_long_save(&file, map_mmc3_irq_interrupt);
		nes_long_save(&file, map_mmc3_irq_reload);
		nes_long_save(&file, map_mmc3_irq_a12);
		nes_long_save(&file, map_mmc3_irq_delay);
		nes_long_save(&file, map_mmc3_irq_shift);
		
		nes_long_save(&file, cpu_reg_a);
		nes_long_save(&file, cpu_reg_x);
		nes_long_save(&file, cpu_reg_y);
		nes_long_save(&file, cpu_reg_s);
		
		nes_long_save(&file, cpu_flag_c);
		nes_long_save(&file, cpu_flag_z);
		nes_long_save(&file, cpu_flag_v);
		nes_long_save(&file, cpu_flag_n);
		nes_long_save(&file, cpu_flag_d);
		nes_long_save(&file, cpu_flag_b);
		nes_long_save(&file, cpu_flag_i);
		
		nes_long_save(&file, cpu_reg_pc);

		nes_long_save(&file, cpu_temp_opcode);
		nes_long_save(&file, cpu_temp_memory);
		nes_long_save(&file, cpu_temp_address);
		nes_long_save(&file, cpu_temp_result);
		nes_long_save(&file, cpu_temp_cycles);

		nes_long_save(&file, cpu_status_r);

		nes_long_save(&file, ppu_reg_v);
		nes_long_save(&file, ppu_reg_t);
		nes_long_save(&file, ppu_reg_w);
		nes_long_save(&file, ppu_reg_a);
		nes_long_save(&file, ppu_reg_b);
		nes_long_save(&file, ppu_reg_x);

		nes_long_save(&file, ppu_flag_e);
		nes_long_save(&file, ppu_flag_p);
		nes_long_save(&file, ppu_flag_h);
		nes_long_save(&file, ppu_flag_b);
		nes_long_save(&file, ppu_flag_s);
		nes_long_save(&file, ppu_flag_i);
		nes_long_save(&file, ppu_flag_v);
		nes_long_save(&file, ppu_flag_0);
		nes_long_save(&file, ppu_flag_o);

		nes_long_save(&file, ppu_flag_g);
		nes_long_save(&file, ppu_flag_lb);
		nes_long_save(&file, ppu_flag_ls);
		nes_long_save(&file, ppu_flag_eb);
		nes_long_save(&file, ppu_flag_es);

		nes_long_save(&file, ppu_status_0);
		nes_long_save(&file, ppu_status_v);
		nes_long_save(&file, (unsigned long)ppu_status_s);
		nes_long_save(&file, (unsigned long)ppu_status_d);
		nes_long_save(&file, ppu_status_m);

		nes_long_save(&file, ctl_flag_s);
		nes_long_save(&file, ctl_value_1);
		nes_long_save(&file, ctl_value_2);
		nes_long_save(&file, ctl_latch_1);
		nes_long_save(&file, ctl_latch_2);
		
		nes_short_save(&file, apu_pulse_1_d);
		nes_short_save(&file, apu_pulse_1_u);
		nes_short_save(&file, apu_pulse_1_i);
		nes_short_save(&file, apu_pulse_1_c);
		nes_short_save(&file, apu_pulse_1_v);
		nes_short_save(&file, apu_pulse_1_m);
		nes_short_save(&file, apu_pulse_1_r);
		nes_short_save(&file, apu_pulse_1_s);
		nes_short_save(&file, apu_pulse_1_a);
		nes_short_save(&file, apu_pulse_1_n);
		nes_short_save(&file, apu_pulse_1_p);
		nes_short_save(&file, apu_pulse_1_w);
		nes_short_save(&file, apu_pulse_1_e);
		nes_short_save(&file, apu_pulse_1_t);
		nes_short_save(&file, apu_pulse_1_k);
		nes_short_save(&file, apu_pulse_1_l);
		nes_short_save(&file, apu_pulse_1_o);
		
		nes_short_save(&file, apu_pulse_2_d);
		nes_short_save(&file, apu_pulse_2_u);
		nes_short_save(&file, apu_pulse_2_i);
		nes_short_save(&file, apu_pulse_2_c);
		nes_short_save(&file, apu_pulse_2_v);
		nes_short_save(&file, apu_pulse_2_m);
		nes_short_save(&file, apu_pulse_2_r);
		nes_short_save(&file, apu_pulse_2_s);
		nes_short_save(&file, apu_pulse_2_a);
		nes_short_save(&file, apu_pulse_2_n);
		nes_short_save(&file, apu_pulse_2_p);
		nes_short_save(&file, apu_pulse_2_w);
		nes_short_save(&file, apu_pulse_2_e);
		nes_short_save(&file, apu_pulse_2_t);
		nes_short_save(&file, apu_pulse_2_k);
		nes_short_save(&file, apu_pulse_2_l);
		nes_short_save(&file, apu_pulse_2_o);

		nes_short_save(&file, apu_triangle_c);
		nes_short_save(&file, apu_triangle_r);
		nes_short_save(&file, apu_triangle_v);
		nes_short_save(&file, apu_triangle_f);
		nes_short_save(&file, apu_triangle_h);
		nes_short_save(&file, apu_triangle_q);
		nes_short_save(&file, apu_triangle_t);
		nes_short_save(&file, apu_triangle_k);
		nes_short_save(&file, apu_triangle_l);
		nes_short_save(&file, apu_triangle_p);
		nes_short_save(&file, apu_triangle_d);
		nes_short_save(&file, apu_triangle_o);

		nes_short_save(&file, apu_noise_i);
		nes_short_save(&file, apu_noise_c);
		nes_short_save(&file, apu_noise_v);
		nes_short_save(&file, apu_noise_m);
		nes_short_save(&file, apu_noise_r);
		nes_short_save(&file, apu_noise_s);
		nes_short_save(&file, apu_noise_x);
		nes_short_save(&file, apu_noise_d);
		nes_short_save(&file, apu_noise_t);
		nes_short_save(&file, apu_noise_k);
		nes_short_save(&file, apu_noise_l);
		nes_short_save(&file, apu_noise_o);
		
		nes_short_save(&file, apu_dmc_i);
		nes_short_save(&file, apu_dmc_l);
		nes_short_save(&file, apu_dmc_r);
		nes_short_save(&file, apu_dmc_k);
		nes_short_save(&file, apu_dmc_d);
		nes_short_save(&file, apu_dmc_a);
		nes_short_save(&file, apu_dmc_s);
		nes_short_save(&file, apu_dmc_b);
		nes_short_save(&file, apu_dmc_t);
		nes_short_save(&file, apu_dmc_o);

		nes_short_save(&file, apu_flag_i);
		nes_short_save(&file, apu_flag_f);
		nes_short_save(&file, apu_flag_d);
		nes_short_save(&file, apu_flag_n);
		nes_short_save(&file, apu_flag_t);
		nes_short_save(&file, apu_flag_2);
		nes_short_save(&file, apu_flag_1);
		nes_short_save(&file, apu_flag_m);
		nes_short_save(&file, apu_flag_b);
		
		nes_short_save(&file, apu_counter_q);
		nes_short_save(&file, apu_counter_s);
		nes_short_save(&file, apu_mixer_output);
		
		while (f_sync(&file) != 0) { }
		while (f_close(&file) != 0) { }
		
		//SendString("Wrote all memory to file\n\r\\");
		
		flag = 1;
	}
	else
	{
		//SendString("Could not write all memory to file\n\r\\");
		
		flag = 0;
		
		nes_error(0x00);
	}	
	
	return flag;
}

unsigned char nes_state_load(char *filename)
{
	// Global variables
	FIL file; // File handle for the file we open
	DIR dir; // Directory information for the current directory
	FATFS fso; // File System Object for the file system we are reading from
	
	//SendString("Initializing disk\n\r\\");
	
	// Wait for the disk to initialise
    while(disk_initialize(0));
    // Mount the disk
    f_mount(&fso, "", 0);
    // Change dir to the root directory
    f_chdir("/");
    // Open the directory
    f_opendir(&dir, ".");
 
	unsigned char buffer[1];
	unsigned int bytes;
	unsigned int result;
	unsigned char flag;
	
	result = f_open(&file, filename, FA_READ);
	if (result == 0)
	{		
		for (unsigned int i=0; i<2048; i++)
		{
			while (f_read(&file, &buffer[0], 1, &bytes) != 0) { } // MUST READ ONE BYTE AT A TIME!!!
			
			cpu_ram[i] = buffer[0];
		}
		
		for (unsigned int i=0; i<2048; i++)
		{
			while (f_read(&file, &buffer[0], 1, &bytes) != 0) { } // MUST READ ONE BYTE AT A TIME!!!
			
			ppu_ram[i] = buffer[0];
		}
		
		for (unsigned int i=0; i<8192; i++)
		{
			while (f_read(&file, &buffer[0], 1, &bytes) != 0) { } // MUST READ ONE BYTE AT A TIME!!!
			
			prg_ram[i] = buffer[0];
		}
		
		for (unsigned int i=0; i<8192; i++)
		{
			while (f_read(&file, &buffer[0], 1, &bytes) != 0) { } // MUST READ ONE BYTE AT A TIME!!!
			
			chr_ram[i] = buffer[0];
		}
		
		for (unsigned int i=0; i<256; i++)
		{
			while (f_read(&file, &buffer[0], 1, &bytes) != 0) { } // MUST READ ONE BYTE AT A TIME!!!
			
			oam_ram[i] = buffer[0];
		}
		
		for (unsigned int i=0; i<32; i++)
		{
			while (f_read(&file, &buffer[0], 1, &bytes) != 0) { } // MUST READ ONE BYTE AT A TIME!!!
			
			pal_ram[i] = buffer[0];
		}
		
		nes_hack_sprite_priority = nes_long_load(&file);

		nes_init_flag = nes_long_load(&file);
		nes_reset_flag = nes_long_load(&file);
		nes_audio_flag = nes_long_load(&file);
		nes_pixel_location = nes_long_load(&file);

		interrupt_count = nes_long_load(&file);
		
		cpu_current_cycles = nes_long_load(&file);
		cpu_dma_cycles = nes_long_load(&file);

		ppu_frame_cycles = nes_long_load(&file);
		ppu_scanline_cycles = nes_long_load(&file);
		ppu_tile_cycles = nes_long_load(&file);

		ppu_frame_count = nes_long_load(&file);
		ppu_scanline_count = (signed long)nes_long_load(&file);
		ppu_tile_count = nes_long_load(&file);

		apu_sample_cycles = nes_long_load(&file);

		map_number = nes_long_load(&file);
		
		map_unrom_bank = nes_long_load(&file);
		map_cnrom_bank = nes_long_load(&file);
		map_anrom_bank = nes_long_load(&file);

		map_mmc1_ready = nes_long_load(&file);
		map_mmc1_shift = nes_long_load(&file);
		map_mmc1_count = nes_long_load(&file);
		map_mmc1_prg_mode = nes_long_load(&file);
		map_mmc1_chr_mode = nes_long_load(&file);
		map_mmc1_chr_bank_0 = nes_long_load(&file);
		map_mmc1_chr_bank_1 = nes_long_load(&file);
		map_mmc1_prg_bank = nes_long_load(&file);
		map_mmc1_ram = nes_long_load(&file);
		
		map_mmc3_bank_next = nes_long_load(&file);
		map_mmc3_prg_mode = nes_long_load(&file);
		map_mmc3_chr_mode = nes_long_load(&file);
		map_mmc3_bank_r0 = nes_long_load(&file);
		map_mmc3_bank_r1 = nes_long_load(&file);
		map_mmc3_bank_r2 = nes_long_load(&file);
		map_mmc3_bank_r3 = nes_long_load(&file);
		map_mmc3_bank_r4 = nes_long_load(&file);
		map_mmc3_bank_r5 = nes_long_load(&file);
		map_mmc3_bank_r6 = nes_long_load(&file);
		map_mmc3_bank_r7 = nes_long_load(&file);
		map_mmc3_ram = nes_long_load(&file);
		map_mmc3_irq_latch = nes_long_load(&file);
		map_mmc3_irq_counter = nes_long_load(&file);
		map_mmc3_irq_enable = nes_long_load(&file);
		map_mmc3_irq_previous = nes_long_load(&file);
		map_mmc3_irq_interrupt = nes_long_load(&file);
		map_mmc3_irq_reload = nes_long_load(&file);
		map_mmc3_irq_a12 = nes_long_load(&file);
		map_mmc3_irq_delay = nes_long_load(&file);
		map_mmc3_irq_shift = nes_long_load(&file);
		
		cpu_reg_a = nes_long_load(&file);
		cpu_reg_x = nes_long_load(&file);
		cpu_reg_y = nes_long_load(&file);
		cpu_reg_s = nes_long_load(&file);
		
		cpu_flag_c = nes_long_load(&file);
		cpu_flag_z = nes_long_load(&file);
		cpu_flag_v = nes_long_load(&file);
		cpu_flag_n = nes_long_load(&file);
		cpu_flag_d = nes_long_load(&file);
		cpu_flag_b = nes_long_load(&file);
		cpu_flag_i = nes_long_load(&file);
		
		cpu_reg_pc = nes_long_load(&file);

		cpu_temp_opcode = nes_long_load(&file);
		cpu_temp_memory = nes_long_load(&file);
		cpu_temp_address = nes_long_load(&file);
		cpu_temp_result = nes_long_load(&file);
		cpu_temp_cycles = nes_long_load(&file);

		cpu_status_r = nes_long_load(&file);

		ppu_reg_v = nes_long_load(&file);
		ppu_reg_t = nes_long_load(&file);
		ppu_reg_w = nes_long_load(&file);
		ppu_reg_a = nes_long_load(&file);
		ppu_reg_b = nes_long_load(&file);
		ppu_reg_x = nes_long_load(&file);

		ppu_flag_e = nes_long_load(&file);
		ppu_flag_p = nes_long_load(&file);
		ppu_flag_h = nes_long_load(&file);
		ppu_flag_b = nes_long_load(&file);
		ppu_flag_s = nes_long_load(&file);
		ppu_flag_i = nes_long_load(&file);
		ppu_flag_v = nes_long_load(&file);
		ppu_flag_0 = nes_long_load(&file);
		ppu_flag_o = nes_long_load(&file);

		ppu_flag_g = nes_long_load(&file);
		ppu_flag_lb = nes_long_load(&file);
		ppu_flag_ls = nes_long_load(&file);
		ppu_flag_eb = nes_long_load(&file);
		ppu_flag_es = nes_long_load(&file);

		ppu_status_0 = nes_long_load(&file);
		ppu_status_v = nes_long_load(&file);
		ppu_status_s = (signed long)nes_long_load(&file);
		ppu_status_d = (signed long)nes_long_load(&file);
		ppu_status_m = nes_long_load(&file);

		ctl_flag_s = nes_long_load(&file);
		ctl_value_1 = nes_long_load(&file);
		ctl_value_2 = nes_long_load(&file);
		ctl_latch_1 = nes_long_load(&file);
		ctl_latch_2 = nes_long_load(&file);
		
		apu_pulse_1_d = nes_short_load(&file);
		apu_pulse_1_u = nes_short_load(&file);
		apu_pulse_1_i = nes_short_load(&file);
		apu_pulse_1_c = nes_short_load(&file);
		apu_pulse_1_v = nes_short_load(&file);
		apu_pulse_1_m = nes_short_load(&file);
		apu_pulse_1_r = nes_short_load(&file);
		apu_pulse_1_s = nes_short_load(&file);
		apu_pulse_1_a = nes_short_load(&file);
		apu_pulse_1_n = nes_short_load(&file);
		apu_pulse_1_p = nes_short_load(&file);
		apu_pulse_1_w = nes_short_load(&file);
		apu_pulse_1_e = nes_short_load(&file);
		apu_pulse_1_t = nes_short_load(&file);
		apu_pulse_1_k = nes_short_load(&file);
		apu_pulse_1_l = nes_short_load(&file);
		apu_pulse_1_o = nes_short_load(&file);
		
		apu_pulse_2_d = nes_short_load(&file);
		apu_pulse_2_u = nes_short_load(&file);
		apu_pulse_2_i = nes_short_load(&file);
		apu_pulse_2_c = nes_short_load(&file);
		apu_pulse_2_v = nes_short_load(&file);
		apu_pulse_2_m = nes_short_load(&file);
		apu_pulse_2_r = nes_short_load(&file);
		apu_pulse_2_s = nes_short_load(&file);
		apu_pulse_2_a = nes_short_load(&file);
		apu_pulse_2_n = nes_short_load(&file);
		apu_pulse_2_p = nes_short_load(&file);
		apu_pulse_2_w = nes_short_load(&file);
		apu_pulse_2_e = nes_short_load(&file);
		apu_pulse_2_t = nes_short_load(&file);
		apu_pulse_2_k = nes_short_load(&file);
		apu_pulse_2_l = nes_short_load(&file);
		apu_pulse_2_o = nes_short_load(&file);

		apu_triangle_c = nes_short_load(&file);
		apu_triangle_r = nes_short_load(&file);
		apu_triangle_v = nes_short_load(&file);
		apu_triangle_f = nes_short_load(&file);
		apu_triangle_h = nes_short_load(&file);
		apu_triangle_q = nes_short_load(&file);
		apu_triangle_t = nes_short_load(&file);
		apu_triangle_k = nes_short_load(&file);
		apu_triangle_l = nes_short_load(&file);
		apu_triangle_p = nes_short_load(&file);
		apu_triangle_d = nes_short_load(&file);
		apu_triangle_o = nes_short_load(&file);

		apu_noise_i = nes_short_load(&file);
		apu_noise_c = nes_short_load(&file);
		apu_noise_v = nes_short_load(&file);
		apu_noise_m = nes_short_load(&file);
		apu_noise_r = nes_short_load(&file);
		apu_noise_s = nes_short_load(&file);
		apu_noise_x = nes_short_load(&file);
		apu_noise_d = nes_short_load(&file);
		apu_noise_t = nes_short_load(&file);
		apu_noise_k = nes_short_load(&file);
		apu_noise_l = nes_short_load(&file);
		apu_noise_o = nes_short_load(&file);
		
		apu_dmc_i = nes_short_load(&file);
		apu_dmc_l = nes_short_load(&file);
		apu_dmc_r = nes_short_load(&file);
		apu_dmc_k = nes_short_load(&file);
		apu_dmc_d = nes_short_load(&file);
		apu_dmc_a = nes_short_load(&file);
		apu_dmc_s = nes_short_load(&file);
		apu_dmc_b = nes_short_load(&file);
		apu_dmc_t = nes_short_load(&file);
		apu_dmc_o = nes_short_load(&file);

		apu_flag_i = nes_short_load(&file);
		apu_flag_f = nes_short_load(&file);
		apu_flag_d = nes_short_load(&file);
		apu_flag_n = nes_short_load(&file);
		apu_flag_t = nes_short_load(&file);
		apu_flag_2 = nes_short_load(&file);
		apu_flag_1 = nes_short_load(&file);
		apu_flag_m = nes_short_load(&file);
		apu_flag_b = nes_short_load(&file);
		
		apu_counter_q = nes_short_load(&file);
		apu_counter_s = nes_short_load(&file);
		apu_mixer_output = nes_short_load(&file);		
		
		while (f_sync(&file) != 0) { }
		while (f_close(&file) != 0) { }
		
		//SendString("Read all memory from file\n\r\\");
		
		flag = 1;
	}
	else
	{		
		//SendString("Could not read all memory from file\n\r\\");
		
		flag = 0;
		
		nes_error(0x00);
	}	
	
	return flag;
}
*/

void nes_pixel_hdmi_raw(unsigned short pos_x, unsigned short pos_y, unsigned short color)
{
	nes_pixel_location = ((pos_y*2))*640+(((pos_x+16)*2));
	screen_large_buffer[(nes_pixel_location)] = (unsigned short)color;
	screen_large_buffer[(nes_pixel_location+1)] = (unsigned short)color;
	nes_pixel_location += 640;
	screen_large_buffer[(nes_pixel_location)] = (unsigned short)color;
	screen_large_buffer[(nes_pixel_location+1)] = (unsigned short)color;
}

void nes_pixel_lcd_raw(unsigned short pos_x, unsigned short pos_y, unsigned short color)
{
	nes_pixel_location = ((pos_y))*320+((pos_x+16));
	screen_small_buffer[(nes_pixel_location)] = (unsigned short)color;
}

void nes_pixel_hdmi_pal(unsigned short pos_x, unsigned short pos_y, unsigned short color)
{
	nes_pixel_val = nes_palette[color];
	nes_pixel_location = (((pos_y)*2))*640+(((pos_x+16)*2));
	screen_large_buffer[(nes_pixel_location)] = (unsigned short)nes_pixel_val;
	screen_large_buffer[(nes_pixel_location+1)] = (unsigned short)nes_pixel_val;
	nes_pixel_location += 640;
	screen_large_buffer[(nes_pixel_location)] = (unsigned short)nes_pixel_val;
	screen_large_buffer[(nes_pixel_location+1)] = (unsigned short)nes_pixel_val;
}

void nes_pixel_lcd_pal(unsigned short pos_x, unsigned short pos_y, unsigned short color)
{
	nes_pixel_val = nes_palette[color];
	nes_pixel_location = ((pos_y))*320+((pos_x+16));
	screen_small_buffer[(nes_pixel_location)] = (unsigned short)nes_pixel_val;
}

void nes_sound(unsigned char sample)
{
	audio_buffer[(audio_write)%AUDIO_LEN] = sample;
	
	audio_write = audio_write + 1;
	
	if (audio_write >= AUDIO_LEN)
	{
		audio_write = 0;
	}
}
	
void nes_buttons()
{
	//ctl_value_1 = 0xFF080000 | (controller_status_3 << 8) | controller_status_1;
	//ctl_value_2 = 0xFF040000 | (controller_status_4 << 8) | controller_status_2;

	ctl_value_1 = 0xFF080000;
	ctl_value_2 = 0xFF040000;

	buttons_file = open("/home/username/PICnes/keyboard.val", O_RDONLY);
	read(buttons_file, &buttons_buffer, 13);
	close(buttons_file);

	// get key inputs
	if (buttons_buffer[0] != '0') nes_running = 0; // exit
	if (buttons_buffer[1] != '0') ctl_value_1 = (ctl_value_1 | 0x10);
	if (buttons_buffer[2] != '0') ctl_value_1 = (ctl_value_1 | 0x20);
	if (buttons_buffer[3] != '0') ctl_value_1 = (ctl_value_1 | 0x40);
	if (buttons_buffer[4] != '0') ctl_value_1 = (ctl_value_1 | 0x80);
	if (buttons_buffer[5] != '0') ctl_value_1 = (ctl_value_1 | 0x04);
	if (buttons_buffer[6] != '0') ctl_value_1 = (ctl_value_1 | 0x08);
	if (buttons_buffer[7] != '0') ctl_value_1 = (ctl_value_1 | 0x01);
	if (buttons_buffer[8] != '0') ctl_value_1 = (ctl_value_1 | 0x02);
	if (buttons_buffer[11] != '0') { }
	if (buttons_buffer[12] != '0') { }

	buttons_file = open("/home/username/PICnes/joystick.val", O_RDONLY);
	read(buttons_file, &buttons_buffer, 13);
	close(buttons_file);

	// get key inputs
	if (buttons_buffer[0] != '0') nes_running = 0; // exit
	if (buttons_buffer[1] != '0') ctl_value_1 = (ctl_value_1 | 0x10);
	if (buttons_buffer[2] != '0') ctl_value_1 = (ctl_value_1 | 0x20);
	if (buttons_buffer[3] != '0') ctl_value_1 = (ctl_value_1 | 0x40);
	if (buttons_buffer[4] != '0') ctl_value_1 = (ctl_value_1 | 0x80);
	if (buttons_buffer[5] != '0') ctl_value_1 = (ctl_value_1 | 0x04);
	if (buttons_buffer[6] != '0') ctl_value_1 = (ctl_value_1 | 0x08);
	if (buttons_buffer[7] != '0') ctl_value_1 = (ctl_value_1 | 0x01);
	if (buttons_buffer[8] != '0') ctl_value_1 = (ctl_value_1 | 0x02);
	if (buttons_buffer[11] != '0') { }
	if (buttons_buffer[12] != '0') { }
}

unsigned long previous_clock = 0;

// needs to be unoptimized else it will be deleted
void nes_wait()
{
	while (clock() < previous_clock + 16340) { } // for 60.0988 Hz
	previous_clock = clock();	
}

unsigned char nes_read_cpu_ram(unsigned long addr)
{
	//return cpu_ram[(addr&2047)];
	return cpu_ram[addr];
}

void nes_write_cpu_ram(unsigned long addr, unsigned char val)
{
	//cpu_ram[(addr&2047)] = val;
	cpu_ram[addr] = val;
}

unsigned char nes_read_ppu_ram(unsigned long addr)
{
	//return ppu_ram[(addr&2047)];
	return ppu_ram[addr];
}

void nes_write_ppu_ram(unsigned long addr, unsigned char val)
{
	//ppu_ram[(addr&2047)] = val;
	ppu_ram[addr] = val;
}

unsigned char nes_read_prg_ram(unsigned long addr)
{
	//return prg_ram[(addr&8191)];
	return prg_ram[addr];
}

void nes_write_prg_ram(unsigned long addr, unsigned char val)
{
	//prg_ram[(addr&8191)] = val;
	prg_ram[addr] = val;
}

unsigned char nes_read_chr_ram(unsigned long addr)
{
	//return chr_ram[(addr&8191)];
	return chr_ram[addr];
}

void nes_write_chr_ram(unsigned long addr, unsigned char val)
{
	//chr_ram[(addr&8191)] = val;
	chr_ram[addr] = val;
}

unsigned char nes_read_oam_ram(unsigned long addr)
{
	//return oam_ram[(addr&255)];
	return oam_ram[addr];
}

void nes_write_oam_ram(unsigned long addr, unsigned char val)
{
	//oam_ram[(addr&255)] = val;
	oam_ram[addr] = val;
}

unsigned char nes_read_pal_ram(unsigned long addr)
{
	//return pal_ram[(addr&31)];
	return pal_ram[addr];
}

void nes_write_pal_ram(unsigned long addr, unsigned char val)
{
	//pal_ram[(addr&31)] = val;
	pal_ram[addr] = val;
}

unsigned long nes_cart_addr = 0;

unsigned char nes_read_cart_rom(unsigned long addr)
{
	//sqi_prepare(addr);
	//return (unsigned char)sqi_read();
	
	return (unsigned char)cart_rom[addr];
}

void nes_mmc3_irq_toggle(unsigned long a12)
{
	if (map_mmc3_irq_a12 == 0 && a12 == 1)
	{	
		if (map_mmc3_irq_counter == 0 || map_mmc3_irq_reload == 1)
		{
			map_mmc3_irq_counter = map_mmc3_irq_latch - map_mmc3_irq_shift + 1;
	
			map_mmc3_irq_reload = 0;

			// Sharp MMC3 behavior??? (not NEC compatible!!!)
			if (map_mmc3_irq_latch == 0)
			{
				map_mmc3_irq_interrupt = 1;
			}
			else
			{
				map_mmc3_irq_interrupt = 0;
			}
		}
		else
		{
			map_mmc3_irq_counter = map_mmc3_irq_counter - 1;
		}

		if (map_mmc3_irq_counter == 0 && map_mmc3_irq_previous == 1) map_mmc3_irq_interrupt = 1;

		map_mmc3_irq_previous = map_mmc3_irq_counter;
	}

	map_mmc3_irq_a12 = a12;
}

unsigned char cpu_read(unsigned long addr)
{	
	nes_loop_read = 1;
	
	if (addr < 0x00002000)
	{
		return nes_read_cpu_ram((addr&0x000007FF)); // internal ram (and mirrors)
	}
	else if (addr < 0x00004000) // ppu (and mirrors)
	{
		switch ((addr&0x00000007))
		{
			case 0x00: // ppuctrl
			{
				return 0x00;
				
				break;
			}
			case 0x01: // ppumask
			{
				return 0x00;
				
				break;
			}
			case 0x02: // ppustatus
			{				
				unsigned char val = ((ppu_flag_v << 7) | (ppu_flag_0 << 6) | (ppu_flag_o << 5));
				
				ppu_flag_v = 0x0000;
				
				ppu_reg_w = 0x0000;
				
				return val;
				
				break;
			}
			case 0x03: // oamaddr
			{
				return 0x00;
				
				break;
			}
			case 0x04: // oamdata
			{
				return nes_read_oam_ram(ppu_reg_a);
				
				break;
			}
			case 0x05: // ppuscroll
			{
				return 0x00;
				
				break;
			}
			case 0x06: // ppuaddr
			{
				return 0x00;
				
				break;
			}
			case 0x07: // ppudata
			{	
				unsigned char val = ppu_reg_b;
				
				if (ppu_reg_v < 0x2000)
				{
					if ((unsigned char)cart_header[5] > 0) // chr_rom
					{
						if (map_number == 1) // mmc1
						{
							if (map_mmc1_chr_mode == 0) // 8KB
							{
								if ((unsigned char)cart_header[4] <= 0x10) // 256KB or less
								{
									ppu_reg_b = nes_read_cart_rom(((chr_offset+0x00001000*(map_mmc1_chr_bank_0&0x1E)+ppu_reg_v)));
								}
								else
								{
									
								}
							}
							else if (map_mmc1_chr_mode == 1) // 4KB banked
							{
								if ((unsigned char)cart_header[4] <= 0x10) // 256KB or less
								{
									if (ppu_reg_v < 0x1000)
									{
										ppu_reg_b = nes_read_cart_rom(((chr_offset+0x00001000*(map_mmc1_chr_bank_0)+ppu_reg_v)));
									}
									else
									{
										ppu_reg_b = nes_read_cart_rom(((chr_offset+0x00001000*(map_mmc1_chr_bank_1)+ppu_reg_v-0x1000)));
									}
								}
								else
								{
									
								}
							}
						}
						else if (map_number == 2) // unrom
						{
							ppu_reg_b = nes_read_cart_rom(((chr_offset+ppu_reg_v)));
						}
						else if (map_number == 3) // cnrom
						{
							ppu_reg_b = nes_read_cart_rom(((chr_offset+0x2000*map_cnrom_bank+ppu_reg_v)));
						}
						else if (map_number == 4) // mmc3
						{							
							if (map_mmc3_chr_mode == 0x0000)
							{
								switch ((ppu_reg_v&0xFC00))
								{
									case 0x0000:
									{
										ppu_reg_b = nes_read_cart_rom(((chr_offset+0x0800*map_mmc3_bank_r0+ppu_reg_v)));
										break;
									}
									case 0x0400:
									{
										ppu_reg_b = nes_read_cart_rom(((chr_offset+0x0800*map_mmc3_bank_r0+ppu_reg_v)));
										break;
									}
									case 0x0800:
									{
										ppu_reg_b = nes_read_cart_rom(((chr_offset+0x0800*map_mmc3_bank_r1+ppu_reg_v-0x0800)));
										break;
									}
									case 0x0C00:
									{
										ppu_reg_b = nes_read_cart_rom(((chr_offset+0x0800*map_mmc3_bank_r1+ppu_reg_v-0x0800)));
										break;
									}
									case 0x1000:
									{
										ppu_reg_b = nes_read_cart_rom(((chr_offset+0x0400*map_mmc3_bank_r2+ppu_reg_v-0x1000)));
										break;
									}
									case 0x1400:
									{
										ppu_reg_b = nes_read_cart_rom(((chr_offset+0x0400*map_mmc3_bank_r3+ppu_reg_v-0x1400)));
										break;
									}
									case 0x1800:
									{
										ppu_reg_b = nes_read_cart_rom(((chr_offset+0x0400*map_mmc3_bank_r4+ppu_reg_v-0x1800)));
										break;
									}
									case 0x1C00:
									{
										ppu_reg_b = nes_read_cart_rom(((chr_offset+0x0400*map_mmc3_bank_r5+ppu_reg_v-0x1C00)));
										break;
									}
								}
							}
							else
							{
								switch ((ppu_reg_v&0xFC00))
								{
									case 0x0000:
									{
										ppu_reg_b = nes_read_cart_rom(((chr_offset+0x0400*map_mmc3_bank_r2+ppu_reg_v)));
										break;
									}
									case 0x0400:
									{
										ppu_reg_b = nes_read_cart_rom(((chr_offset+0x0400*map_mmc3_bank_r3+ppu_reg_v-0x0400)));
										break;
									}
									case 0x0800:
									{
										ppu_reg_b = nes_read_cart_rom(((chr_offset+0x0400*map_mmc3_bank_r4+ppu_reg_v-0x0800)));
										break;
									}
									case 0x0C00:
									{
										ppu_reg_b = nes_read_cart_rom(((chr_offset+0x0400*map_mmc3_bank_r5+ppu_reg_v-0x0C00)));
										break;
									}
									case 0x1000:
									{
										ppu_reg_b = nes_read_cart_rom(((chr_offset+0x0800*map_mmc3_bank_r0+ppu_reg_v-0x1000)));
										break;
									}
									case 0x1400:
									{
										ppu_reg_b = nes_read_cart_rom(((chr_offset+0x0800*map_mmc3_bank_r0+ppu_reg_v-0x1000)));
										break;
									}
									case 0x1800:
									{
										ppu_reg_b = nes_read_cart_rom(((chr_offset+0x0800*map_mmc3_bank_r1+ppu_reg_v-0x1800)));
										break;
									}
									case 0x1C00:
									{
										ppu_reg_b = nes_read_cart_rom(((chr_offset+0x0800*map_mmc3_bank_r1+ppu_reg_v-0x1800)));
										break;
									}
								}
							}
						}
						else // nrom
						{
							ppu_reg_b = nes_read_cart_rom(((chr_offset+ppu_reg_v)));
						}
					}
					else // chr_ram
					{
						ppu_reg_b = nes_read_chr_ram(ppu_reg_v);
					}
				}
				else if (ppu_reg_v >= 0x2000 && ppu_reg_v < 0x3000)
				{
					if (ppu_reg_v < 0x2400)
					{
						if (ppu_status_m == 0x0000 || ppu_status_m == 0x0001 || ppu_status_m == 0x0002)
						{
							ppu_reg_b = nes_read_ppu_ram(ppu_reg_v-0x2000);
						}
						else if (ppu_status_m == 0x0003)
						{
							ppu_reg_b = nes_read_ppu_ram(ppu_reg_v-0x2000+0x0400);
						}
					}
					else if (ppu_reg_v < 0x2800)
					{
						if (ppu_status_m == 0x0000 || ppu_status_m == 0x0002) // vertical scrolling
						{
							ppu_reg_b = nes_read_ppu_ram(ppu_reg_v-0x2400);
						}
						else if (ppu_status_m == 0x0001 || ppu_status_m == 0x0003) // horizontal scrolling
						{
							ppu_reg_b = nes_read_ppu_ram(ppu_reg_v-0x2000);
						}
					}
					else if (ppu_reg_v < 0x2C00)
					{
						if (ppu_status_m == 0x0000 || ppu_status_m == 0x0003) // vertical scrolling
						{
							ppu_reg_b = nes_read_ppu_ram(ppu_reg_v-0x2400);
						}
						else if (ppu_status_m == 0x0001 || ppu_status_m == 0x0002) // horizontal scrolling
						{
							ppu_reg_b = nes_read_ppu_ram(ppu_reg_v-0x2800);
						}
					}
					else
					{
						if (ppu_status_m == 0x0000 || ppu_status_m == 0x0001 || ppu_status_m == 0x0003)
						{
							ppu_reg_b = nes_read_ppu_ram(ppu_reg_v-0x2800);
						}
						else if (ppu_status_m == 0x0002)
						{
							ppu_reg_b = nes_read_ppu_ram(ppu_reg_v-0x2C00);
						}
					}
				}
				else if (ppu_reg_v >= 0x3F00 && ppu_reg_v < 0x4000)
				{
					ppu_reg_b = nes_read_pal_ram((ppu_reg_v&0x001F));
				}
				 
				if (ppu_flag_i == 0x0000)
				{
					ppu_reg_v += 0x0001;
				}
				else
				{
					ppu_reg_v += 0x0020;
				}
				
				return val;
				
				break;
			}
		}
	}
	else if (addr < 0x00004018) // apu and i/o
	{
		switch ((addr&0x000000FF))
		{
			case 0x15: // apu status
			{
				unsigned char val = ((apu_flag_i << 7) |
					(apu_flag_f << 6) |
					(apu_flag_d << 4) |
					(apu_flag_n << 3) |
					(apu_flag_t << 2) |
					(apu_flag_2 << 1) |
					(apu_flag_1));

				apu_flag_f = 0x0000;
				
				return val;
				
				break;
			}
			case 0x16: // controller 1
			{
				if (ctl_flag_s == 0x0000)
				{
					unsigned char val = ((ctl_latch_1 & 0x01) | 0x40);
					
					ctl_latch_1 = (ctl_latch_1 >> 1);
					
					return val;
				}
				else
				{
					return (ctl_value_1 & 0x01);
				}
				
				break;
			}
			case 0x17: // controller 2
			{
				if (ctl_flag_s == 0x0000)
				{
					unsigned char val = ((ctl_latch_2 & 0x01) | 0x40);
					
					ctl_latch_2 = (ctl_latch_2 >> 1);
					
					return val;
				}
				else
				{
					return (ctl_value_2 & 0x01);
				}
				
				break;
			}
			default:
			{
				return 0xFF;
				break;
			}
		}
	}
	else if (addr < 0x00004020) // disabled apu and i/o
	{
		return 0xFF; 
	}
	else if (addr < 0x00006000) // unmapped
	{
		return (unsigned char)(addr >> 8); 
	}
	else if (addr < 0x00008000) // cart ram
	{	
		//if (cpu_status_r > 0)
		//{
			if (map_number == 0x0001)
			{
				if (map_mmc1_ram == 0)
				{
					return nes_read_prg_ram(addr-0x6000);
				}
				else return 0xFF;
			}
			else if (map_number == 0x0004)
			{
				if (map_mmc3_ram == 1)
				{
					return nes_read_prg_ram(addr-0x6000);
				}
				else return 0xFF;
			}
			else
			{
				return nes_read_prg_ram(addr-0x6000);
			}
		//}
		//else return 0xFF;
	}
	else if (addr < 0x0000C000) // cart rom (lower half)
	{
		if (map_number == 0x0001) // mmc1
		{
			if ((unsigned char)cart_header[4] <= 0x10) // 256KB or less
			{
				if (map_mmc1_prg_mode == 0 || map_mmc1_prg_mode == 1)
				{
					return nes_read_cart_rom((prg_offset+0x8000*((map_mmc1_prg_bank&0x0E)>>1)+addr-0x8000));
				}
				else if (map_mmc1_prg_mode == 2)
				{
					return nes_read_cart_rom((prg_offset+addr-0x8000));
				}
				else
				{
					return nes_read_cart_rom((prg_offset+0x4000*map_mmc1_prg_bank+addr-0x8000));
				}
			}
			else // up to 512KB
			{
				if (map_mmc1_prg_mode == 0 || map_mmc1_prg_mode == 1)
				{
					return nes_read_cart_rom((prg_offset+0x00040000*((map_mmc1_chr_bank_0&0x10)>>4)+0x8000*((map_mmc1_prg_bank&0x0E)>>1)+addr-0x8000));
				}
				else if (map_mmc1_prg_mode == 2)
				{
					return nes_read_cart_rom((prg_offset+0x00040000*((map_mmc1_chr_bank_0&0x10)>>4)+addr-0x8000));
				}
				else
				{
					return nes_read_cart_rom((prg_offset+0x00040000*((map_mmc1_chr_bank_0&0x10)>>4)+0x4000*map_mmc1_prg_bank+addr-0x8000));
				}
			}
		}
		else if (map_number == 0x0002) // unrom
		{
			return nes_read_cart_rom((prg_offset+0x4000*map_unrom_bank+addr-0x8000));
		}
		else if (map_number == 0x0003) // cnrom
		{
			return nes_read_cart_rom((prg_offset+addr-0x8000));
		}
		else if (map_number == 0x0004) // mmc3
		{
			if (addr < 0x0000A000)
			{
				if (map_mmc3_prg_mode == 0x0000)
				{
					return nes_read_cart_rom((prg_offset+0x2000*map_mmc3_bank_r6+addr-0x8000));
				}
				else
				{	
					return cart_bottom[addr-0x8000];
					//return nes_read_cart_rom((prg_offset+0x2000*(((unsigned char)cart_header[4]<<1)-2)+addr-0x8000));
				}
			}
			else
			{
				return nes_read_cart_rom((prg_offset+0x2000*map_mmc3_bank_r7+addr-0xA000));
			}
		}
		else if (map_number == 0x0007) // anrom
		{
			return nes_read_cart_rom((prg_offset+0x8000*map_anrom_bank+addr-0x8000));
		}
		else // nrom
		{
			return nes_read_cart_rom((prg_offset+addr-0x8000));
		}
	}
	else if (addr < 0x00010000) // cart rom (upper half)
	{
		if (map_number == 0x0001) // mmc1
		{
			if ((unsigned char)cart_header[4] <= 0x10) // 256KB or less
			{
				if (map_mmc1_prg_mode == 0 || map_mmc1_prg_mode == 1)
				{
					return nes_read_cart_rom((prg_offset+0x8000*((map_mmc1_prg_bank&0x0E)>>1)+addr-0x8000));
				}
				else if (map_mmc1_prg_mode == 2)
				{
					return nes_read_cart_rom((prg_offset+0x4000*map_mmc1_prg_bank+addr-0xC000));
				}
				else
				{
					return cart_bottom[addr-0xC000];
					//return nes_read_cart_rom((prg_offset+0x4000*((unsigned char)cart_header[4]-1)+addr-0xC000));			
				}
			}
			else // up to 512KB
			{
				if (map_mmc1_prg_mode == 0 || map_mmc1_prg_mode == 1)
				{
					return nes_read_cart_rom((prg_offset+0x00040000*((map_mmc1_chr_bank_0&0x10)>>4)+0x8000*((map_mmc1_prg_bank&0x0E)>>1)+addr-0x8000));
				}
				else if (map_mmc1_prg_mode == 2)
				{
					return nes_read_cart_rom((prg_offset+0x00040000*((map_mmc1_chr_bank_0&0x10)>>4)+0x4000*map_mmc1_prg_bank+addr-0xC000));
				}
				else
				{
					return cart_bottom[addr-0xC000];
					//return nes_read_cart_rom((prg_offset+0x00040000*((map_mmc1_chr_bank_0&0x10)>>4)+0x4000*(((unsigned char)cart_header[4]-0x10)-1)+addr-0xC000));		
				}
			}
		}
		else if (map_number == 0x0002) // unrom
		{
			return cart_bottom[addr-0xC000];
			//return nes_read_cart_rom((prg_offset+0x4000*((unsigned char)cart_header[4]-1)+addr-0xC000));
		}
		else if (map_number == 0x0003) // cnrom
		{
			if ((unsigned char)cart_header[4] == 0x01)
			{
				return cart_bottom[addr-0xC000];
				//return nes_read_cart_rom((prg_offset+addr-0xC000));
			}
			else
			{
				return cart_bottom[addr-0xC000];
				//return nes_read_cart_rom((prg_offset+addr-0x8000));
			}
		}
		else if (map_number == 0x0004) // mmc3
		{
			if (addr < 0x0000E000)
			{
				if (map_mmc3_prg_mode == 0x0000)
				{
					return cart_bottom[addr-0xC000];
					//return nes_read_cart_rom((prg_offset+0x2000*(((unsigned char)cart_header[4]<<1)-2)+addr-0xC000));
				}
				else
				{	
					return nes_read_cart_rom((prg_offset+0x2000*map_mmc3_bank_r6+addr-0xC000));
				}
			}
			else
			{
				return cart_bottom[addr-0xC000];
				//return nes_read_cart_rom((prg_offset+0x2000*(((unsigned char)cart_header[4]<<1)-1)+addr-0xE000));
			}
		}
		else if (map_number == 0x0007) // anrom
		{
			return nes_read_cart_rom((prg_offset+0x8000*map_anrom_bank+addr-0x8000));
		}
		else // nrom
		{
			if ((unsigned char)cart_header[4] == 0x01)
			{
				return cart_bottom[addr-0xC000];
				//return nes_read_cart_rom((prg_offset+addr-0xC000));
			}
			else
			{
				return cart_bottom[addr-0xC000];
				//return nes_read_cart_rom((prg_offset+addr-0x8000));
			}
		}
	}
	else
	{
		return cpu_read((addr & 0x0000FFFF));
	}
	
	return 0xFF;
}

void cpu_write(unsigned long addr, unsigned char val)
{	
	nes_loop_write = 1;
	
	if (addr < 0x00002000)
	{
		nes_write_cpu_ram((addr&0x000007FF), val); // internal ram (and mirrors)
	}
	else if (addr < 0x00004000) // ppu (and mirrors)
	{
		switch ((addr&0x00000007))
		{
			case 0x00: // ppuctrl
			{	
				ppu_flag_e = (val>>7);
				ppu_flag_p = ((val>>6)&0x01);
				ppu_flag_h = ((val>>5)&0x01);
				ppu_flag_b = ((val>>4)&0x01);
				ppu_flag_s = ((val>>3)&0x01);
				ppu_flag_i = ((val>>2)&0x01);
				
				ppu_reg_t = ((ppu_reg_t & 0x73FF) | (((unsigned long)val & 0x0003) << 10));
				
				break;
			}
			case 0x01: // ppumask
			{	
				ppu_flag_g = (val & 0x01);
				ppu_flag_lb = ((val & 0x02) >> 1);
				ppu_flag_ls = ((val & 0x04) >> 2);
				ppu_flag_eb = ((val & 0x08) >> 3);
				ppu_flag_es = ((val & 0x10) >> 4);
				
				break;
			}
			case 0x02: // ppustatus
			{
				ppu_reg_w = 0x0000;
				
				break;
			}
			case 0x03: // oamaddr
			{
				ppu_reg_a = val;
				
				break;
			}
			case 0x04: // oamdata
			{
				nes_write_oam_ram((ppu_reg_a&0x00FF), val);
				ppu_reg_a++;
				
				break;
			}
			case 0x05: // ppuscroll
			{
				if (ppu_reg_w == 0x0000)
				{
					ppu_reg_t = ((ppu_reg_t & 0x7FE0) | (((unsigned long)val & 0x00F8) >> 3));
					ppu_reg_x = ((unsigned long)val & 0x0007);
					ppu_reg_w = 0x0001;
				}
				else
				{
					ppu_reg_t = ((ppu_reg_t & 0x0C1F) | 
						(((unsigned long)val & 0x0007) << 12) |
						(((unsigned long)val & 0x00F8) << 2));
					ppu_reg_w = 0x0000;
				}
				
				break;
			}
			case 0x06: // ppuaddr
			{
				if (ppu_reg_w == 0x0000)
				{
					ppu_reg_t = ((ppu_reg_t & 0x00FF) | (((unsigned long)val & 0x003F) << 8));
					ppu_reg_w = 0x0001;
				}
				else
				{	
					ppu_reg_t = ((ppu_reg_t & 0x7F00) | (unsigned long)val);
					ppu_reg_v = ppu_reg_t;
					ppu_reg_w = 0x0000;
				}

				if (map_number == 0x0004) // mmc3
				{
					nes_mmc3_irq_toggle(((ppu_reg_v & 0x1000) >> 12));
				}
				
				break;
			}
			case 0x07: // ppudata
			{
				if (ppu_reg_v < 0x2000)
				{
					nes_write_chr_ram(ppu_reg_v, val);
				}
				else if (ppu_reg_v >= 0x2000 && ppu_reg_v < 0x3000)
				{	
					if (ppu_reg_v < 0x2400)
					{
						if (ppu_status_m == 0x0000 || ppu_status_m == 0x0001 || ppu_status_m == 0x0002)
						{
							nes_write_ppu_ram(ppu_reg_v-0x2000, val);
						}
						else if (ppu_status_m == 0x0003)
						{
							nes_write_ppu_ram(ppu_reg_v-0x2000+0x0400, val);
						}
					}
					else if (ppu_reg_v < 0x2800)
					{
						if (ppu_status_m == 0x0000 || ppu_status_m == 0x0002) // vertical scrolling
						{
							nes_write_ppu_ram(ppu_reg_v-0x2400, val);
						}
						else if (ppu_status_m == 0x0001 || ppu_status_m == 0x0003) // horizontal scrolling
						{
							nes_write_ppu_ram(ppu_reg_v-0x2000, val);
						}
					}
					else if (ppu_reg_v < 0x2C00)
					{
						if (ppu_status_m == 0x0000 || ppu_status_m == 0x0003) // vertical scrolling
						{
							nes_write_ppu_ram(ppu_reg_v-0x2400, val);
						}
						else if (ppu_status_m == 0x0001 || ppu_status_m == 0x0002) // horizontal scrolling
						{
							nes_write_ppu_ram(ppu_reg_v-0x2800, val);
						}
					}
					else
					{
						if (ppu_status_m == 0x0000 || ppu_status_m == 0x0001 || ppu_status_m == 0x0003)
						{
							nes_write_ppu_ram(ppu_reg_v-0x2800, val);
						}
						else if (ppu_status_m == 0x0002)
						{
							nes_write_ppu_ram(ppu_reg_v-0x2C00, val);
						}
					}
				}
				else if (ppu_reg_v >= 0x3F00 && ppu_reg_v < 0x4000)
				{
					if ((ppu_reg_v&0x001F) == 0x0000 || (ppu_reg_v&0x001F) == 0x0010)
					{
						nes_write_pal_ram(0x00, val);
						nes_write_pal_ram(0x10, val);
					}
					else
					{
						nes_write_pal_ram((ppu_reg_v&0x001F), val);
					}
				}
					
				if (ppu_flag_i == 0x0000)
				{
					ppu_reg_v += 0x0001;
				}
				else
				{
					ppu_reg_v += 0x0020;
				}
				
				break;
			}
		}
	}
	else if (addr < 0x00004018) // apu and i/o
	{
		switch ((addr&0x000000FF))
		{
			case 0x00: // pulse 1 volume
			{
				apu_pulse_1_c = ((val & 0x10) >> 4);
				
				if (apu_pulse_1_c > 0)
				{
					apu_pulse_1_v = (val & 0x0F);
				}
				else
				{
					apu_pulse_1_v = 0x0F;
				}
				
				apu_pulse_1_m = (val & 0x0F);
				
				apu_pulse_1_r = apu_pulse_1_m + 1;
				
				apu_pulse_1_i = ((val & 0x20) >> 5);
				
				apu_pulse_1_d = ((val & 0xC0) >> 6);
				
				break;
			}
			case 0x01: // pulse 1 sweep
			{
				apu_pulse_1_s = (val & 0x07);
				
				apu_pulse_1_n = ((val & 0x08) >> 3);
				
				apu_pulse_1_p = ((val & 0x70) >> 4);
				
				apu_pulse_1_w = apu_pulse_1_p + 1;
				
				apu_pulse_1_e = ((val & 0x80) >> 7);
				
				break;
			}
			case 0x02: // pulse 1 timer
			{
				apu_pulse_1_t = ((apu_pulse_1_t & 0x0700) | val);
				
				apu_pulse_1_k = apu_pulse_1_t;
				
				break;
			}
			case 0x03: // pulse 1 length
			{
				apu_pulse_1_t = ((apu_pulse_1_t & 0x00FF) | ((val & 0x07) << 8));
				
				apu_pulse_1_l = apu_length[(val>>3)];
				
				apu_pulse_1_k = apu_pulse_1_t;
				
				break;
			}
			case 0x04: // pulse 2 volume
			{
				apu_pulse_2_c = ((val & 0x10) >> 4);
				
				if (apu_pulse_2_c > 0)
				{
					apu_pulse_2_v = (val & 0x0F);
				}
				else
				{
					apu_pulse_2_v = 0x0F;
				}
				
				apu_pulse_2_m = (val & 0x0F);
				
				apu_pulse_2_r = apu_pulse_2_m + 1;
				
				apu_pulse_2_i = ((val & 0x20) >> 5);
				
				apu_pulse_2_d = ((val & 0xC0) >> 6);
				
				break;
			}
			case 0x05: // pulse 2 sweep
			{
				apu_pulse_2_s = (val & 0x07);
				
				apu_pulse_2_n = ((val & 0x08) >> 3);
				
				apu_pulse_2_p = ((val & 0x70) >> 4);
				
				apu_pulse_2_w = apu_pulse_2_p + 1;
				
				apu_pulse_2_e = ((val & 0x80) >> 7);

				break;
			}
			case 0x06: // pulse 2 timer
			{
				apu_pulse_2_t = ((apu_pulse_2_t & 0x0700) | val);
				
				apu_pulse_2_k = apu_pulse_2_t;
				
				break;
			}
			case 0x07: // pulse 2 length
			{
				apu_pulse_2_t = ((apu_pulse_2_t & 0x00FF) | ((val & 0x07) << 8));
				
				apu_pulse_2_l = apu_length[(val>>3)];
				
				apu_pulse_2_k = apu_pulse_2_t;
				
				break;
			}
			case 0x08: // triangle linear
			{
				apu_triangle_r = (val & 0x7F);
				
				apu_triangle_v = apu_triangle_r;
				
				apu_triangle_c = ((val & 0x80) >> 7);
				
				break;
			}
			case 0x09: // triangle unused
			{
				break;
			}
			case 0x0A: // triangle timer
			{
				apu_triangle_t = ((apu_triangle_t & 0x0700) | val);
				
				apu_triangle_k = apu_triangle_t;
				
				break;
			}
			case 0x0B: // triangle length
			{
				apu_triangle_t = ((apu_triangle_t & 0x00FF) | ((val & 0x07) << 8));
				
				apu_triangle_l = apu_length[(val>>3)] + 1;
				
				apu_triangle_k = apu_triangle_t;
				
				apu_triangle_f = 0x0001;
				
				break;
			}
			case 0x0C: // noise volume
			{
				apu_noise_c = ((val & 0x10) >> 4);
				
				if (apu_noise_c > 0)
				{
					apu_noise_v = (val & 0x0F);
				}
				else
				{
					apu_noise_v = 0x0F;
				}
				
				apu_noise_m = (val & 0x0F);
				
				apu_noise_r = apu_noise_m + 1;
				
				apu_noise_i = ((val & 0x20) >> 5);
				
				break;
			}
			case 0x0D: // noise unused
			{
				
				break;
			}
			case 0x0E: // noise period
			{
				apu_noise_d = ((val & 0x80) >> 7);
				
				apu_noise_t = apu_period[(val & 0x0F)];
				
				apu_noise_k = apu_noise_t;
				
				break;
			}
			case 0x0F: // noise length
			{
				apu_noise_l = apu_length[(val>>3)];
				
				break;
			}
			case 0x10: // DMC frequency
			{
				apu_dmc_i = (val >> 7);
				apu_dmc_l = ((val >> 6) & 0x01);
				apu_dmc_r = apu_rate[(val & 0x0F)];

				apu_dmc_k = apu_dmc_r;
				
				if (apu_dmc_i == 0) apu_flag_i = 0;
				
				break;
			}
			case 0x11: // DMC load
			{	
				apu_dmc_d = (val & 0x7F);
				
				break;
			}
			case 0x12: // DMC address
			{
				apu_dmc_a = 0xC000 + ((unsigned short)val << 6); 
				
				break;
			}
			case 0x13: // DMC length
			{
				apu_dmc_s = (val << 4) + 1;
				
				break;
			}
			case 0x14: // oam dma
			{
				if ((cpu_current_cycles&0x00000002) == 0x00000000) cpu_dma_cycles = 513; // 513 cycles on even
				else cpu_dma_cycles = 514; // 514 cycles on odd
				
				for (unsigned long loop=0; loop<256; loop++)
				{
					nes_write_oam_ram(loop, cpu_read(loop+((unsigned long)val<<8))); // perhaps read from internal ram directly to make faster?
				}
				
				break;
			}
			case 0x15: // apu status
			{	
				apu_flag_d = ((val>>4)&0x01);
				apu_flag_n = ((val>>3)&0x01);
				apu_flag_t = ((val>>2)&0x01);
				apu_flag_2 = ((val>>1)&0x01);
				apu_flag_1 = (val&0x01);
				
				if (apu_flag_d == 0) apu_dmc_s = 0;
				if (apu_flag_n == 0) apu_noise_l = 0;
				if (apu_flag_t == 0) apu_triangle_l = 0;
				if (apu_flag_2 == 0) apu_pulse_2_l = 0;
				if (apu_flag_1 == 0) apu_pulse_1_l = 0;
				
				apu_flag_i = 0;
				
				break;
			}
			case 0x16: // controller 1
			{
				if (ctl_flag_s > 0 && val == 0x00)
				{
					ctl_latch_1 = ctl_value_1;
					ctl_latch_2 = ctl_value_2;
				}
					
				ctl_flag_s = (val&0x01);
				
				break;
			}
			case 0x17: // apu frame counter
			{
				apu_flag_m = ((val>>7)&0x01);
				apu_flag_b = ((val>>6)&0x01);
				
				if (apu_flag_b > 0) apu_flag_f = 0x0000;
				
				apu_counter_q = 0;
				apu_counter_s = 0;
				
				break;
			}
			default:
			{
				break;
			}
		}
	}
	else if (addr < 0x00004020) // disabled apu and i/o
	{
	} 
	else if (addr < 0x00006000) // unmapped
	{
	}
	else if (addr < 0x00008000) // cart ram
	{
		//if (cpu_status_r > 0)
		//{
			if (map_number == 0x0001)
			{
				if (map_mmc1_ram == 0)
				{
					nes_write_prg_ram(addr-0x6000, val);
				}
			}
			else if (map_number == 0x0004)
			{
				if (map_mmc3_ram == 1)
				{
					nes_write_prg_ram(addr-0x6000, val);
				}
			}
			else
			{
				nes_write_prg_ram(addr-0x6000, val);
			}
		//}
	}
	else if (addr < 0x00010000) // cart rom and mapper
	{
		if (map_number == 0x0001) // mmc1
		{
			if ((val & 0x80) != 0x00)
			{
				map_mmc1_shift = 0x0000;
				map_mmc1_count = 0x0000;
				map_mmc1_prg_mode = 0x0003;
				map_mmc1_chr_mode = 0x0000;
				map_mmc1_chr_bank_0 = 0x0010;
				map_mmc1_chr_bank_1 = 0x0000;
				map_mmc1_prg_bank = 0x0000;
				map_mmc1_ram = 0x0000;
			}
			else if (map_mmc1_ready > 0)
			{
				map_mmc1_ready = 0;
				
				map_mmc1_shift = ((map_mmc1_shift >> 1) | (((unsigned long)val & 0x01) << 4));
				map_mmc1_count = map_mmc1_count + 1;
				
				if (map_mmc1_count >= 5)
				{
					map_mmc1_count = 0;
				
					if (addr < 0x0000A000) // control
					{
						switch ((map_mmc1_shift & 0x03)) // nametable
						{
							case 0x00:
							{
								ppu_status_m = 0x0002; // single, first bank
								
								break;
							}
							case 0x01:
							{
								ppu_status_m = 0x0003; // single, second bank
								
								break;
							}
							case 0x02:
							{
								ppu_status_m = 0x0001; // horizontal scrolling
								
								break;
							}
							case 0x03:
							{	
								ppu_status_m = 0x0000; // vertical scrolling
								
								break;
							}
						}
						
						map_mmc1_prg_mode = ((map_mmc1_shift & 0x0C) >> 2);
						map_mmc1_chr_mode = ((map_mmc1_shift & 0x10) >> 4);
					}
					else if (addr < 0x0000C000) // chr bank 0
					{
						map_mmc1_chr_bank_0 = (map_mmc1_shift & 0x1F);
					}
					else if (addr < 0x0000E000) // chr bank 1
					{
						map_mmc1_chr_bank_1 = (map_mmc1_shift & 0x1F);
					}
					else // prg bank
					{	
						map_mmc1_prg_bank = (map_mmc1_shift & 0x0F);
						map_mmc1_prg_bank = (map_mmc1_prg_bank & ((((unsigned char)cart_header[4]))-1));
						map_mmc1_ram = ((map_mmc1_shift & 0x10) >> 4);
					}
					
					map_mmc1_shift = 0x0000;
				}
			}
		}
		else if (map_number == 0x0002) // unrom
		{
			map_unrom_bank = ((unsigned long)val & 0x1F); // was 0x0F but Alwa's Awakening needs it to be 0x1F
		}
		else if (map_number == 0x0003) // cnrom
		{
			map_cnrom_bank = ((unsigned long)val & 0x03);
		}
		else if (map_number == 0x0004) // mmc3
		{
			if (addr < 0x0000A000) // banks
			{
				if ((addr & 0x00000001) == 0x00000000) // even
				{
					map_mmc3_bank_next = ((unsigned long)val & 0x07);
					map_mmc3_prg_mode = (((unsigned long)val & 0x40) >> 6);
					map_mmc3_chr_mode = (((unsigned long)val & 0x80) >> 7);
				}
				else // odd
				{
					switch (map_mmc3_bank_next)
					{
						case 0x00:
						{
							map_mmc3_bank_r0 = (((unsigned long)val & 0xFE) >> 1);
							break;
						}
						case 0x01:
						{
							map_mmc3_bank_r1 = (((unsigned long)val & 0xFE) >> 1);
							break;
						}
						case 0x02:
						{
							map_mmc3_bank_r2 = (unsigned long)val;
							break;
						}
						case 0x03:
						{
							map_mmc3_bank_r3 = (unsigned long)val;
							break;
						}
						case 0x04:
						{
							map_mmc3_bank_r4 = (unsigned long)val;
							break;
						}
						case 0x05:
						{
							map_mmc3_bank_r5 = (unsigned long)val;
							break;
						}
						case 0x06:
						{
							map_mmc3_bank_r6 = ((unsigned long)val & 0x3F);
							map_mmc3_bank_r6 = (map_mmc3_bank_r6 & ((((unsigned char)cart_header[4])<<1)-1));
							break;
						}
						case 0x07:
						{
							map_mmc3_bank_r7 = ((unsigned long)val & 0x3F);
							map_mmc3_bank_r7 = (map_mmc3_bank_r7 & ((((unsigned char)cart_header[4])<<1)-1));
							break;
						}
					}
				}
			}
			else if (addr < 0x0000C000) // mirroring and ram enable
			{
				if ((addr & 0x00000001) == 0x00000000) // even
				{
					if (((unsigned long)val & 0x01) == 0x00)
					{
						ppu_status_m = 0x0001; // horizontal scrolling
					}
					else
					{
						ppu_status_m = 0x0000; // vertical scrolling
					}
				}
				else // odd
				{
					map_mmc3_ram = (((unsigned long)val & 0x80) >> 7);
				}
			}
			else if (addr < 0x0000E000) // irq values
			{
				if ((addr & 0x00000001) == 0x00000000) // even
				{
					map_mmc3_irq_latch = (unsigned long)val;
				}
				else // odd
				{
					map_mmc3_irq_counter = 0;
					map_mmc3_irq_reload = 1;
				}
			}
			else // irq enable
			{
				if ((addr & 0x00000001) == 0x00000000) // even
				{
					map_mmc3_irq_enable = 0x0000; // disable
				}
				else // odd
				{
					map_mmc3_irq_enable = 0x0001; // enable
				}
			}
		}
		else if (map_number == 0x0007) // anrom
		{
			map_anrom_bank = ((unsigned long)val & 0x07);
			
			if (((unsigned long)val & 0x10) == 0x00)
			{
				ppu_status_m = 0x0002;
			}
			else
			{
				ppu_status_m = 0x0003;
			}
		}
	}
	else
	{
		cpu_write((addr & 0x0000FFFF), val);
	}
}

// cpu addressing modes
#define CPU_IMM { \
	cpu_temp_memory = (unsigned long)cpu_read(cpu_reg_pc++); }

#define CPU_ZPR { \
	cpu_temp_memory = (unsigned long)(cpu_ram[(unsigned long)cpu_read(cpu_reg_pc++)]&0x00FF); }

#define CPU_ZPW { \
	cpu_temp_address = (unsigned long)cpu_read(cpu_reg_pc++); }

#define CPU_ZPM { \
	cpu_temp_address = (unsigned long)cpu_read(cpu_reg_pc++); }

#define CPU_ZPXR { \
	cpu_temp_memory = (unsigned long)(cpu_ram[(((unsigned long)cpu_read(cpu_reg_pc++)+cpu_reg_x)&0x00FF)]&0x00FF); }

#define CPU_ZPXW { \
	cpu_temp_address = (((unsigned long)cpu_read(cpu_reg_pc++)+cpu_reg_x)&0x00FF); }

#define CPU_ZPXM { \
	cpu_temp_address = (((unsigned long)cpu_read(cpu_reg_pc++)+cpu_reg_x)&0x00FF); }

#define CPU_ZPYR { \
	cpu_temp_memory = (unsigned long)(cpu_ram[(((unsigned long)cpu_read(cpu_reg_pc++)+cpu_reg_y)&0x00FF)]&0x00FF); }

#define CPU_ZPYW { \
	cpu_temp_address = (((unsigned long)cpu_read(cpu_reg_pc++)+cpu_reg_y)&0x00FF); }

#define CPU_ABSR { \
	cpu_temp_address = (unsigned long)cpu_read(cpu_reg_pc++); \
	cpu_temp_address += ((unsigned long)cpu_read(cpu_reg_pc++)<<8); \
	cpu_temp_memory = (unsigned long)cpu_read(cpu_temp_address); }

#define CPU_ABSW { \
	cpu_temp_address = (unsigned long)cpu_read(cpu_reg_pc++); \
	cpu_temp_address += ((unsigned long)cpu_read(cpu_reg_pc++)<<8); }
	
#define CPU_ABSM { \
	cpu_temp_address = (unsigned long)cpu_read(cpu_reg_pc++); \
	cpu_temp_address += ((unsigned long)cpu_read(cpu_reg_pc++)<<8); }

#define CPU_ABXR { \
	cpu_temp_address = (unsigned long)cpu_read(cpu_reg_pc++); \
	cpu_temp_cycles += ((cpu_temp_address+cpu_reg_x)>>8); \
	cpu_temp_address += ((unsigned long)cpu_read(cpu_reg_pc++)<<8); \
	cpu_temp_address += cpu_reg_x; \
	cpu_temp_memory = (unsigned long)cpu_read(cpu_temp_address); }

#define CPU_ABXW { \
	cpu_temp_address = (unsigned long)cpu_read(cpu_reg_pc++); \
	cpu_temp_cycles += ((cpu_temp_address+cpu_reg_x)>>8); \
	cpu_temp_address += ((unsigned long)cpu_read(cpu_reg_pc++)<<8); \
	cpu_temp_address += cpu_reg_x; }
	
#define CPU_ABXM { \
	cpu_temp_address = (unsigned long)cpu_read(cpu_reg_pc++); \
	cpu_temp_address += ((unsigned long)cpu_read(cpu_reg_pc++)<<8); \
	cpu_temp_address += cpu_reg_x; }
	
#define CPU_ABYR { \
	cpu_temp_address = (unsigned long)cpu_read(cpu_reg_pc++); \
	cpu_temp_cycles += ((cpu_temp_address+cpu_reg_y)>>8); \
	cpu_temp_address += ((unsigned long)cpu_read(cpu_reg_pc++)<<8); \
	cpu_temp_address += cpu_reg_y; \
	cpu_temp_memory = (unsigned long)cpu_read(cpu_temp_address); }

#define CPU_ABYW { \
	cpu_temp_address = (unsigned long)cpu_read(cpu_reg_pc++); \
	cpu_temp_cycles += ((cpu_temp_address+cpu_reg_y)>>8); \
	cpu_temp_address += ((unsigned long)cpu_read(cpu_reg_pc++)<<8); \
	cpu_temp_address += cpu_reg_y; }
	
#define CPU_INDXR { \
	cpu_temp_memory = (unsigned long)cpu_read(cpu_reg_pc++); \
	cpu_temp_address = (unsigned long)cpu_ram[((cpu_temp_memory+cpu_reg_x)&0x00FF)]+((unsigned long)cpu_ram[((cpu_temp_memory+cpu_reg_x+1)&0x00FF)]<<8); \
	cpu_temp_memory = (unsigned long)cpu_read(cpu_temp_address); }

#define CPU_INDXW { \
	cpu_temp_memory = (unsigned long)cpu_read(cpu_reg_pc++); \
	cpu_temp_address = (unsigned long)cpu_ram[((cpu_temp_memory+cpu_reg_x)&0x00FF)]+((unsigned long)cpu_ram[((cpu_temp_memory+cpu_reg_x+1)&0x00FF)]<<8); }
	
#define CPU_INDYR { \
	cpu_temp_memory = (unsigned long)cpu_read(cpu_reg_pc++); \
	cpu_temp_address = (unsigned long)cpu_ram[cpu_temp_memory]+((unsigned long)cpu_ram[((cpu_temp_memory+1)&0x00FF)]<<8); \
	cpu_temp_cycles += (((cpu_temp_address&0x00FF)+cpu_reg_y)>>8); \
	cpu_temp_address += cpu_reg_y; \
	cpu_temp_memory = (unsigned long)cpu_read(cpu_temp_address); }

#define CPU_INDYW { \
	cpu_temp_memory = (unsigned long)cpu_read(cpu_reg_pc++); \
	cpu_temp_address = (unsigned long)cpu_ram[cpu_temp_memory]+((unsigned long)cpu_ram[((cpu_temp_memory+1)&0x00FF)]<<8); \
	cpu_temp_address += cpu_reg_y; }
	
// instructions
#define CPU_ADC { \
	cpu_temp_result = cpu_reg_a+cpu_temp_memory+cpu_flag_c; \
	cpu_flag_c = (cpu_temp_result>0x00FF); \
	cpu_temp_result = (cpu_temp_result&0x00FF); \
	cpu_flag_z = (cpu_temp_result==0x0000); \
	cpu_flag_v = !(((cpu_reg_a^cpu_temp_result)&0x80)&&((cpu_reg_a^cpu_temp_memory)&0x80)); \
	cpu_flag_n = (cpu_temp_result>>7); \
	cpu_reg_a = cpu_temp_result; }
	
#define CPU_AND { \
	cpu_reg_a = (cpu_reg_a&cpu_temp_memory); \
	cpu_flag_z = (cpu_reg_a==0x0000); \
	cpu_flag_n = (cpu_reg_a>>7); }
	
#define CPU_ASL { \
	cpu_temp_memory = (unsigned long)cpu_read(cpu_temp_address); \
	cpu_flag_c = (cpu_temp_memory>>7); \
	cpu_temp_memory = ((cpu_temp_memory<<1)&0x00FF); \
	cpu_write(cpu_temp_address,(unsigned char)(cpu_temp_memory & 0x00FF)); \
	cpu_flag_z = (cpu_temp_memory==0x0000); \
	cpu_flag_n = (cpu_temp_memory>>7); }
	
#define CPU_BIT { \
	cpu_flag_v = ((cpu_temp_memory>>6)&0x01); \
	cpu_flag_n = (cpu_temp_memory>>7); \
	cpu_temp_memory = (cpu_reg_a&cpu_temp_memory); \
	cpu_flag_z = (cpu_temp_memory == 0x0000); }
	
#define CPU_CMP { \
	cpu_flag_c = (cpu_reg_a>=cpu_temp_memory); \
	cpu_flag_z = (cpu_reg_a==cpu_temp_memory); \
	cpu_temp_memory = ((cpu_reg_a-cpu_temp_memory)&0x00FF); \
	cpu_flag_n = (cpu_temp_memory>>7); }
	
#define CPU_CPX { \
	cpu_flag_c = (cpu_reg_x>=cpu_temp_memory); \
	cpu_flag_z = (cpu_reg_x==cpu_temp_memory); \
	cpu_temp_memory = ((cpu_reg_x-cpu_temp_memory)&0x00FF); \
	cpu_flag_n = (cpu_temp_memory>>7); }
	
#define CPU_CPY { \
	cpu_flag_c = (cpu_reg_y>=cpu_temp_memory); \
	cpu_flag_z = (cpu_reg_y==cpu_temp_memory); \
	cpu_temp_memory = ((cpu_reg_y-cpu_temp_memory)&0x00FF); \
	cpu_flag_n = (cpu_temp_memory>>7); }
	
#define CPU_DEC { \
	cpu_temp_memory = (unsigned long)cpu_read(cpu_temp_address); \
	cpu_temp_memory = ((cpu_temp_memory-1)&0x00FF); \
	cpu_write(cpu_temp_address,(unsigned char)(cpu_temp_memory&0x00FF)); \
	cpu_flag_z = (cpu_temp_memory==0x0000); \
	cpu_flag_n = (cpu_temp_memory>>7); }

#define CPU_EOR { \
	cpu_reg_a = (cpu_reg_a^cpu_temp_memory); \
	cpu_flag_z = (cpu_reg_a==0x0000); \
	cpu_flag_n = (cpu_reg_a>>7); }

#define CPU_INC { \
	cpu_temp_memory = (unsigned long)cpu_read(cpu_temp_address); \
	cpu_temp_memory = ((cpu_temp_memory+1)&0x00FF); \
	cpu_write(cpu_temp_address,(unsigned char)(cpu_temp_memory&0x00FF)); \
	cpu_flag_z = (cpu_temp_memory==0x0000); \
	cpu_flag_n = (cpu_temp_memory>>7); }

#define CPU_LDA { \
	cpu_reg_a = cpu_temp_memory; \
	cpu_flag_z = (cpu_reg_a==0x0000); \
	cpu_flag_n = (cpu_reg_a>>7); }

#define CPU_LDX { \
	cpu_reg_x = cpu_temp_memory; \
	cpu_flag_z = (cpu_reg_x==0x0000); \
	cpu_flag_n = (cpu_reg_x>>7); }

#define CPU_LDY { \
	cpu_reg_y = cpu_temp_memory; \
	cpu_flag_z = (cpu_reg_y==0x0000); \
	cpu_flag_n = (cpu_reg_y>>7); }

#define CPU_LSR { \
	cpu_temp_memory = (unsigned long)cpu_read(cpu_temp_address); \
	cpu_flag_c = (cpu_temp_memory&0x01); \
	cpu_temp_memory = (cpu_temp_memory>>1); \
	cpu_write(cpu_temp_address,(unsigned char)(cpu_temp_memory&0x00FF)); \
	cpu_flag_z = (cpu_temp_memory==0x0000); \
	cpu_flag_n = (cpu_temp_memory>>7); }

#define CPU_ORA { \
	cpu_reg_a = (cpu_reg_a|cpu_temp_memory); \
	cpu_flag_z = (cpu_reg_a==0x0000); \
	cpu_flag_n = (cpu_reg_a>>7); }

#define CPU_ROL { \
	cpu_temp_memory = (unsigned long)cpu_read(cpu_temp_address); \
	cpu_temp_memory = (((cpu_temp_memory<<1)&0x01FF)|cpu_flag_c); \
	cpu_flag_c = (cpu_temp_memory>>8); \
	cpu_temp_memory = (cpu_temp_memory&0x00FF); \
	cpu_write(cpu_temp_address,(unsigned char)(cpu_temp_memory&0x00FF)); \
	cpu_flag_z = (cpu_temp_memory==0x0000); \
	cpu_flag_n = (cpu_temp_memory>>7); }

#define CPU_ROR { \
	cpu_temp_memory = (unsigned long)cpu_read(cpu_temp_address); \
	cpu_temp_memory = (cpu_temp_memory|(cpu_flag_c<<8)); \
	cpu_flag_c = (cpu_temp_memory&0x01); \
	cpu_temp_memory = (cpu_temp_memory>>1); \
	cpu_write(cpu_temp_address,(unsigned char)(cpu_temp_memory&0x00FF)); \
	cpu_flag_z = (cpu_temp_memory==0x0000); \
	cpu_flag_n = (cpu_temp_memory>>7); }

#define CPU_SBC { \
	cpu_temp_result = cpu_reg_a-cpu_temp_memory-(0x00001-cpu_flag_c); \
	cpu_flag_c = (0x0001-((cpu_temp_result&0x8000)>>15)); \
	cpu_temp_result = (cpu_temp_result&0x00FF); \
	cpu_flag_z = (cpu_temp_result==0x0000); \
	cpu_flag_v = (((cpu_reg_a^cpu_temp_result)&0x80)&&((cpu_reg_a^cpu_temp_memory)&0x80)); \
	cpu_flag_n = (cpu_temp_result>>7); \
	cpu_reg_a = cpu_temp_result; }

#define CPU_STA { \
	cpu_write(cpu_temp_address, (unsigned char)(cpu_reg_a&0x00FF)); }

#define CPU_STX { \
	cpu_write(cpu_temp_address, (unsigned char)(cpu_reg_x&0x00FF)); }

#define CPU_STY { \
	cpu_write(cpu_temp_address, (unsigned char)(cpu_reg_y&0x00FF)); }

// internal functions
#define CPU_BRA { \
	nes_loop_branch = 1; \
	cpu_temp_address = cpu_reg_pc; \
	if (cpu_temp_memory > 127) cpu_reg_pc = (unsigned long)((cpu_reg_pc + cpu_temp_memory - 256) & 0x0000FFFF); \
	else cpu_reg_pc = (unsigned long)((cpu_reg_pc + cpu_temp_memory) & 0x0000FFFF); \
	cpu_temp_cycles += ((cpu_temp_address&0xFF00)!=(cpu_reg_pc&0xFF00)); }

#define CPU_PUSH { \
	cpu_ram[0x0100+(cpu_reg_s&0x00FF)]=cpu_temp_memory; \
	cpu_reg_s=((cpu_reg_s-1)&0x00FF); }

#define CPU_PULL { \
	cpu_reg_s=((cpu_reg_s+1)&0x00FF); \
	cpu_temp_memory=cpu_ram[(0x0100+(cpu_reg_s&0x00FF))]; }


void nes_irq()
{
	//SendString("IRQ \\");
	//SendLongHex(cpu_reg_pc);
	//SendString("\n\r\\");

	//printf("IRQ %04X %02X\n", (unsigned int)cpu_reg_pc, (unsigned int)ppu_scanline_count);

	nes_loop_halt = 0; // turn CPU back on
	
	cpu_flag_b = 0;
			
	cpu_temp_memory = ((cpu_reg_pc)>>8);
	CPU_PUSH;
	cpu_temp_memory = ((cpu_reg_pc)&0x00FF);
	CPU_PUSH;
	cpu_temp_memory = ((cpu_flag_n<<7)|(cpu_flag_v<<6)|(0x20)|(cpu_flag_b<<4)|
		(cpu_flag_d<<3)|(cpu_flag_i<<2)|(cpu_flag_z<<1)|cpu_flag_c);
	CPU_PUSH;
	cpu_reg_pc = (unsigned long)cpu_read(0xFFFE);
	cpu_reg_pc += ((unsigned long)cpu_read(0xFFFF)<<8);

	cpu_current_cycles += 7;
	
	cpu_flag_i = 1;
}

void nes_nmi()
{	
	//SendString("NMI \\");
	//SendLongHex(cpu_reg_pc);
	//SendString("\n\r\\");

	//printf("NMI %04X %02X\n", (unsigned int)cpu_reg_pc, (unsigned int)ppu_scanline_count);

	nes_loop_halt = 0; // turn CPU back on
	
	cpu_flag_b = 0;

	cpu_temp_memory = ((cpu_reg_pc)>>8);
	CPU_PUSH;
	cpu_temp_memory = ((cpu_reg_pc)&0x00FF);
	CPU_PUSH;
	cpu_temp_memory = ((cpu_flag_n<<7)|(cpu_flag_v<<6)|(0x20)|(cpu_flag_b<<4)|
		(cpu_flag_d<<3)|(cpu_flag_i<<2)|(cpu_flag_z<<1)|cpu_flag_c);
	CPU_PUSH;
	cpu_reg_pc = (unsigned long)cpu_read(0xFFFA);
	cpu_reg_pc += ((unsigned long)cpu_read(0xFFFB)<<8);

	ppu_frame_cycles += 7;
}

void nes_brk()
{
	//SendString("BRK \\");
	//SendLongHex(cpu_reg_pc);
	//SendString("\n\r\\");

	//printf("BRK %04X %d\n", (unsigned int)cpu_reg_pc, (signed int)ppu_scanline_count);

	nes_loop_halt = 0; // turn CPU back on
	
	cpu_flag_b = 1;

	cpu_reg_pc += 1; // add one to PC
	cpu_temp_memory = ((cpu_reg_pc)>>8);
	CPU_PUSH;
	cpu_temp_memory = ((cpu_reg_pc)&0x00FF);
	CPU_PUSH;
	cpu_temp_memory = ((cpu_flag_n<<7)|(cpu_flag_v<<6)|(0x20)|(cpu_flag_b<<4)|
		(cpu_flag_d<<3)|(cpu_flag_i<<2)|(cpu_flag_z<<1)|cpu_flag_c);
	CPU_PUSH;
	cpu_reg_pc = (unsigned long)cpu_read(0xFFFE);
	cpu_reg_pc += ((unsigned long)cpu_read(0xFFFF)<<8);

	cpu_flag_i = 1;
}

unsigned long cpu_run()
{	
	cpu_temp_opcode = (unsigned long)cpu_read(cpu_reg_pc++);

	switch (cpu_temp_opcode)
	{
		// ADC
		case 0x69: { cpu_temp_cycles = 2; CPU_IMM; CPU_ADC; break; }
		case 0x65: { cpu_temp_cycles = 3; CPU_ZPR; CPU_ADC; break; }
		case 0x75: { cpu_temp_cycles = 4; CPU_ZPXR; CPU_ADC; break; }
		case 0x6D: { cpu_temp_cycles = 4; CPU_ABSR; CPU_ADC; break; }
		case 0x7D: { cpu_temp_cycles = 4; CPU_ABXR; CPU_ADC; break; }
		case 0x79: { cpu_temp_cycles = 4; CPU_ABYR; CPU_ADC; break; }
		case 0x61: { cpu_temp_cycles = 6; CPU_INDXR; CPU_ADC; break; }
		case 0x71: { cpu_temp_cycles = 5; CPU_INDYR; CPU_ADC; break; }
		
		// AND
		case 0x29: { cpu_temp_cycles = 2; CPU_IMM; CPU_AND; break; }
		case 0x25: { cpu_temp_cycles = 3; CPU_ZPR; CPU_AND; break; }
		case 0x35: { cpu_temp_cycles = 4; CPU_ZPXR; CPU_AND; break; }
		case 0x2D: { cpu_temp_cycles = 4; CPU_ABSR; CPU_AND; break; }
		case 0x3D: { cpu_temp_cycles = 4; CPU_ABXR; CPU_AND; break; }
		case 0x39: { cpu_temp_cycles = 4; CPU_ABYR; CPU_AND; break; }
		case 0x21: { cpu_temp_cycles = 6; CPU_INDXR; CPU_AND; break; }
		case 0x31: { cpu_temp_cycles = 5; CPU_INDYR; CPU_AND; break; }
		
		// ASL
		case 0x0A:
		{
			cpu_temp_cycles = 0x0002;
			cpu_flag_c = (cpu_reg_a>>7);
			cpu_reg_a = ((cpu_reg_a<<1)&0x00FF);
			cpu_flag_z = (cpu_reg_a==0x0000);
			cpu_flag_n = (cpu_reg_a>>7);
			break;
		}
		case 0x06: { cpu_temp_cycles = 5; CPU_ZPM; CPU_ASL; break; }
		case 0x16: { cpu_temp_cycles = 6; CPU_ZPXM; CPU_ASL; break; }
		case 0x0E: { cpu_temp_cycles = 6; CPU_ABSM; CPU_ASL; break; }
		case 0x1E: { cpu_temp_cycles = 7; CPU_ABXM; CPU_ASL; break; }
		
		// BCC
		case 0x90: { cpu_temp_cycles = 2; CPU_IMM; 
			if (cpu_flag_c == 0x0000) { cpu_temp_cycles = 3; CPU_BRA; } break; }
		// BCS
		case 0xB0: { cpu_temp_cycles = 2; CPU_IMM; 
			if (cpu_flag_c != 0x0000) { cpu_temp_cycles = 3; CPU_BRA; } break; }
		// BEQ
		case 0xF0: { cpu_temp_cycles = 2; CPU_IMM; 
			if (cpu_flag_z != 0x0000) { cpu_temp_cycles = 3; CPU_BRA; } break; }
		
		// BIT
		case 0x24: { cpu_temp_cycles = 3; CPU_ZPR; CPU_BIT; break; }
		case 0x2C: { cpu_temp_cycles = 4; CPU_ABSR; CPU_BIT; break; }
	
		// BMI
		case 0x30: { cpu_temp_cycles = 2; CPU_IMM; 
			if (cpu_flag_n != 0x0000) { cpu_temp_cycles = 3; CPU_BRA; } break; }
		// BNE
		case 0xD0: { cpu_temp_cycles = 2; CPU_IMM; 
			if (cpu_flag_z == 0x0000) { cpu_temp_cycles = 3; CPU_BRA; } break; }
		// BPL
		case 0x10: { cpu_temp_cycles = 2; CPU_IMM; 
			if (cpu_flag_n == 0x0000) { cpu_temp_cycles = 3; CPU_BRA; } break; }
		
		// BRK
		case 0x00:
		{
			cpu_temp_cycles = 0x0007;
			nes_brk();
			break;
		}
		
		// BVC
		case 0x50:
		{
			cpu_temp_cycles = 0x0002;
			cpu_temp_memory = (unsigned long)cpu_read(cpu_reg_pc++);
			if (cpu_flag_v == 0x0000) { cpu_temp_cycles = 3; CPU_BRA; }
			break;
		}
		
		// BVS
		case 0x70:
		{
			cpu_temp_cycles = 0x0002;
			cpu_temp_memory = (unsigned long)cpu_read(cpu_reg_pc++);
			if (cpu_flag_v != 0x0000) { cpu_temp_cycles = 3; CPU_BRA; }
			break;
		}
		
		// CLC
		case 0x18:
		{
			cpu_temp_cycles = 0x0002;
			cpu_flag_c = 0x0000;
			break;
		}
		
		// CLD
		case 0xD8:
		{
			cpu_temp_cycles = 0x0002;
			cpu_flag_d = 0x0000;
			break;
		}
		
		// CLI
		case 0x58:
		{
			cpu_temp_cycles = 0x0002;
			cpu_flag_i = 0x0000;
			break;
		}
		
		// CLV
		case 0xB8:
		{
			cpu_temp_cycles = 0x0002;
			cpu_flag_v = 0x0000;
			break;
		}
		
		// CMP
		case 0xC9: { cpu_temp_cycles = 2; CPU_IMM; CPU_CMP; break; }
		case 0xC5: { cpu_temp_cycles = 3; CPU_ZPR; CPU_CMP; break; }
		case 0xD5: { cpu_temp_cycles = 4; CPU_ZPXR; CPU_CMP; break; }
		case 0xCD: { cpu_temp_cycles = 4; CPU_ABSR; CPU_CMP; break; }
		case 0xDD: { cpu_temp_cycles = 4; CPU_ABXR; CPU_CMP; break; }
		case 0xD9: { cpu_temp_cycles = 4; CPU_ABYR; CPU_CMP; break; }
		case 0xC1: { cpu_temp_cycles = 6; CPU_INDXR; CPU_CMP; break; }
		case 0xD1: { cpu_temp_cycles = 5; CPU_INDYR; CPU_CMP; break; }
		
		// CPX
		case 0xE0: { cpu_temp_cycles = 2; CPU_IMM; CPU_CPX; break; }
		case 0xE4: { cpu_temp_cycles = 3; CPU_ZPR; CPU_CPX; break; }
		case 0xEC: { cpu_temp_cycles = 4; CPU_ABSR; CPU_CPX; break; }
		
		// CPY
		case 0xC0: { cpu_temp_cycles = 2; CPU_IMM; CPU_CPY; break; }
		case 0xC4: { cpu_temp_cycles = 3; CPU_ZPR; CPU_CPY; break; }
		case 0xCC: { cpu_temp_cycles = 4; CPU_ABSR; CPU_CPY; break; }
		
		// DEC
		case 0xC6: { cpu_temp_cycles = 5; CPU_ZPM; CPU_DEC; break; }
		case 0xD6: { cpu_temp_cycles = 6; CPU_ZPXM; CPU_DEC; break; }
		case 0xCE: { cpu_temp_cycles = 6; CPU_ABSM; CPU_DEC; break; }
		case 0xDE: { cpu_temp_cycles = 7; CPU_ABXM; CPU_DEC; break; }
		
		// DEX
		case 0xCA:
		{
			cpu_temp_cycles = 0x0002;
			cpu_reg_x = ((cpu_reg_x-1) & 0x00FF);
			cpu_flag_z = (cpu_reg_x == 0x0000);
			cpu_flag_n = (cpu_reg_x >> 7);
			break;
		}
		
		// DEY
		case 0x88:
		{
			cpu_temp_cycles = 0x0002;
			cpu_reg_y = ((cpu_reg_y-1) & 0x00FF);
			cpu_flag_z = (cpu_reg_y == 0x0000);
			cpu_flag_n = (cpu_reg_y >> 7);
			break;
		}
		
		// EOR
		case 0x49: { cpu_temp_cycles = 2; CPU_IMM; CPU_EOR; break; }
		case 0x45: { cpu_temp_cycles = 3; CPU_ZPR; CPU_EOR; break; }
		case 0x55: { cpu_temp_cycles = 4; CPU_ZPXR; CPU_EOR; break; }
		case 0x4D: { cpu_temp_cycles = 4; CPU_ABSR; CPU_EOR; break; }
		case 0x5D: { cpu_temp_cycles = 4; CPU_ABXR; CPU_EOR; break; }
		case 0x59: { cpu_temp_cycles = 4; CPU_ABYR; CPU_EOR; break; }
		case 0x41: { cpu_temp_cycles = 6; CPU_INDXR; CPU_EOR; break; }
		case 0x51: { cpu_temp_cycles = 5; CPU_INDYR; CPU_EOR; break; }
		
		// INC
		case 0xE6: { cpu_temp_cycles = 5; CPU_ZPM; CPU_INC; break; }
		case 0xF6: { cpu_temp_cycles = 6; CPU_ZPXM; CPU_INC; break; }
		case 0xEE: { cpu_temp_cycles = 6; CPU_ABSM; CPU_INC; break; }
		case 0xFE: { cpu_temp_cycles = 7; CPU_ABXM; CPU_INC; break; }
		
		// INX
		case 0xE8:
		{
			cpu_temp_cycles = 0x0002;
			cpu_reg_x = ((cpu_reg_x+1) & 0x00FF);
			cpu_flag_z = (cpu_reg_x == 0x0000);
			cpu_flag_n = (cpu_reg_x >> 7);
			break;
		}
		
		// INY
		case 0xC8:
		{
			cpu_temp_cycles = 0x0002;
			cpu_reg_y = ((cpu_reg_y+1) & 0x00FF);
			cpu_flag_z = (cpu_reg_y == 0x0000);
			cpu_flag_n = (cpu_reg_y >> 7);
			break;
		}
		
		// JMP
		case 0x4C:
		{
			nes_loop_branch = 1;
			cpu_temp_cycles = 0x0003;
			cpu_temp_address = (unsigned long)cpu_read(cpu_reg_pc++);
			cpu_temp_address += ((unsigned long)cpu_read(cpu_reg_pc++)<<8);
			cpu_reg_pc = cpu_temp_address;
			break;
		}
		case 0x6C:
		{
			nes_loop_branch = 1;
			cpu_temp_cycles = 0x0005;
			cpu_temp_address = (unsigned long)cpu_read(cpu_reg_pc++);
			cpu_temp_address += ((unsigned long)cpu_read(cpu_reg_pc++)<<8);
			cpu_temp_memory = (unsigned long)cpu_read(cpu_temp_address);
			cpu_temp_memory += (unsigned long)(cpu_read((cpu_temp_address&0xFF00)|(((cpu_temp_address&0x00FF)+1)&0x00FF))<<8);
			cpu_reg_pc = cpu_temp_memory;
			break;
		}
		
		// JSR
		case 0x20:
		{
			nes_loop_branch = 1;
			cpu_temp_cycles = 0x0006;
			cpu_temp_memory = ((cpu_reg_pc+1)>>8);
			CPU_PUSH;
			cpu_temp_memory = ((cpu_reg_pc+1)&0x00FF);
			CPU_PUSH;
			cpu_temp_address = (unsigned long)cpu_read(cpu_reg_pc++);
			cpu_temp_address += ((unsigned long)cpu_read(cpu_reg_pc)<<8);
			cpu_reg_pc = cpu_temp_address;
			break;
		}
		
		// LDA
		case 0xA9: { cpu_temp_cycles = 2; CPU_IMM; CPU_LDA; break; }
		case 0xA5: { cpu_temp_cycles = 3; CPU_ZPR; CPU_LDA; break; }
		case 0xB5: { cpu_temp_cycles = 4; CPU_ZPXR; CPU_LDA; break; }
		case 0xAD: { cpu_temp_cycles = 4; CPU_ABSR; CPU_LDA; break; }
		case 0xBD: { cpu_temp_cycles = 4; CPU_ABXR; CPU_LDA; break; }
		case 0xB9: { cpu_temp_cycles = 4; CPU_ABYR; CPU_LDA; break; }
		case 0xA1: { cpu_temp_cycles = 6; CPU_INDXR; CPU_LDA; break; }
		case 0xB1: { cpu_temp_cycles = 5; CPU_INDYR; CPU_LDA; break; }
		
		// LDX
		case 0xA2: { cpu_temp_cycles = 2; CPU_IMM; CPU_LDX; break; }
		case 0xA6: { cpu_temp_cycles = 3; CPU_ZPR; CPU_LDX; break; }
		case 0xB6: { cpu_temp_cycles = 4; CPU_ZPYR; CPU_LDX; break; }
		case 0xAE: { cpu_temp_cycles = 4; CPU_ABSR; CPU_LDX; break; }
		case 0xBE: { cpu_temp_cycles = 4; CPU_ABYR; CPU_LDX; break; }
		
		// LDY
		case 0xA0: { cpu_temp_cycles = 2; CPU_IMM; CPU_LDY; break; }
		case 0xA4: { cpu_temp_cycles = 3; CPU_ZPR; CPU_LDY; break; }
		case 0xB4: { cpu_temp_cycles = 4; CPU_ZPXR; CPU_LDY; break; }
		case 0xAC: { cpu_temp_cycles = 4; CPU_ABSR; CPU_LDY; break; }
		case 0xBC: { cpu_temp_cycles = 4; CPU_ABXR; CPU_LDY; break; }
		
		// LSR
		case 0x4A:
		{
			cpu_temp_cycles = 0x0002;
			cpu_flag_c = (cpu_reg_a&0x01);
			cpu_reg_a = (cpu_reg_a>>1);
			cpu_flag_z = (cpu_reg_a==0x0000);
			cpu_flag_n = (cpu_reg_a>>7);
			break;
		}
		case 0x46: { cpu_temp_cycles = 5; CPU_ZPM; CPU_LSR; break; }
		case 0x56: { cpu_temp_cycles = 6; CPU_ZPXM; CPU_LSR; break; }
		case 0x4E: { cpu_temp_cycles = 6; CPU_ABSM; CPU_LSR; break; }
		case 0x5E: { cpu_temp_cycles = 7; CPU_ABXM; CPU_LSR; break; }
		
		// NOP
		case 0xEA:
		{
			cpu_temp_cycles = 0x0002;
			break;
		}
		
		// ORA
		case 0x09: { cpu_temp_cycles = 2; CPU_IMM; CPU_ORA; break; }
		case 0x05: { cpu_temp_cycles = 3; CPU_ZPR; CPU_ORA; break; }
		case 0x15: { cpu_temp_cycles = 4; CPU_ZPXR; CPU_ORA; break; }
		case 0x0D: { cpu_temp_cycles = 4; CPU_ABSR; CPU_ORA; break; }
		case 0x1D: { cpu_temp_cycles = 4; CPU_ABXR; CPU_ORA; break; }
		case 0x19: { cpu_temp_cycles = 4; CPU_ABYR; CPU_ORA; break; }
		case 0x01: { cpu_temp_cycles = 6; CPU_INDXR; CPU_ORA; break; }
		case 0x11: { cpu_temp_cycles = 5; CPU_INDYR; CPU_ORA; break; }
		
		// PHA
		case 0x48:
		{
			cpu_temp_cycles = 0x0003;
			cpu_temp_memory = cpu_reg_a;
			CPU_PUSH;
			break;
		}
		
		// PHP
		case 0x08:
		{
			cpu_temp_cycles = 0x0003;
			cpu_flag_b = 1;
			cpu_temp_memory = ((cpu_flag_n<<7)|(cpu_flag_v<<6)|(0x20)|(cpu_flag_b<<4)|
				(cpu_flag_d<<3)|(cpu_flag_i<<2)|(cpu_flag_z<<1)|cpu_flag_c);
			CPU_PUSH;
			break;
		}
		
		// PLA
		case 0x68:
		{
			cpu_temp_cycles = 0x0004;
			CPU_PULL;
			cpu_reg_a = cpu_temp_memory;
			cpu_flag_z = (cpu_reg_a==0);
			cpu_flag_n = (cpu_reg_a>>7);
			break;
		}
		
		// PLP
		case 0x28:
		{
			cpu_temp_cycles = 0x0004;
			CPU_PULL;
			cpu_flag_n = (cpu_temp_memory>>7);
			cpu_flag_v = ((cpu_temp_memory>>6)&0x01);
			cpu_flag_d = ((cpu_temp_memory>>3)&0x01);
			cpu_flag_i = ((cpu_temp_memory>>2)&0x01);
			cpu_flag_z = ((cpu_temp_memory>>1)&0x01);
			cpu_flag_c = (cpu_temp_memory&0x01);
			break;
		}
		
		// ROL
		case 0x2A:
		{
			cpu_temp_cycles = 0x0002;
			cpu_reg_a = (((cpu_reg_a<<1)&0x01FF)|cpu_flag_c);
			cpu_flag_c = (cpu_reg_a>>8);
			cpu_reg_a = (cpu_reg_a&0x00FF);
			cpu_flag_z = (cpu_reg_a==0x0000);
			cpu_flag_n = (cpu_reg_a>>7);
			break;
		}
		case 0x26: { cpu_temp_cycles = 5; CPU_ZPM; CPU_ROL; break; }
		case 0x36: { cpu_temp_cycles = 6; CPU_ZPXM; CPU_ROL; break; }
		case 0x2E: { cpu_temp_cycles = 6; CPU_ABSM; CPU_ROL; break; }
		case 0x3E: { cpu_temp_cycles = 7; CPU_ABXM; CPU_ROL; break; }

		// ROR
		case 0x6A:
		{
			cpu_temp_cycles = 0x0002;
			cpu_reg_a = (cpu_reg_a|(cpu_flag_c<<8));
			cpu_flag_c = (cpu_reg_a&0x01);
			cpu_reg_a = (cpu_reg_a>>1);
			cpu_flag_z = (cpu_reg_a==0x0000);
			cpu_flag_n = (cpu_reg_a>>7);
			break;
		}
		case 0x66: { cpu_temp_cycles = 5; CPU_ZPM; CPU_ROR; break; }
		case 0x76: { cpu_temp_cycles = 6; CPU_ZPXM; CPU_ROR; break; }
		case 0x6E: { cpu_temp_cycles = 6; CPU_ABSM; CPU_ROR; break; }
		case 0x7E: { cpu_temp_cycles = 7; CPU_ABXM; CPU_ROR; break; }	
		
		// RTI
		case 0x40:
		{
			nes_loop_branch = 1;
			cpu_temp_cycles = 0x0006;
			CPU_PULL;
			cpu_flag_n = ((cpu_temp_memory>>7)&0x01);
			cpu_flag_v = ((cpu_temp_memory>>6)&0x01);
			cpu_flag_d = ((cpu_temp_memory>>3)&0x01);
			cpu_flag_i = ((cpu_temp_memory>>2)&0x01);
			cpu_flag_z = ((cpu_temp_memory>>1)&0x01);
			cpu_flag_c = (cpu_temp_memory&0x01);
			CPU_PULL;
			cpu_reg_pc = cpu_temp_memory;
			CPU_PULL;
			cpu_reg_pc += (cpu_temp_memory<<8);	
			
			//SendLongHex(cpu_reg_pc);
			//SendString("RTI\n\r\\");
			
			break;
		}
		
		// RTS
		case 0x60:
		{
			nes_loop_branch = 1;
			cpu_temp_cycles = 0x0006;
			CPU_PULL;
			cpu_reg_pc = cpu_temp_memory;
			CPU_PULL;
			cpu_reg_pc += (cpu_temp_memory<<8)+1;
			break;
		}
		
		// SBC
		case 0xE9: { cpu_temp_cycles = 2; CPU_IMM; CPU_SBC; break; }
		case 0xE5: { cpu_temp_cycles = 3; CPU_ZPR; CPU_SBC; break; }
		case 0xF5: { cpu_temp_cycles = 4; CPU_ZPXR; CPU_SBC; break; }
		case 0xED: { cpu_temp_cycles = 4; CPU_ABSR; CPU_SBC; break; }
		case 0xFD: { cpu_temp_cycles = 4; CPU_ABXR; CPU_SBC; break; }
		case 0xF9: { cpu_temp_cycles = 4; CPU_ABYR; CPU_SBC; break; }
		case 0xE1: { cpu_temp_cycles = 6; CPU_INDXR; CPU_SBC; break; }
		case 0xF1: { cpu_temp_cycles = 5; CPU_INDYR; CPU_SBC; break; }
		
		// SEC
		case 0x38:
		{
			cpu_temp_cycles = 0x0002;
			cpu_flag_c = 0x0001;
			break;
		}
		
		// SED
		case 0xF8:
		{
			cpu_temp_cycles = 0x0002;
			cpu_flag_d = 0x0001;
			break;
		}
		
		// SEI
		case 0x78:
		{
			cpu_temp_cycles = 0x0002;
			cpu_flag_i = 0x0001;
			break;
		}
		
		// STA
		case 0x85: { CPU_ZPW; CPU_STA; cpu_temp_cycles = 3; break; }
		case 0x95: { CPU_ZPXW; CPU_STA; cpu_temp_cycles = 4; break; }
		case 0x8D: { CPU_ABSW; CPU_STA; cpu_temp_cycles = 4; break; }
		case 0x9D: { CPU_ABXW; CPU_STA; cpu_temp_cycles = 5; break; }
		case 0x99: { CPU_ABYW; CPU_STA; cpu_temp_cycles = 5; break; }
		case 0x81: { CPU_INDXW; CPU_STA; cpu_temp_cycles = 6; break; }
		case 0x91: { CPU_INDYW; CPU_STA; cpu_temp_cycles = 6; break; }
		
		// STX
		case 0x86: { CPU_ZPW; CPU_STX; cpu_temp_cycles = 3; break; }
		case 0x96: { CPU_ZPYW; CPU_STX; cpu_temp_cycles = 4; break; }
		case 0x8E: { CPU_ABSW; CPU_STX; cpu_temp_cycles = 4; break; }
		
		// STY
		case 0x84: { CPU_ZPW; CPU_STY; cpu_temp_cycles = 3; break; }
		case 0x94: { CPU_ZPXW; CPU_STY; cpu_temp_cycles = 4; break; }
		case 0x8C: { CPU_ABSW; CPU_STY; cpu_temp_cycles = 4; break; }
		
		// TAX
		case 0xAA:
		{
			cpu_temp_cycles = 0x0002;
			cpu_reg_x = cpu_reg_a;
			cpu_flag_z = (cpu_reg_x==0);
			cpu_flag_n = (cpu_reg_x>>7);
			break;
		}
		
		// TAY
		case 0xA8:
		{
			cpu_temp_cycles = 0x0002;
			cpu_reg_y = cpu_reg_a;
			cpu_flag_z = (cpu_reg_y==0);
			cpu_flag_n = (cpu_reg_y>>7);
			break;
		}
		
		// TSX
		case 0xBA:
		{
			cpu_temp_cycles = 0x0002;
			cpu_reg_x = cpu_reg_s;
			cpu_flag_z = (cpu_reg_x==0);
			cpu_flag_n = (cpu_reg_x>>7);
			break;
		}
		
		// TXA
		case 0x8A:
		{
			cpu_temp_cycles = 0x0002;
			cpu_reg_a = cpu_reg_x;
			cpu_flag_z = (cpu_reg_a==0);
			cpu_flag_n = (cpu_reg_a>>7);
			break;
		}
		
		// TXS
		case 0x9A:
		{
			cpu_temp_cycles = 0x0002;
			cpu_reg_s = cpu_reg_x;
			cpu_flag_z = (cpu_reg_s==0);
			cpu_flag_n = (cpu_reg_s>>7);
			break;
		}
		
		// TYA
		case 0x98:
		{
			cpu_temp_cycles = 0x0002;
			cpu_reg_a = cpu_reg_y;
			cpu_flag_z = (cpu_reg_a==0);
			cpu_flag_n = (cpu_reg_a>>7);
			break;
		}
		
		default:
		{
			cpu_temp_cycles = 0x0000;
		}
	}
	
	return cpu_temp_cycles;
}

void nes_border()
{	
	if (screen_handheld == 0)
	{
		//screen_fill((unsigned short)nes_palette_single[(pal_ram[0x00]&0x3F)], 1);
		
		unsigned short pixel_color = nes_palette[(pal_ram[0x00]&0x3F)];
		
		if (nes_border_shrink > 0)
		{
			for (unsigned short y=16; y<224; y++) // remove overscan
			{
				for (unsigned short x=0; x<256; x++)
				{
					nes_pixel_hdmi_raw(x, y, pixel_color); // background color
				}
			}
		}
		else
		{
			//for (unsigned short y=8; y<232; y++) // remove overscan
			for (unsigned short y=0; y<240; y++)
			{
				for (unsigned short x=0; x<256; x++)
				{
					nes_pixel_hdmi_raw(x, y, pixel_color); // background color
				}
			}
		}
	}
	else
	{
		//screen_fill((unsigned short)nes_palette_double[(pal_ram[0x00]&0x3F)], 2);
		
		unsigned short pixel_color = nes_palette[(pal_ram[0x00]&0x3F)];
		
		if (nes_border_shrink > 0)
		{
			for (unsigned short y=16; y<224; y++) // remove overscan
			{
				for (unsigned short x=0; x<256; x++)
				{
					nes_pixel_lcd_raw(x, y, pixel_color); // background color
				}
			}
		}
		else
		{
			//for (unsigned short y=8; y<232; y++) // remove overscan
			for (unsigned short y=0; y<240; y++)
			{
				for (unsigned short x=0; x<256; x++)
				{
					nes_pixel_lcd_raw(x, y, pixel_color); // background color
				}
			}
		}
	}
}

void nes_horizontal_increment()
{
	if ((ppu_reg_v & 0x001F) == 0x001F)
	{
		ppu_reg_v = (ppu_reg_v & 0x7FE0);

		if ((ppu_reg_v & 0x0400) == 0x0400)
		{
			ppu_reg_v = (ppu_reg_v & 0x7BFF);
		}
		else
		{
			ppu_reg_v = ((ppu_reg_v & 0x7BFF) | 0x0400);
		}
	}
	else
	{
		ppu_reg_v = ((ppu_reg_v & 0x7FE0) | ((ppu_reg_v & 0x001F) + 1));
	}
}

void nes_vertical_increment()
{
	if ((ppu_reg_v & 0x7000) == 0x7000)
	{
		if ((ppu_reg_v & 0x03E0) == 0x03A0)
		{
			ppu_reg_v = (ppu_reg_v & 0x0C1F);			

			if ((ppu_reg_v & 0x0800) == 0x0800)
			{
				ppu_reg_v = (ppu_reg_v & 0x07FF);
			}
			else
			{
				ppu_reg_v = ((ppu_reg_v & 0x07FF) | 0x0800);
			}
		}
		else if ((ppu_reg_v & 0x03E0) == 0x03E0)
		{
			ppu_reg_v = (ppu_reg_v & 0x0C1F);
		}
		else
		{
			ppu_reg_v = ((ppu_reg_v & 0x0C1F) | (((ppu_reg_v & 0x03E0) + 0x0020) & 0x03E0));
		}
	}
	else
	{
		ppu_reg_v = ((ppu_reg_v & 0x0FFF) | (((ppu_reg_v & 0x7000) + 0x1000) & 0x7000));
	}

	ppu_reg_v = ((ppu_reg_v & 0x7BE0) | (ppu_reg_t & 0x041F));
}

void nes_background(unsigned long tile, unsigned long line)
{
	unsigned long scroll_t = 0, scroll_l = 0;
	
	unsigned long add_t = 0, add_l = 0;
	
	unsigned long pixel_lookup = 0, pixel_table = 0;
	unsigned long pixel_x = 0, pixel_y = 0;
	unsigned char pixel_high = 0, pixel_low = 0;
	unsigned char pixel_color = 0;
	
	if (ppu_flag_eb > 0)
	{
		//if (map_number == 0x0004) // mmc3
		//{
		//	nes_mmc3_irq_toggle(ppu_flag_b);
		//}
		
		if (line >= 0 && line < 240 && tile >= 0 && tile < 33)
		{	
			if (nes_border_shrink >= 1 && (line < 8 || line >= 232)) return;
			
			if (nes_border_shrink >= 2 && (tile < 1 || tile >= 31)) return;
			
			if (nes_border_shrink >= 3 && (line < 16 || line >= 224)) return;
				
			pixel_y = line;

			scroll_t = ((ppu_reg_v & 0x0C00) | 0x03C0 | ((ppu_reg_v & 0x0380)>>4) | ((ppu_reg_v & 0x001C)>>2));

			add_t = ((((ppu_reg_v & 0x0040)>>5) | ((ppu_reg_v & 0x0002)>>1)) << 1);

			scroll_l = (ppu_reg_v & 0x0FFF);
			
			add_l = 0x1000*ppu_flag_b + ((ppu_reg_v & 0x7000)>>12);	
			
			if (ppu_status_m == 0x0001) // horizontal scrolling
			{
				if (scroll_t < 0x0800)
				{
					pixel_table = (ppu_ram[(scroll_t)]>>add_t);
				}
				else
				{
					pixel_table = (ppu_ram[(scroll_t-0x0800)]>>add_t);
				}
			}
			else if (ppu_status_m == 0x0000) // vertical scrolling
			{
				if (scroll_t < 0x0400)
				{
					pixel_table = (ppu_ram[(scroll_t)]>>add_t);
				}
				else if (scroll_t < 0x0800)
				{
					pixel_table = (ppu_ram[(scroll_t-0x0400)]>>add_t);
				}
				else if (scroll_t < 0x0C00)
				{
					pixel_table = (ppu_ram[(scroll_t-0x0400)]>>add_t);
				}
				else
				{
					pixel_table = (ppu_ram[(scroll_t-0x0800)]>>add_t);
				}
			}
			else if (ppu_status_m == 0x0002) // single, first bank
			{
				pixel_table = (ppu_ram[((scroll_t&0x03FF))]>>add_t);
			}
			else if (ppu_status_m == 0x0003) // single, second bank
			{
				pixel_table = (ppu_ram[((scroll_t&0x03FF)+0x0400)]>>add_t);
			}
			
			if (ppu_status_m == 0x0001) // horizontal scrolling
			{
				if (scroll_l < 0x0800)
				{
					pixel_lookup = (ppu_ram[(scroll_l)]<<4)+add_l;
				}
				else
				{
					pixel_lookup = (ppu_ram[(scroll_l-0x0800)]<<4)+add_l;
				}
			}
			else if (ppu_status_m == 0x0000) // vertical scrolling
			{
				if (scroll_l < 0x0400)
				{
					pixel_lookup = (ppu_ram[(scroll_l)]<<4)+add_l;
				}
				else if (scroll_l < 0x0800)
				{
					pixel_lookup = (ppu_ram[(scroll_l-0x0400)]<<4)+add_l;
				}
				else if (scroll_l < 0x0C00)
				{
					pixel_lookup = (ppu_ram[(scroll_l-0x0400)]<<4)+add_l;
				}
				else
				{
					pixel_lookup = (ppu_ram[(scroll_l-0x0800)]<<4)+add_l;
				}
			}
			else if (ppu_status_m == 0x0002) // single, first bank
			{
				pixel_lookup = (ppu_ram[((scroll_l&0x03FF))]<<4)+add_l;
			}
			else if (ppu_status_m == 0x0003) // single, second bank
			{
				pixel_lookup = (ppu_ram[((scroll_l&0x03FF)+0x0400)]<<4)+add_l;
			}
			
			if ((unsigned char)cart_header[5] > 0)
			{
				if (map_number == 1) // mmc1
				{
					if (map_mmc1_chr_mode == 0) // 8KB
					{
						if ((unsigned char)cart_header[4] <= 0x10) // 256KB or less
						{
							pixel_lookup += 0x00001000*(map_mmc1_chr_bank_0&0x1E);
						}
						else
						{
							
						}
					}
					else if (map_mmc1_chr_mode == 1) // 4KB banked
					{
						if ((unsigned char)cart_header[4] <= 0x10) // 256KB or less
						{
							if (pixel_lookup < 0x1000)
							{
								pixel_lookup += 0x00001000*(map_mmc1_chr_bank_0);
							}
							else
							{
								pixel_lookup += 0x00001000*(map_mmc1_chr_bank_1);
								pixel_lookup -= 0x00001000;
							}
						}
						else
						{

						}
					}
				}
				else if (map_number == 3) // cnrom
				{
					pixel_lookup += 0x2000*map_cnrom_bank;
				}
				else if (map_number == 4) // mmc3
				{
					if (map_mmc3_chr_mode == 0x0000)
					{
						switch ((pixel_lookup&0xFC00))
						{
							case 0x0000:
							{
								pixel_lookup += 0x00000800*map_mmc3_bank_r0;
								break;
							}
							case 0x0400:
							{
								pixel_lookup += 0x00000800*map_mmc3_bank_r0;
								break;
							}
							case 0x0800:
							{
								pixel_lookup += 0x00000800*map_mmc3_bank_r1;
								pixel_lookup -= 0x0800;
								break;
							}
							case 0x0C00:
							{
								pixel_lookup += 0x00000800*map_mmc3_bank_r1;
								pixel_lookup -= 0x0800;
								break;
							}
							case 0x1000:
							{
								pixel_lookup += 0x00000400*map_mmc3_bank_r2;
								pixel_lookup -= 0x1000;
								break;
							}
							case 0x1400:
							{
								pixel_lookup += 0x00000400*map_mmc3_bank_r3;
								pixel_lookup -= 0x1400;
								break;
							}
							case 0x1800:
							{
								pixel_lookup += 0x00000400*map_mmc3_bank_r4;
								pixel_lookup -= 0x1800;
								break;
							}
							case 0x1C00:
							{
								pixel_lookup += 0x00000400*map_mmc3_bank_r5;
								pixel_lookup -= 0x1C00;
								break;
							}
						}
					}
					else
					{
						switch ((pixel_lookup&0xFC00))
						{
							case 0x0000:
							{
								pixel_lookup += 0x00000400*map_mmc3_bank_r2;
								break;
							}
							case 0x0400:
							{
								pixel_lookup += 0x00000400*map_mmc3_bank_r3;
								pixel_lookup -= 0x0400;
								break;
							}
							case 0x0800:
							{
								pixel_lookup += 0x00000400*map_mmc3_bank_r4;
								pixel_lookup -= 0x0800;
								break;
							}
							case 0x0C00:
							{
								pixel_lookup += 0x00000400*map_mmc3_bank_r5;
								pixel_lookup -= 0x0C00;
								break;
							}
							case 0x1000:
							{
								pixel_lookup += 0x00000800*map_mmc3_bank_r0;
								pixel_lookup -= 0x1000;
								break;
							}
							case 0x1400:
							{
								pixel_lookup += 0x00000800*map_mmc3_bank_r0;
								pixel_lookup -= 0x1000;
								break;
							}
							case 0x1800:
							{
								pixel_lookup += 0x00000800*map_mmc3_bank_r1;
								pixel_lookup -= 0x1800;
								break;
							}
							case 0x1C00:
							{
								pixel_lookup += 0x00000800*map_mmc3_bank_r1;
								pixel_lookup -= 0x1800;
								break;
							}
						}
					}
				}
				
				pixel_lookup += chr_offset;

				pixel_low = (unsigned char)cart_rom[(pixel_lookup)];
				pixel_high = (unsigned char)cart_rom[(pixel_lookup+8)];
			}
			else
			{
				pixel_low = chr_ram[(pixel_lookup)];
				pixel_high = chr_ram[(pixel_lookup+8)];
			}

			if (screen_handheld == 0)
			{
				for (unsigned char i=0; i<8; i++)
				{
					pixel_color = ((pixel_high>>6)&0x02)|(pixel_low>>7);

					if (pixel_color != 0x00)
					{
						pixel_x = tile * 8 + i - (ppu_reg_x & 0x07);

						if (pixel_x >= 0 && pixel_x < 256)
						{
							if (ppu_flag_lb > 0 || tile*8+i >= 8) 
							{
								if (ppu_flag_g > 0)
								{
									nes_pixel_hdmi_pal(pixel_x, pixel_y, (pal_ram[(((pixel_table&0x03)<<2)+pixel_color)]&0x30));
								}
								else
								{
									nes_pixel_hdmi_pal(pixel_x, pixel_y, pal_ram[(((pixel_table&0x03)<<2)+pixel_color)]);
								}
							}
						}
					}

					pixel_high = ((pixel_high << 1) & 0xFF);
					pixel_low = ((pixel_low << 1) & 0xFF);
				}
			}
			else
			{
				for (unsigned char i=0; i<8; i++)
				{
					pixel_color = ((pixel_high>>6)&0x02)|(pixel_low>>7);

					if (pixel_color != 0x00)
					{
						pixel_x = tile * 8 + i - (ppu_reg_x & 0x07);

						if (pixel_x >= 0 && pixel_x < 256)
						{
							if (ppu_flag_lb > 0 || tile*8+i >= 8) 
							{
								if (ppu_flag_g > 0)
								{
									nes_pixel_lcd_pal(pixel_x, pixel_y, (pal_ram[(((pixel_table&0x03)<<2)+pixel_color)]&0x30));
								}
								else
								{
									nes_pixel_lcd_pal(pixel_x, pixel_y, pal_ram[(((pixel_table&0x03)<<2)+pixel_color)]);
								}
							}
						}
					}

					pixel_high = ((pixel_high << 1) & 0xFF);
					pixel_low = ((pixel_low << 1) & 0xFF);
				}
			}
		}
	}
}
	
void nes_sprites(unsigned char ground, unsigned long min_y, unsigned long max_y)
{
	unsigned long sprite_x = 0, sprite_y = 0;
	unsigned long sprite_attr = 0, sprite_tile = 0;
	unsigned char sprite_flip_horz = 0, sprite_flip_vert = 0;
	
	unsigned long pixel_x = 0, pixel_y = 0;
	unsigned long pixel_lookup = 0;
	unsigned char pixel_high = 0, pixel_low = 0;
	unsigned char pixel_color = 0;
	
	if (ppu_flag_es > 0)
	{
		for (signed char s=63; s>=0; s--) // must be signed!
		{
			sprite_y = oam_ram[(((s<<2)+0)&0x00FF)];
			
			//if (map_number == 0x0004) // mmc3
			//{
			//	if (ppu_flag_h == 0) // 8x8 sprites
			//	{
			//		nes_mmc3_irq_toggle(ppu_flag_s);
			//	}
			//	else
			//	{
			//		nes_mmc3_irq_toggle(ppu_flag_s); // THIS NEEDS WORK!
			//	}
			//}
			
			if (sprite_y >= min_y && sprite_y < max_y)
			{
				sprite_x = oam_ram[(((s<<2)+3)&0x00FF)];
				
				if (sprite_x < 0xF9 && sprite_y < 0xEF)
				{
					sprite_attr = oam_ram[(((s<<2)+2)&0x00FF)];

					if (((sprite_attr&0x20)>>5) == ground || ground > 1) // foreground/background
					{
						sprite_flip_horz = ((sprite_attr>>6)&0x01);
						sprite_flip_vert = (sprite_attr>>7);

						if (ppu_flag_h == 0) // 8x8 sprites
						{
							sprite_tile = oam_ram[(s<<2)+1];

							for (unsigned char j=0; j<8; j++)
							{
								if ((unsigned char)cart_header[5] > 0)
								{
									pixel_lookup = sprite_tile*16+0x1000*ppu_flag_s+(sprite_flip_vert==0x00?j:7-j);
									
									if (map_number == 1) // mmc1
									{
										if (map_mmc1_chr_mode == 0) // 8KB
										{
											if ((unsigned char)cart_header[4] <= 0x10) // 256KB or less
											{
												pixel_lookup += 0x00001000*(map_mmc1_chr_bank_0&0x1E);
											}
											else
											{
												pixel_lookup += 0x00001000*(map_mmc1_chr_bank_0&0x01);
											}
										}
										else if (map_mmc1_chr_mode == 1) // 4KB banked
										{
											if ((unsigned char)cart_header[4] <= 0x10) // 256KB or less
											{
												if (pixel_lookup < 0x1000)
												{
													pixel_lookup += 0x00001000*(map_mmc1_chr_bank_0);
												}
												else
												{
													pixel_lookup += 0x00001000*(map_mmc1_chr_bank_1);
													pixel_lookup -= 0x00001000;
												}
											}
											else
											{
												if (pixel_lookup < 0x1000)
												{
													pixel_lookup += 0x00001000*(map_mmc1_chr_bank_0&0x01);
												}
												else
												{
													pixel_lookup += 0x00001000*(map_mmc1_chr_bank_1&0x01);
													pixel_lookup -= 0x00001000;
												}
											}
										}
									}
									else if (map_number == 3) // cnrom
									{
										pixel_lookup += 0x2000*map_cnrom_bank;
									}
									else if (map_number == 4) // mmc3
									{
										if (map_mmc3_chr_mode == 0x0000)
										{
											switch ((pixel_lookup&0xFC00))
											{
												case 0x0000:
												{
													pixel_lookup += 0x00000800*map_mmc3_bank_r0;
													break;
												}
												case 0x0400:
												{
													pixel_lookup += 0x00000800*map_mmc3_bank_r0;
													break;
												}
												case 0x0800:
												{
													pixel_lookup += 0x00000800*map_mmc3_bank_r1;
													pixel_lookup -= 0x0800;
													break;
												}
												case 0x0C00:
												{
													pixel_lookup += 0x00000800*map_mmc3_bank_r1;
													pixel_lookup -= 0x0800;
													break;
												}
												case 0x1000:
												{
													pixel_lookup += 0x00000400*map_mmc3_bank_r2;
													pixel_lookup -= 0x1000;
													break;
												}
												case 0x1400:
												{
													pixel_lookup += 0x00000400*map_mmc3_bank_r3;
													pixel_lookup -= 0x1400;
													break;
												}
												case 0x1800:
												{
													pixel_lookup += 0x00000400*map_mmc3_bank_r4;
													pixel_lookup -= 0x1800;
													break;
												}
												case 0x1C00:
												{
													pixel_lookup += 0x00000400*map_mmc3_bank_r5;
													pixel_lookup -= 0x1C00;
													break;
												}
											}
										}
										else
										{
											switch ((pixel_lookup&0xFC00))
											{
												case 0x0000:
												{
													pixel_lookup += 0x00000400*map_mmc3_bank_r2;
													break;
												}
												case 0x0400:
												{
													pixel_lookup += 0x00000400*map_mmc3_bank_r3;
													pixel_lookup -= 0x0400;
													break;
												}
												case 0x0800:
												{
													pixel_lookup += 0x00000400*map_mmc3_bank_r4;
													pixel_lookup -= 0x0800;
													break;
												}
												case 0x0C00:
												{
													pixel_lookup += 0x00000400*map_mmc3_bank_r5;
													pixel_lookup -= 0x0C00;
													break;
												}
												case 0x1000:
												{
													pixel_lookup += 0x00000800*map_mmc3_bank_r0;
													pixel_lookup -= 0x1000;
													break;
												}
												case 0x1400:
												{
													pixel_lookup += 0x00000800*map_mmc3_bank_r0;
													pixel_lookup -= 0x1000;
													break;
												}
												case 0x1800:
												{
													pixel_lookup += 0x00000800*map_mmc3_bank_r1;
													pixel_lookup -= 0x1800;
													break;
												}
												case 0x1C00:
												{
													pixel_lookup += 0x00000800*map_mmc3_bank_r1;
													pixel_lookup -= 0x1800;
													break;
												}
											}
										}
									}
									
									pixel_lookup += chr_offset;

									pixel_low = (unsigned char)cart_rom[(pixel_lookup)];
									pixel_high = (unsigned char)cart_rom[(pixel_lookup+8)];
								}
								else
								{
									pixel_lookup = sprite_tile*16+0x1000*ppu_flag_s+(sprite_flip_vert==0x00?j:7-j);

									pixel_low = chr_ram[(pixel_lookup)];
									pixel_high = chr_ram[(pixel_lookup+8)];
								}

								if (screen_handheld == 0)
								{
									for (unsigned char i=0; i<8; i++)
									{
										if (sprite_flip_horz == 0x00) pixel_color = ((pixel_high>>6)&0x02)|(pixel_low>>7);
										else pixel_color = ((pixel_high<<1)&0x02)|(pixel_low&0x01);

										if (pixel_color != 0x00)
										{
											pixel_x = sprite_x+i;
											pixel_y = sprite_y+j+1;

											if (ppu_flag_ls > 0 || pixel_x >= 8) 
											{
												if (pixel_x >= 0 && pixel_x < 256 && pixel_y >= 0 && pixel_y < 240)
												{
													if (nes_border_shrink >= 1 && (pixel_y < 8 || pixel_y >= 232)) { }
													else if (nes_border_shrink >= 2 && (pixel_x < 8 || pixel_x >= 248)) { }
													else if (nes_border_shrink >= 3 && (pixel_y < 16 || pixel_y >= 224)) { }
													else
													{
														if (ppu_flag_g > 0)
														{
															nes_pixel_hdmi_pal(pixel_x, pixel_y, (pal_ram[(0x0010+((sprite_attr&0x03)<<2)+pixel_color)]&0x30));
														}
														else
														{
															nes_pixel_hdmi_pal(pixel_x, pixel_y, pal_ram[(0x0010+((sprite_attr&0x03)<<2)+pixel_color)]);
														}
													}
												}
											}
										}

										if (sprite_flip_horz == 0x00)
										{
											pixel_high = pixel_high << 1;
											pixel_low = pixel_low << 1;
										}
										else
										{
											pixel_high = pixel_high >> 1;
											pixel_low = pixel_low >> 1;
										}
									}
								}
								else
								{
									for (unsigned char i=0; i<8; i++)
									{
										if (sprite_flip_horz == 0x00) pixel_color = ((pixel_high>>6)&0x02)|(pixel_low>>7);
										else pixel_color = ((pixel_high<<1)&0x02)|(pixel_low&0x01);

										if (pixel_color != 0x00)
										{
											pixel_x = sprite_x+i;
											pixel_y = sprite_y+j+1;

											if (ppu_flag_ls > 0 || pixel_x >= 8) 
											{
												if (pixel_x >= 0 && pixel_x < 256 && pixel_y >= 0 && pixel_y < 240)
												{
													if (nes_border_shrink >= 1 && (pixel_y < 8 || pixel_y >= 232)) { }
													else if (nes_border_shrink >= 2 && (pixel_x < 8 || pixel_x >= 248)) { }
													else if (nes_border_shrink >= 3 && (pixel_y < 16 || pixel_y >= 224)) { }
													else
													{
														if (ppu_flag_g > 0)
														{
															nes_pixel_lcd_pal(pixel_x, pixel_y, (pal_ram[(0x0010+((sprite_attr&0x03)<<2)+pixel_color)]&0x30));
														}
														else
														{
															nes_pixel_lcd_pal(pixel_x, pixel_y, pal_ram[(0x0010+((sprite_attr&0x03)<<2)+pixel_color)]);
														}
													}
												}
											}
										}

										if (sprite_flip_horz == 0x00)
										{
											pixel_high = pixel_high << 1;
											pixel_low = pixel_low << 1;
										}
										else
										{
											pixel_high = pixel_high >> 1;
											pixel_low = pixel_low >> 1;
										}
									}
								}
							}
						}
						else // 8x16 sprites
						{
							sprite_tile = (oam_ram[(s<<2)+1] & 0xFE);

							for (unsigned char j=0; j<16; j++)
							{
								if ((unsigned char)cart_header[5] > 0)
								{
									pixel_lookup = sprite_tile*16+0x1000*(oam_ram[(((s<<2)+1)&0x00FF)]&0x01)+(sprite_flip_vert==0x00?j:15-j);

									if (map_number == 1) // mmc1
									{
										if (map_mmc1_chr_mode == 0) // 8KB
										{
											if ((unsigned char)cart_header[4] <= 0x10) // 256KB or less
											{
												pixel_lookup += 0x00001000*(map_mmc1_chr_bank_0&0x1E);
											}
											else
											{

											}
										}
										else if (map_mmc1_chr_mode == 1) // 4KB banked
										{
											if ((unsigned char)cart_header[4] <= 0x10) // 256KB or less
											{
												if (pixel_lookup < 0x1000)
												{
													pixel_lookup += 0x00001000*(map_mmc1_chr_bank_0);
												}
												else
												{
													pixel_lookup += 0x00001000*(map_mmc1_chr_bank_1);
													pixel_lookup -= 0x00001000;
												}
											}
											else
											{

											}
										}
									}
									else if (map_number == 3) // cnrom
									{
										pixel_lookup += 0x2000*map_cnrom_bank;
									}
									else if (map_number == 4) // mmc3
									{
										if (map_mmc3_chr_mode == 0x0000)
										{
											switch ((pixel_lookup&0xFC00))
											{
												case 0x0000:
												{
													pixel_lookup += 0x00000800*map_mmc3_bank_r0;
													break;
												}
												case 0x0400:
												{
													pixel_lookup += 0x00000800*map_mmc3_bank_r0;
													break;
												}
												case 0x0800:
												{
													pixel_lookup += 0x00000800*map_mmc3_bank_r1;
													pixel_lookup -= 0x0800;
													break;
												}
												case 0x0C00:
												{
													pixel_lookup += 0x00000800*map_mmc3_bank_r1;
													pixel_lookup -= 0x0800;
													break;
												}
												case 0x1000:
												{
													pixel_lookup += 0x00000400*map_mmc3_bank_r2;
													pixel_lookup -= 0x1000;
													break;
												}
												case 0x1400:
												{
													pixel_lookup += 0x00000400*map_mmc3_bank_r3;
													pixel_lookup -= 0x1400;
													break;
												}
												case 0x1800:
												{
													pixel_lookup += 0x00000400*map_mmc3_bank_r4;
													pixel_lookup -= 0x1800;
													break;
												}
												case 0x1C00:
												{
													pixel_lookup += 0x00000400*map_mmc3_bank_r5;
													pixel_lookup -= 0x1C00;
													break;
												}
											}
										}
										else
										{
											switch ((pixel_lookup&0xFC00))
											{
												case 0x0000:
												{
													pixel_lookup += 0x00000400*map_mmc3_bank_r2;
													break;
												}
												case 0x0400:
												{
													pixel_lookup += 0x00000400*map_mmc3_bank_r3;
													pixel_lookup -= 0x0400;
													break;
												}
												case 0x0800:
												{
													pixel_lookup += 0x00000400*map_mmc3_bank_r4;
													pixel_lookup -= 0x0800;
													break;
												}
												case 0x0C00:
												{
													pixel_lookup += 0x00000400*map_mmc3_bank_r5;
													pixel_lookup -= 0x0C00;
													break;
												}
												case 0x1000:
												{
													pixel_lookup += 0x00000800*map_mmc3_bank_r0;
													pixel_lookup -= 0x1000;
													break;
												}
												case 0x1400:
												{
													pixel_lookup += 0x00000800*map_mmc3_bank_r0;
													pixel_lookup -= 0x1000;
													break;
												}
												case 0x1800:
												{
													pixel_lookup += 0x00000800*map_mmc3_bank_r1;
													pixel_lookup -= 0x1800;
													break;
												}
												case 0x1C00:
												{
													pixel_lookup += 0x00000800*map_mmc3_bank_r1;
													pixel_lookup -= 0x1800;
													break;
												}
											}
										}
									}
									
									pixel_lookup += chr_offset;
									
									if ((sprite_flip_vert==0x00?j:15-j) < 8)
									{
										pixel_low = (unsigned char)cart_rom[(pixel_lookup)];
										pixel_high = (unsigned char)cart_rom[(pixel_lookup+8)];
									}
									else
									{
										pixel_low = (unsigned char)cart_rom[(pixel_lookup+8)];
										pixel_high = (unsigned char)cart_rom[(pixel_lookup+16)];
									}
								}
								else
								{
									pixel_lookup = sprite_tile*16+0x1000*(oam_ram[(((s<<2)+1)&0x00FF)]&0x01)+(sprite_flip_vert==0x00?j:15-j);

									if ((sprite_flip_vert==0x00?j:15-j) < 8)
									{
										pixel_low = chr_ram[(pixel_lookup)];
										pixel_high = chr_ram[(pixel_lookup+8)];
									}
									else
									{
										pixel_low = chr_ram[(pixel_lookup+8)];
										pixel_high = chr_ram[(pixel_lookup+16)];
									}
								}

								if (screen_handheld == 0)
								{
									for (unsigned char i=0; i<8; i++)
									{
										if (sprite_flip_horz == 0x00) pixel_color = ((pixel_high>>6)&0x02)|(pixel_low>>7);
										else pixel_color = ((pixel_high<<1)&0x02)|(pixel_low&0x01);

										if (pixel_color != 0x00)
										{
											pixel_x = sprite_x+i;
											pixel_y = sprite_y+j+1;

											if (ppu_flag_ls > 0 || pixel_x >= 8) 
											{									
												if (pixel_x >= 0 && pixel_x < 256 && pixel_y >= 0 && pixel_y < 240)
												{
													if (nes_border_shrink >= 1 && (pixel_y < 8 || pixel_y >= 232)) { }
													else if (nes_border_shrink >= 2 && (pixel_x < 8 || pixel_x >= 248)) { }
													else if (nes_border_shrink >= 3 && (pixel_y < 16 || pixel_y >= 224)) { }
													else
													{
														if (ppu_flag_g > 0)
														{
															nes_pixel_hdmi_pal(pixel_x, pixel_y, (pal_ram[(0x0010+((sprite_attr&0x03)<<2)+pixel_color)]&0x30));
														}
														else
														{
															nes_pixel_hdmi_pal(pixel_x, pixel_y, pal_ram[(0x0010+((sprite_attr&0x03)<<2)+pixel_color)]);
														}
													}
												}
											}
										}

										if (sprite_flip_horz == 0x00)
										{
											pixel_high = ((pixel_high << 1) & 0xFF);
											pixel_low = ((pixel_low << 1) & 0xFF);
										}
										else
										{
											pixel_high = ((pixel_high >> 1) & 0xFF);
											pixel_low = ((pixel_low >> 1) & 0xFF);
										}
									}
								}
								else
								{
									for (unsigned char i=0; i<8; i++)
									{
										if (sprite_flip_horz == 0x00) pixel_color = ((pixel_high>>6)&0x02)|(pixel_low>>7);
										else pixel_color = ((pixel_high<<1)&0x02)|(pixel_low&0x01);

										if (pixel_color != 0x00)
										{
											pixel_x = sprite_x+i;
											pixel_y = sprite_y+j+1;

											if (ppu_flag_ls > 0 || pixel_x >= 8) 
											{
												if (pixel_x >= 0 && pixel_x < 256 && pixel_y >= 0 && pixel_y < 240)
												{
													if (nes_border_shrink >= 1 && (pixel_y < 8 || pixel_y >= 232)) { }
													else if (nes_border_shrink >= 2 && (pixel_x < 8 || pixel_x >= 248)) { }
													else if (nes_border_shrink >= 3 && (pixel_y < 16 || pixel_y >= 224)) { }
													else
													{
														if (ppu_flag_g > 0)
														{
															nes_pixel_lcd_pal(pixel_x, pixel_y, (pal_ram[(0x0010+((sprite_attr&0x03)<<2)+pixel_color)]&0x30));
														}
														else
														{
															nes_pixel_lcd_pal(pixel_x, pixel_y, pal_ram[(0x0010+((sprite_attr&0x03)<<2)+pixel_color)]);
														}
													}
												}
											}
										}

										if (sprite_flip_horz == 0x00)
										{
											pixel_high = ((pixel_high << 1) & 0xFF);
											pixel_low = ((pixel_low << 1) & 0xFF);
										}
										else
										{
											pixel_high = ((pixel_high >> 1) & 0xFF);
											pixel_low = ((pixel_low >> 1) & 0xFF);
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
}

void nes_audio(unsigned long cycles)
{
	apu_counter_q += (cycles);
	
	while (apu_counter_q >= 7446) //7456) // quarter of a frame
	{
		apu_counter_q -= 7446; //7456;
		
		apu_counter_s++;
		
		if (apu_counter_s == 1 || apu_counter_s == 2 || apu_counter_s == 3 ||
			((apu_counter_s == 4 && apu_flag_m == 0) || (apu_counter_s == 5 && apu_flag_m == 1)))
		{
			if (apu_pulse_1_c == 0)
			{
				if (apu_pulse_1_r > 0)
				{
					apu_pulse_1_r--;
				}
				else
				{
					apu_pulse_1_r = apu_pulse_1_m + 1;

					if (apu_pulse_1_v > 0)
					{
						apu_pulse_1_v--;
					}
					else
					{
						if (apu_pulse_1_i > 0) apu_pulse_1_v = 0x0F;
					}
				}
			}

			if (apu_pulse_2_c == 0)
			{
				if (apu_pulse_2_r > 0)
				{
					apu_pulse_2_r--;
				}
				else
				{
					apu_pulse_2_r = apu_pulse_2_m + 1;

					if (apu_pulse_2_v > 0)
					{
						apu_pulse_2_v--;
					}
					else
					{
						if (apu_pulse_2_i > 0) apu_pulse_2_v = 0x0F;
					}
				}
			}

			if (apu_triangle_f == 0)
			{
				if (apu_triangle_v > 0)
				{
					apu_triangle_v--;
				}
			}
			else if (apu_triangle_f == 1)
			{
				apu_triangle_v = apu_triangle_r;
			}
			
			if (apu_noise_c == 0)
			{
				if (apu_noise_r > 0)
				{
					apu_noise_r--;
				}
				else
				{
					apu_noise_r = apu_noise_m + 1;

					if (apu_noise_v > 0)
					{
						apu_noise_v--;
					}
					else
					{
						if (apu_noise_i > 0) apu_noise_v = 0x0F;
					}
				}
			}
		}

		if (apu_counter_s == 2 || 
			((apu_counter_s == 4 && apu_flag_m == 0) || (apu_counter_s == 5 && apu_flag_m == 1)))
		{
			if (apu_pulse_1_i == 0x0000)
			{
				if (apu_pulse_1_l > 0)
				{
					apu_pulse_1_l--;
				}
			}
			
			if (apu_pulse_1_e > 0x0000)
			{
				if (apu_pulse_1_w > 0)
				{
					apu_pulse_1_w--;
				}
				else
				{
					apu_pulse_1_w = apu_pulse_1_p + 1;
					
					apu_pulse_1_a = ((apu_pulse_1_t >> apu_pulse_1_s) & 0x07FF);
					
					if (apu_pulse_1_n > 0)
					{						
						if (apu_pulse_1_t < apu_pulse_1_a)
						{
							apu_pulse_1_t = 0;
						}
						else
						{
							apu_pulse_1_t = ((apu_pulse_1_t - apu_pulse_1_a) & 0x07FF);

							if (apu_pulse_1_t > 0) apu_pulse_1_t--; // only for pulse 1
						}
					}
					else
					{
						apu_pulse_1_t = (apu_pulse_1_t + apu_pulse_1_a);

						if (apu_pulse_1_t > 0x07FF) apu_pulse_1_t = 0;
					}
				}
			}

			if (apu_pulse_2_i == 0x0000)
			{
				if (apu_pulse_2_l > 0)
				{
					apu_pulse_2_l--;
				}
			}
			
			if (apu_pulse_2_e > 0x0000)
			{
				if (apu_pulse_2_w > 0)
				{
					apu_pulse_2_w--;
				}
				else
				{
					apu_pulse_2_w = apu_pulse_2_p + 1;

					apu_pulse_2_a = ((apu_pulse_2_t >> apu_pulse_2_s) & 0x07FF);
					
					if (apu_pulse_2_n > 0)
					{
						if (apu_pulse_2_t < apu_pulse_2_a)
						{
							apu_pulse_2_t = 0;
						}
						else
						{
							apu_pulse_2_t = ((apu_pulse_2_t - apu_pulse_2_a) & 0x07FF);
						}
					}
					else
					{
						apu_pulse_2_t = ((apu_pulse_2_t + apu_pulse_2_a) & 0x07FF);
						
						if (apu_pulse_2_t > 0x07FF) apu_pulse_2_t = 0;
					}
				}
			}

			if (apu_triangle_c == 0x0000)
			{	
				if (apu_triangle_l > 0)
				{
					apu_triangle_l--;
				}
				
				apu_triangle_f = 0;
			}
			
			if (apu_noise_i == 0x0000)
			{
				if (apu_noise_l > 0)
				{
					apu_noise_l--;
				}
			}
		}
		
		if (apu_counter_s == 4 && apu_flag_m == 0 && apu_flag_b == 0) // irq
		{
			apu_flag_f = 1;
		}
		
		if ((apu_counter_s == 4 && apu_flag_m == 0) ||
			(apu_counter_s == 5 && apu_flag_m == 1))
		{
			apu_counter_s = 0;
		}
	}
	
	if (apu_pulse_1_l > 0 && apu_pulse_1_t >= 8 && apu_flag_1 > 0)
	{
		apu_pulse_1_k += (cycles);

		while (apu_pulse_1_k >= (apu_pulse_1_t<<1))
		{
			apu_pulse_1_k -= (apu_pulse_1_t<<1);
			
			apu_pulse_1_o = (unsigned short)(apu_duty[(apu_pulse_1_d<<3)+apu_pulse_1_u] & (apu_pulse_1_v << 4));
			
			apu_pulse_1_u++;
			
			if (apu_pulse_1_u >= 8) apu_pulse_1_u = 0;
		}
	}
	else apu_pulse_1_o = 0x0000;
	
	if (apu_pulse_2_l > 0 && apu_pulse_2_t >= 8 && apu_flag_2 > 0)
	{
		apu_pulse_2_k += (cycles);

		while (apu_pulse_2_k >= (apu_pulse_2_t<<1))
		{
			apu_pulse_2_k -= (apu_pulse_2_t<<1);
			
			apu_pulse_2_o = (unsigned short)(apu_duty[(apu_pulse_2_d<<3)+apu_pulse_2_u] & (apu_pulse_2_v << 4));
			
			apu_pulse_2_u++;
			
			if (apu_pulse_2_u >= 8) apu_pulse_2_u = 0;
		}
	}
	else apu_pulse_2_o = 0x0000;
	
	if (apu_triangle_l > 0 && apu_triangle_r > 0 && apu_triangle_v > 0 && apu_triangle_t > 0 && apu_flag_t > 0)
	{
		apu_triangle_k += (cycles);

		while (apu_triangle_k >= (apu_triangle_t))
		{
			apu_triangle_k -= (apu_triangle_t);
			
			if (apu_triangle_d == 0)
			{
				if (apu_triangle_p > 0x00)
				{
					apu_triangle_p -= 0x11;
				}
				else
				{
					apu_triangle_d = 1;
					
					apu_triangle_p = 0x11;
				}
			}
			else
			{
				if (apu_triangle_p < 0xFF)
				{
					apu_triangle_p += 0x11;
				}
				else
				{
					apu_triangle_d = 0;
					
					apu_triangle_p = 0xEE;
				}
			}
			
			apu_triangle_o = (apu_triangle_p & 0x00FF);
		}
	}
	else apu_triangle_o = 0x0000;
	
	if (apu_noise_l > 0 && apu_flag_n > 0 && apu_noise_t > 0)
	{
		apu_noise_k += (cycles);

		while (apu_noise_k >= (apu_noise_t))
		{
			apu_noise_k -= (apu_noise_t);
			
			if (apu_noise_d == 0)
			{
				apu_noise_x = ((apu_noise_s & 0x0002) >> 1);
			}
			else
			{
				apu_noise_x = ((apu_noise_s & 0x0040) >> 6);
			}
			
			apu_noise_x = ((apu_noise_s & 0x0001) ^ apu_noise_x);
			
			if (apu_noise_x > 0) apu_noise_o = 0x0000;
			else apu_noise_o = (apu_noise_v << 4);
			
			apu_noise_s = (apu_noise_s >> 1);
			
			apu_noise_s = ((apu_noise_s | (apu_noise_x << 14)) & 0x7FFF);
		}
	}
	else apu_noise_o = 0x0000;
	
	if (apu_flag_d > 0 && apu_dmc_r > 0)
	{	
		apu_dmc_k += (cycles);

		while (apu_dmc_k >= (apu_dmc_r))
		{	
			apu_dmc_k -= (apu_dmc_r);
			
			if (apu_dmc_s > 0)
			{
				if (apu_dmc_b == 8)
				{
					apu_dmc_b = 0;
					
					apu_dmc_s--;
					
					if (apu_dmc_s == 0 && apu_dmc_i > 0) // irq
					{
						apu_flag_i = 1;
					}
					
					apu_dmc_t = (unsigned short)cpu_read(apu_dmc_a);
					
					apu_dmc_a++;
				}
			
				apu_dmc_b++;

				if ((apu_dmc_t & 0x01) == 0x00) apu_dmc_d = ((apu_dmc_d + 2) & 0x7F);
				else apu_dmc_d = ((apu_dmc_d - 2) & 0x7F);

				apu_dmc_t = (apu_dmc_t >> 1);
			}
			
			apu_dmc_o = (apu_dmc_d<<1);
		}
	}
	else apu_dmc_o = (apu_dmc_d<<1);
}

void nes_mixer()
{
	apu_mixer_output = 0x0000;
	
	apu_mixer_output += apu_pulse_1_o;	
	apu_mixer_output += apu_pulse_2_o;
	apu_mixer_output += (apu_triangle_o>>1);
	apu_mixer_output += (apu_noise_o>>1);
	apu_mixer_output += (apu_dmc_o);
	apu_mixer_output = (apu_mixer_output>>2); // divide by 4
			
	nes_sound(apu_mixer_output);
}

void nes_sprite_0_calc()
{	
	ppu_status_s = 0xFF;
	ppu_status_d = 0xFF;

	// sprite 0
	if (ppu_flag_h == 0) // 8x8 sprites
	{
		//ppu_status_s = 8;

		for (unsigned long i=0; i<8; i++)
		{
			if (map_number == 1) // mmc1
			{
				if (map_mmc1_chr_mode == 0) // 8KB
				{
					if ((unsigned char)cart_header[5] > 0) // using CHR_ROM
					{
						if ((unsigned char)cart_header[4] <= 0x10) // and 256KB or less
						{
							if ((unsigned char)cart_rom[chr_offset+0x00001000*(map_mmc1_chr_bank_0&0x1E)+oam_ram[1]*16+0x1000*ppu_flag_s+i] != 0x00 ||
								(unsigned char)cart_rom[chr_offset+0x00001000*(map_mmc1_chr_bank_0&0x1E)+oam_ram[1]*16+0x1000*ppu_flag_s+i+8] != 0x00)
							{
								ppu_status_s = i+1;

								break;
							}
						}
						else
						{
							if ((unsigned char)cart_rom[chr_offset+oam_ram[1]*16+0x1000*ppu_flag_s+i] != 0x00 ||
								(unsigned char)cart_rom[chr_offset+oam_ram[1]*16+0x1000*ppu_flag_s+i+8] != 0x00)
							{
								ppu_status_s = i+1;

								break;
							}
						}
					}
					else
					{
						if ((unsigned char)chr_ram[oam_ram[1]*16+0x1000*ppu_flag_s+i] != 0x00 ||
							(unsigned char)chr_ram[oam_ram[1]*16+0x1000*ppu_flag_s+i+8] != 0x00)
						{
							ppu_status_s = i+1;

							break;
						}
					}
				}
				else if (map_mmc1_chr_mode == 1) // 4KB banked
				{
					if ((unsigned char)cart_header[5] > 0) // using CHR_ROM
					{
						if ((unsigned char)cart_header[4] <= 0x10) // and 256KB or less
						{
							if (ppu_flag_s == 0)
							{
								if ((unsigned char)cart_rom[chr_offset+0x00001000*(map_mmc1_chr_bank_0)+oam_ram[1]*16+0x1000*ppu_flag_s+i] != 0x00 ||
									(unsigned char)cart_rom[chr_offset+0x00001000*(map_mmc1_chr_bank_0)+oam_ram[1]*16+0x1000*ppu_flag_s+i+8] != 0x00)
								{
									ppu_status_s = i+1;

									break;
								}
							}
							else
							{
								if ((unsigned char)cart_rom[chr_offset+0x00001000*(map_mmc1_chr_bank_1)+oam_ram[1]*16+0x1000*ppu_flag_s+i] != 0x00 ||
									(unsigned char)cart_rom[chr_offset+0x00001000*(map_mmc1_chr_bank_1)+oam_ram[1]*16+0x1000*ppu_flag_s+i+8] != 0x00)
								{
									ppu_status_s = i+1;

									break;
								}
							}
						}
						else
						{
							if (ppu_flag_s == 0)
							{
								if ((unsigned char)cart_rom[chr_offset+0x00001000*(map_mmc1_chr_bank_0&0x01)+oam_ram[1]*16+0x1000*ppu_flag_s+i] != 0x00 ||
									(unsigned char)cart_rom[chr_offset+0x00001000*(map_mmc1_chr_bank_0&0x01)+oam_ram[1]*16+0x1000*ppu_flag_s+i+8] != 0x00)
								{
									ppu_status_s = i+1;

									break;
								}
							}
							else
							{
								if ((unsigned char)cart_rom[chr_offset+0x00001000*(map_mmc1_chr_bank_1&0x01)+oam_ram[1]*16+0x1000*ppu_flag_s+i] != 0x00 ||
									(unsigned char)cart_rom[chr_offset+0x00001000*(map_mmc1_chr_bank_1&0x01)+oam_ram[1]*16+0x1000*ppu_flag_s+i+8] != 0x00)
								{
									ppu_status_s = i+1;

									break;
								}
							}
						}
					}
					else
					{
						if ((unsigned char)chr_ram[oam_ram[1]*16+0x1000*ppu_flag_s+i] != 0x00 ||
							(unsigned char)chr_ram[oam_ram[1]*16+0x1000*ppu_flag_s+i+8] != 0x00)
						{
							ppu_status_s = i+1;

							break;
						}
					}
				}
			}
			else if (map_number == 2) // unrom
			{
				if ((unsigned char)cart_header[5] > 0)
				{
					if ((unsigned char)cart_rom[chr_offset+oam_ram[1]*16+0x1000*ppu_flag_s+i] != 0x00 ||
						(unsigned char)cart_rom[chr_offset+oam_ram[1]*16+0x1000*ppu_flag_s+i+8] != 0x00)
					{
						ppu_status_s = i+1;

						break;
					}
				}
				else
				{
					if ((unsigned char)chr_ram[oam_ram[1]*16+0x1000*ppu_flag_s+i] != 0x00 ||
						(unsigned char)chr_ram[oam_ram[1]*16+0x1000*ppu_flag_s+i+8] != 0x00)
					{
						ppu_status_s = i+1;

						break;
					}
				}
			}
			else if (map_number == 3) // cnrom
			{
				if ((unsigned char)cart_header[5] > 0)
				{
					if ((unsigned char)cart_rom[chr_offset+0x2000*map_cnrom_bank+oam_ram[1]*16+0x1000*ppu_flag_s+i] != 0x00 ||
						(unsigned char)cart_rom[chr_offset+0x2000*map_cnrom_bank+oam_ram[1]*16+0x1000*ppu_flag_s+i+8] != 0x00)
					{
						ppu_status_s = i+1;

						break;
					}
				}
				else
				{
					if ((unsigned char)chr_ram[oam_ram[1]*16+0x1000*ppu_flag_s+i] != 0x00 ||
						(unsigned char)chr_ram[oam_ram[1]*16+0x1000*ppu_flag_s+i+8] != 0x00)
					{
						ppu_status_s = i+1;

						break;
					}
				}			
			}
			else if (map_number == 4) // mmc3
			{
				ppu_status_s = i+1; // THIS NEEDS WORK!

				break;
			}
			else if (map_number == 7) // anrom
			{
				if ((unsigned char)cart_header[5] > 0)
				{
					if ((unsigned char)cart_rom[chr_offset+oam_ram[1]*16+0x1000*ppu_flag_s+i] != 0x00 ||
						(unsigned char)cart_rom[chr_offset+oam_ram[1]*16+0x1000*ppu_flag_s+i+8] != 0x00)
					{
						ppu_status_s = i+1;

						break;
					}
				}
				else
				{
					if ((unsigned char)chr_ram[oam_ram[1]*16+0x1000*ppu_flag_s+i] != 0x00 ||
						(unsigned char)chr_ram[oam_ram[1]*16+0x1000*ppu_flag_s+i+8] != 0x00)
					{
						ppu_status_s = i+1;

						break;
					}
				}
			}
			else // nrom
			{
				if ((unsigned char)cart_header[5] > 0)
				{
					if ((unsigned char)cart_rom[chr_offset+oam_ram[1]*16+0x1000*ppu_flag_s+i] != 0x00 ||
						(unsigned char)cart_rom[chr_offset+oam_ram[1]*16+0x1000*ppu_flag_s+i+8] != 0x00)
					{
						ppu_status_s = i+1;

						break;
					}
				}
				else
				{
					if ((unsigned char)chr_ram[oam_ram[1]*16+0x1000*ppu_flag_s+i] != 0x00 ||
						(unsigned char)chr_ram[oam_ram[1]*16+0x1000*ppu_flag_s+i+8] != 0x00)
					{
						ppu_status_s = i+1;

						break;
					}
				}
			}
		}
	}
	else // 8x16 sprites
	{
		//ppu_status_s = 16;

		for (unsigned long i=0; i<16; i++)
		{
			if (i < 8)
			{
				if (map_number == 1) // mmc1
				{
					if (map_mmc1_chr_mode == 0) // 8KB
					{
						if ((unsigned char)cart_header[5] > 0) // using CHR_ROM
						{
							if ((unsigned char)cart_header[4] <= 0x10) // and 256KB or less
							{
								if ((unsigned char)cart_rom[chr_offset+0x00001000*(map_mmc1_chr_bank_0&0x1E)+(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i] != 0x00 ||
									(unsigned char)cart_rom[chr_offset+0x00001000*(map_mmc1_chr_bank_0&0x1E)+(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+8] != 0x00)
								{
									ppu_status_s = i+1;

									break;
								}
							}
							else
							{
								if ((unsigned char)cart_rom[chr_offset+(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i] != 0x00 ||
									(unsigned char)cart_rom[chr_offset+(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+8] != 0x00)
								{
									ppu_status_s = i+1;

									break;
								}
							}
						}
						else
						{
							if ((unsigned char)chr_ram[(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i] != 0x00 ||
								(unsigned char)chr_ram[(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+8] != 0x00)
							{
								ppu_status_s = i+1;

								break;
							}
						}
					}
					else if (map_mmc1_chr_mode == 1) // 4KB banked
					{
						if ((unsigned char)cart_header[5] > 0) // using CHR_ROM
						{
							if ((unsigned char)cart_header[4] <= 0x10) // and 256KB or less
							{
								if (ppu_flag_s == 0)
								{
									if ((unsigned char)cart_rom[chr_offset+0x00001000*(map_mmc1_chr_bank_0)+(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i] != 0x00 ||
										(unsigned char)cart_rom[chr_offset+0x00001000*(map_mmc1_chr_bank_0)+(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+8] != 0x00)
									{
										ppu_status_s = i+1;

										break;
									}
								}
								else
								{
									if ((unsigned char)cart_rom[chr_offset+0x00001000*(map_mmc1_chr_bank_1)+(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i] != 0x00 ||
										(unsigned char)cart_rom[chr_offset+0x00001000*(map_mmc1_chr_bank_1)+(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+8] != 0x00)
									{
										ppu_status_s = i+1;

										break;
									}
								}
							}
							else
							{
								if (ppu_flag_s == 0)
								{
									if ((unsigned char)cart_rom[chr_offset+0x00001000*(map_mmc1_chr_bank_0&0x01)+(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i] != 0x00 ||
										(unsigned char)cart_rom[chr_offset+0x00001000*(map_mmc1_chr_bank_0&0x01)+(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+8] != 0x00)
									{
										ppu_status_s = i+1;

										break;
									}
								}
								else
								{
									if ((unsigned char)cart_rom[chr_offset+0x00001000*(map_mmc1_chr_bank_1&0x01)+(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i] != 0x00 ||
										(unsigned char)cart_rom[chr_offset+0x00001000*(map_mmc1_chr_bank_1&0x01)+(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+8] != 0x00)
									{
										ppu_status_s = i+1;

										break;
									}
								}
							}
						}
						else
						{
							if ((unsigned char)chr_ram[(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i] != 0x00 ||
								(unsigned char)chr_ram[(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+8] != 0x00)
							{
								ppu_status_s = i+1;

								break;
							}
						}
					}
				}
				else if (map_number == 2) // unrom
				{
					if ((unsigned char)cart_header[5] > 0)
					{
						if ((unsigned char)cart_rom[chr_offset+(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i] != 0x00 ||
							(unsigned char)cart_rom[chr_offset+(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+8] != 0x00)
						{
							ppu_status_s = i+1;

							break;
						}
					}
					else
					{
						if ((unsigned char)chr_ram[(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i] != 0x00 ||
							(unsigned char)chr_ram[(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+8] != 0x00)
						{
							ppu_status_s = i+1;

							break;
						}
					}
				}
				else if (map_number == 3) // cnrom
				{
					if ((unsigned char)cart_header[5] > 0)
					{
						if ((unsigned char)cart_rom[chr_offset+0x2000*map_cnrom_bank+(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i] != 0x00 ||
							(unsigned char)cart_rom[chr_offset+0x2000*map_cnrom_bank+(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+8] != 0x00)
						{
							ppu_status_s = i+1;

							break;
						}
					}
					else
					{
						if ((unsigned char)cart_rom[(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i] != 0x00 ||
							(unsigned char)cart_rom[(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+8] != 0x00)
						{
							ppu_status_s = i+1;

							break;
						}
					}
				}
				else if (map_number == 4) // mmc3
				{
					ppu_status_s = i+1; // THIS NEEDS WORK!

					break;
				}
				else if (map_number == 7) // anrom
				{
					if ((unsigned char)cart_header[5] > 0)
					{
						if ((unsigned char)cart_rom[chr_offset+(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i] != 0x00 ||
							(unsigned char)cart_rom[chr_offset+(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+8] != 0x00)
						{
							ppu_status_s = i+1;

							break;
						}
					}
					else
					{
						if ((unsigned char)chr_ram[(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i] != 0x00 ||
							(unsigned char)chr_ram[(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+8] != 0x00)
						{
							ppu_status_s = i+1;

							break;
						}
					}
				}
				else // nrom
				{
					if ((unsigned char)cart_header[5] > 0)
					{
						if ((unsigned char)cart_rom[chr_offset+(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i] != 0x00 ||
							(unsigned char)cart_rom[chr_offset+(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+8] != 0x00)
						{
							ppu_status_s = i+1;

							break;
						}
					}
					else
					{
						if ((unsigned char)chr_ram[(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i] != 0x00 ||
							(unsigned char)chr_ram[(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+8] != 0x00)
						{
							ppu_status_s = i+1;

							break;
						}
					}
				}
			}
			else
			{
				if (map_number == 1) // mmc1
				{
					if (map_mmc1_chr_mode == 0) // 8KB
					{
						if ((unsigned char)cart_header[5] > 0) // using CHR_ROM 
						{
							if ((unsigned char)cart_header[4] <= 0x10) // and 256KB or less
							{
								if ((unsigned char)cart_rom[chr_offset+0x00001000*(map_mmc1_chr_bank_0&0x1E)+(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+8] != 0x00 ||
									(unsigned char)cart_rom[chr_offset+0x00001000*(map_mmc1_chr_bank_0&0x1E)+(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+16] != 0x00)
								{
									ppu_status_s = i+1;

									break;
								}
							}
							else
							{
								if ((unsigned char)cart_rom[chr_offset+(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+8] != 0x00 ||
									(unsigned char)cart_rom[chr_offset+(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+16] != 0x00)
								{
									ppu_status_s = i+1;

									break;
								}
							}
						}
						else
						{
							if ((unsigned char)chr_ram[(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+8] != 0x00 ||
								(unsigned char)chr_ram[(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+16] != 0x00)
							{
								ppu_status_s = i+1;

								break;
							}
						}
					}
					else if (map_mmc1_chr_mode == 1) // 4KB banked
					{
						if ((unsigned char)cart_header[5] > 0) // using CHR_ROM and 256KB or less
						{
							if ((unsigned char)cart_header[4] <= 0x10) // and 256KB or less
							{
								if (ppu_flag_s == 0)
								{
									if ((unsigned char)cart_rom[chr_offset+0x00001000*(map_mmc1_chr_bank_0)+(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+8] != 0x00 ||
										(unsigned char)cart_rom[chr_offset+0x00001000*(map_mmc1_chr_bank_0)+(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+16] != 0x00)
									{
										ppu_status_s = i+1;

										break;
									}
								}
								else
								{
									if ((unsigned char)cart_rom[chr_offset+0x00001000*(map_mmc1_chr_bank_1)+(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+8] != 0x00 ||
										(unsigned char)cart_rom[chr_offset+0x00001000*(map_mmc1_chr_bank_1)+(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+16] != 0x00)
									{
										ppu_status_s = i+1;

										break;
									}
								}
							}
							else
							{
								if (ppu_flag_s == 0)
								{
									if ((unsigned char)cart_rom[chr_offset+0x00001000*(map_mmc1_chr_bank_0&0x01)+(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+8] != 0x00 ||
										(unsigned char)cart_rom[chr_offset+0x00001000*(map_mmc1_chr_bank_0&0x01)+(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+16] != 0x00)
									{
										ppu_status_s = i+1;

										break;
									}
								}
								else
								{
									if ((unsigned char)cart_rom[chr_offset+0x00001000*(map_mmc1_chr_bank_1&0x01)+(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+8] != 0x00 ||
										(unsigned char)cart_rom[chr_offset+0x00001000*(map_mmc1_chr_bank_1&0x01)+(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+16] != 0x00)
									{
										ppu_status_s = i+1;

										break;
									}
								}
							}
						}
						else
						{
							if ((unsigned char)chr_ram[(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+8] != 0x00 ||
								(unsigned char)chr_ram[(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+16] != 0x00)
							{
								ppu_status_s = i+1;

								break;
							}
						}
					}
				}
				else if (map_number == 2) // unrom
				{
					if ((unsigned char)cart_header[5] > 0)
					{
						if ((unsigned char)cart_rom[chr_offset+(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+8] != 0x00 ||
							(unsigned char)cart_rom[chr_offset+(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+16] != 0x00)
						{
							ppu_status_s = i+1;

							break;
						}
					}
					else
					{
						if ((unsigned char)chr_ram[(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+8] != 0x00 ||
							(unsigned char)chr_ram[(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+16] != 0x00)
						{
							ppu_status_s = i+1;

							break;
						}
					}
				}
				else if (map_number == 3) // cnrom
				{
					if ((unsigned char)cart_header[5] > 0)
					{
						if ((unsigned char)cart_rom[chr_offset+0x2000*map_cnrom_bank+(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+8] != 0x00 ||
							(unsigned char)cart_rom[chr_offset+0x2000*map_cnrom_bank+(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+16] != 0x00)
						{
							ppu_status_s = i+1;

							break;
						}
					}
					else
					{
						if ((unsigned char)chr_ram[(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+8] != 0x00 ||
							(unsigned char)chr_ram[(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+16] != 0x00)
						{
							ppu_status_s = i+1;

							break;
						}
					}
				}
				else if (map_number == 4) // mmc3
				{
					ppu_status_s = i+1; // THIS NEEDS WORK!

					break;
				}
				else if (map_number == 7) // anrom
				{
					if ((unsigned char)cart_header[5] > 0)
					{
						if ((unsigned char)cart_rom[chr_offset+(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+8] != 0x00 ||
							(unsigned char)cart_rom[chr_offset+(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+16] != 0x00)
						{
							ppu_status_s = i+1;

							break;
						}
					}
					else
					{
						if ((unsigned char)chr_ram[(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+8] != 0x00 ||
							(unsigned char)chr_ram[(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+16] != 0x00)
						{
							ppu_status_s = i+1;

							break;
						}
					}
				}
				else // nrom
				{
					if ((unsigned char)cart_header[5] > 0)
					{
						if ((unsigned char)cart_rom[chr_offset+(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+8] != 0x00 ||
							(unsigned char)cart_rom[chr_offset+(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+16] != 0x00)
						{
							ppu_status_s = i+1;

							break;
						}
					}
					else
					{
						if ((unsigned char)chr_ram[(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+8] != 0x00 ||
							(unsigned char)chr_ram[(oam_ram[1]&0xFE)*16+0x1000*(oam_ram[1]&0x01)+i+16] != 0x00)
						{
							ppu_status_s = i+1;

							break;
						}
					}
				}
			}
		}
	}
	
	if (ppu_status_s < 0xFF)
	{
		ppu_status_s += oam_ram[0]; // add y-coordinate
		ppu_status_d = oam_ram[3]; // x-coordinate
	}
	else
	{
		ppu_status_s = oam_ram[0]; // default in case Sprite 0 is blank???
		ppu_status_d = oam_ram[3];
	}
}

void nes_init()
{
	prg_ram = sys_ram + 0x0000; // cpu ram from 0x6000 to 0x7FFF (if used)
	
	cpu_ram = ext_ram + 0x0000; // only cpu ram from 0x0000 to 0x07FF
	ppu_ram = ext_ram + 0x0800; // ppu ram from 0x2000 to 0x2FFF (halved, mirrored)
	chr_ram = ext_ram + 0x1000; // ppu ram from 0x0000 to 0x1FFF (if used)
	
	cart_bottom = cart_ram + 0x0000;
	
	if (nes_init_flag == 0)
	{
		nes_init_flag = 1;
		
		// offsets
		prg_offset = 16; // length of header
		chr_offset = 16 + (unsigned char)cart_rom[4]*16384; // length of header + prg_rom
		end_offset = 16 + (unsigned char)cart_rom[4]*16384 + (unsigned char)cart_rom[5]*8192; // end of cart rom (not used?)

		// mapper
		map_number = (((unsigned char)cart_rom[6] & 0xF0) >> 4); // + ((unsigned char)cart_rom[7] & 0xF0); // high nibble isn't reliable?
		
		// scrolling mask
		if (((unsigned char)cart_rom[6] & 0x01) == 0x00)
		{
			ppu_status_m = 0x0000; // vertical scrolling
		}
		else
		{
			ppu_status_m = 0x0001; // horizontal scrolling
		}

		// prg ram
		cpu_status_r = (((unsigned char)cart_rom[6] & 0x02) >> 1); // (not used?)

		// reset
		cpu_reg_pc = (unsigned char)cart_rom[prg_offset+0x4000*((unsigned char)cart_rom[4]-1)+0x3FFC] + ((unsigned char)cart_rom[prg_offset+0x4000*((unsigned char)cart_rom[4]-1)+0x3FFD] << 8);

		// header
		for (unsigned char i=0; i<16; i++)
		{
			cart_header[i] = cart_rom[i];
		}

		for (unsigned long i=0; i<16384; i++)
		{
			cart_bottom[i] = cart_rom[prg_offset+0x4000*((unsigned char)cart_rom[4]-1)+i];
		}
		
		//SendLongHex(cpu_reg_pc);
		//SendString("Initialized\n\r\\");
	}
	
	if (nes_reset_flag == 0)
	{
		nes_reset_flag = 1;
		
		// reset
		cpu_reg_pc = (unsigned char)cart_rom[prg_offset+0x4000*((unsigned char)cart_rom[4]-1)+0x3FFC] + ((unsigned char)cart_rom[prg_offset+0x4000*((unsigned char)cart_rom[4]-1)+0x3FFD] << 8);
		
		//SendLongHex(cpu_reg_pc);
		//SendString("Reset\n\r\\");
	}
}

void nes_loop()
{	
	if (nes_loop_option > 0 && nes_loop_halt == 0)
	{
		nes_loop_write = 0;
		nes_loop_read = 0;
		nes_loop_branch = 0;
		nes_loop_reg_a = cpu_reg_a;
		nes_loop_reg_x = cpu_reg_x;
		nes_loop_reg_y = cpu_reg_y;
	}
	
	cpu_current_cycles = 0;
	
	if (nes_loop_halt == 0)
	{
#ifdef DEBUG
debug_reset();
#endif
		cpu_current_cycles += cpu_run();
#ifdef DEBUG
debug_capture(0);
#endif
	}

	if (nes_loop_option > 0 && nes_loop_halt == 0)
	{
		if (nes_loop_write > 0 ||
			(nes_loop_reg_a != cpu_reg_a) ||
			(nes_loop_reg_x != cpu_reg_x) ||
			(nes_loop_reg_y != cpu_reg_y))
		{
			nes_loop_detect = 0;
			nes_loop_count = 0;
			nes_loop_cycles = 0;
		}
		else if (nes_loop_branch > 0)
		{
			nes_loop_max = nes_loop_detect;
			
			nes_loop_detect = 1;
			nes_loop_count++;
			
			nes_loop_cycles += cpu_current_cycles;
			
			if (nes_loop_count >= 16) // 16 is a good number
			{
				nes_loop_halt = 1;
				
				nes_loop_detect = 0;
				nes_loop_count = 0;
				
				nes_loop_cycles = ((unsigned long)(nes_loop_cycles / nes_loop_max) >> 4); // divide by 16
			}
		}
		else if (nes_loop_detect > 0 && nes_loop_read > 0)
		{
			nes_loop_detect++;
			
			nes_loop_cycles += cpu_current_cycles;
				
			if (nes_loop_detect > nes_loop_option+1)
			{
				nes_loop_detect = 0;
				nes_loop_count = 0;
				nes_loop_cycles = 0;
			}
		}
	}

	cpu_current_cycles += cpu_dma_cycles;
	
	cpu_dma_cycles = 0;
	
	if (nes_loop_halt > 0) cpu_current_cycles = nes_loop_cycles;
	
	if (cpu_current_cycles == 0)
	{	
		nes_error(0x01);
	}
	
	if (map_number == 1) // mmc1
	{
		map_mmc1_ready = 1;
	}

	ppu_tile_cycles += ((cpu_current_cycles<<1)+cpu_current_cycles);
	
	while (ppu_tile_cycles >= 8 && ppu_tile_count < 33) // 8 dots per tile
	{		
		ppu_tile_cycles -= 8;

		if (ppu_scanline_count >= 0)
		{
			if (ppu_flag_eb > 0)
			{
				if (map_number == 4)
				{
					nes_mmc3_irq_toggle(ppu_flag_b); // this is a speed hack
				}
			}
			
			//if (ppu_frame_count >= screen_rate)
			//{		
#ifdef DEBUG
debug_reset();
#endif
				nes_background(ppu_tile_count, ppu_scanline_count);
#ifdef DEBUG
debug_capture(1);
#endif
			//}
			
			if (ppu_flag_eb > 0)
			{
				nes_horizontal_increment();
			}
		}

		ppu_tile_count++;
	}
	
	ppu_scanline_cycles += ((cpu_current_cycles<<1)+cpu_current_cycles);
	
	if (ppu_scanline_cycles >= 341) // 113.667 cycles per scanline
	{		
		ppu_scanline_cycles -= 341;
		
		if (ppu_scanline_count >= 0 && ppu_scanline_count <= 240)
		{
			if (ppu_flag_es > 0)
			{
				if (map_number == 4)
				{
					nes_mmc3_irq_toggle(ppu_flag_s); // this is a speed hack
				}
			}
			
			//if (ppu_frame_count >= screen_rate)
			//{	
#ifdef DEBUG
debug_reset();
#endif
				if (ppu_flag_h == 0) // 8x8 sprites
				{
					nes_sprites(1, ppu_scanline_count+1, ppu_scanline_count+2); // background sprites

					if (nes_hack_sprite_priority > 0)
					{
						nes_sprites(2, ppu_scanline_count-8, ppu_scanline_count-7); // foreground sprite hack
					}
					else
					{
						nes_sprites(0, ppu_scanline_count-8, ppu_scanline_count-7); // foreground sprites
					}
				}
				else // 8x16 sprites
				{
					nes_sprites(1, ppu_scanline_count+1, ppu_scanline_count+2); // background sprites

					if (nes_hack_sprite_priority > 0)
					{
						nes_sprites(2, ppu_scanline_count-16, ppu_scanline_count-15); // foreground sprite hack
					}
					else
					{
						nes_sprites(0, ppu_scanline_count-16, ppu_scanline_count-15); // foreground sprites
					}
				}
#ifdef DEBUG
debug_capture(2);
#endif
			//}
		}
		
		ppu_tile_count = 0;
		ppu_scanline_count++;

		if (ppu_scanline_count == 0)
		{
			if (ppu_flag_eb > 0) ppu_reg_v = ppu_reg_t;
		}
		else if (ppu_scanline_count > 0)
		{
			if (ppu_flag_eb > 0) nes_vertical_increment();
		}
	}
	
	apu_sample_cycles += (cpu_current_cycles); // adjusting for better quality??
	
	if (apu_sample_cycles >= 29)
	{
		apu_sample_cycles -= 29;
		
		if (nes_audio_flag > 0)
		{	
			apu_sample_cycles_ext++;

			if (apu_sample_cycles_ext >= 12)
			{
				apu_sample_cycles_ext = 0;
		
				nes_audio(30);
			}
			else
			{
				nes_audio(29); // 29.0825 = 29780.5 / 1024, cut in half
			}
			
			nes_mixer(); // move this somewhere else?
		}
	}

	// irq
	if (cpu_flag_i == 0 && !(cpu_temp_opcode == 0x58 || cpu_temp_opcode == 0x78 || cpu_temp_opcode == 0x28))
	{
		if (apu_flag_i == 1 || apu_flag_f == 1)
		{	
			nes_irq();
		}
	}
		
	// irq
	if (cpu_flag_i == 0 && !(cpu_temp_opcode == 0x58 || cpu_temp_opcode == 0x78 || cpu_temp_opcode == 0x28))
	{
		if (map_number == 4) // mmc3
		{
			if (map_mmc3_irq_enable > 0 && map_mmc3_irq_interrupt > 0)
			{
				if (ppu_scanline_cycles + 8 > map_mmc3_irq_delay)
				{
					if (map_mmc3_irq_delay > 0)
					{
						nes_irq();
					}

					map_mmc3_irq_interrupt = 0;
				}
			}
		}
	}
	
	ppu_frame_cycles += (cpu_current_cycles<<1);
	
	if (ppu_frame_cycles < 14) // at least one instruction
	{		
		ppu_status_v = 0x0001;

		ppu_flag_v = 0x0001; // keep it high
		
		nes_loop_halt = 0; // turn CPU back on

		ppu_status_0 = 0;

		ppu_flag_0 = 0;
	}
	else if (ppu_frame_cycles < 4546) // 2273 cycles in v-blank
	{
		// v-sync
		ppu_status_v = 0x0001;
		
		if (nes_hack_vsync_flag > 0)
		{
			ppu_flag_v = 0x0001; // do not keep it high, just set it high once??? (see below)
		}
		
		nes_loop_halt = 0; // turn CPU back on
		
		ppu_status_0 = 0;

		ppu_flag_0 = 0;
	}
	else if (ppu_frame_cycles < 59565) // 29780.5 cycles per frame
	{	
		if (ppu_status_v == 0x0001)
		{
			nes_sprite_0_calc();

			//if (ppu_frame_count >= screen_rate)
			//{					
				nes_border();
			//}
			
			nes_loop_halt = 0; // turn CPU back on
		}
		
		// v-sync
		ppu_status_v = 0x0000;
		
		ppu_flag_v = 0x0000;
		
		if (ppu_flag_eb > 0 && ppu_flag_es > 0)
		{
			if (ppu_scanline_count >= (ppu_status_s) && ppu_scanline_cycles >= (ppu_status_d) && ppu_status_0 == 0) // very rough math
			{	
				ppu_status_0 = 1;
				
				ppu_flag_0 = 1;
				
				nes_loop_halt = 0; // turn CPU back on
			}
		}
	}
	else
	{		
		ppu_frame_cycles -= 59565;
		
		// v-sync
		ppu_status_v = 0x0001;
		
		ppu_flag_v = 0x0001;
		
		ppu_reg_a = 0;	
		
		// nmi
		if (ppu_flag_e != 0x0000)
		{	
			nes_nmi();	
		}

		nes_loop_halt = 0; // turn CPU back on

		nes_buttons();

		if (screen_handheld == 0)
		{
			screen_file = open("/dev/fb0", O_RDWR);
			write(screen_file, &screen_large_buffer, 640*480*2);
			close(screen_file);
		}
		else
		{
			screen_file = open("/dev/fb0", O_RDWR);
			write(screen_file, &screen_small_buffer, 320*240*2);
			close(screen_file);
		}

		for (int i=0; i<1024; i++)
		{
			audio_array[i] = (unsigned char)(audio_buffer[audio_read] + 0x80);
			audio_read++;
			if (audio_read >= AUDIO_LEN) audio_read = 0;
		}		

		write(audio_file, &audio_array, 1024); // AUDIO_SAMPLES

		for (int i=0; i<AUDIO_LEN; i++)
		{
			audio_buffer[i] = 0x00;
		}

		audio_read = 0;
		audio_write = 0;

		nes_wait();
		
		ppu_frame_count++;
		
		ppu_tile_count = 0;
		ppu_scanline_count = -22; // was -21
	}
}

int main(const int argc, const char **argv)
{
	printf("PICnes, by Professor Steven Chad Burrow\n");

	if (argc < 2)
	{
		printf("Needs ROM file argument!\n");
	
		return 0;
	}

	FILE *input = NULL;

	input = fopen(argv[1], "rb");
	if (!input)
	{
		printf("Couldn't open ROM file!\n");
		
		return 0;
	}
	
	int bytes = 1;
	unsigned char buffer = 0;
	unsigned long loc = 0;

	while (bytes > 0)
	{
		bytes = fscanf(input, "%c", &buffer);

		if (bytes > 0)	
		{
			cart_rom[loc] = buffer;

			loc++;
		}
	}

	fclose(input);

	for (unsigned long i=0; i<640*480; i++)
	{
		screen_large_buffer[i] = 0;
	}

	for (unsigned long i=0; i<320*240; i++)
	{
		screen_small_buffer[i] = 0;
	}

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

	int sound_fragment = 0x0004000A; // 4 blocks, each is 2^A = 2^10 = 1024
	int sound_stereo = 0;
	int sound_format = AFMT_S8; // AFMT_U8;
	int sound_speed = (unsigned int)((61542)); // calculations: 61542 = 1024 * 60.0988 + 1

	audio_file = open("/dev/dsp", O_WRONLY);

	ioctl(audio_file, SNDCTL_DSP_SETFRAGMENT, &sound_fragment); // needed to stop the drift
	ioctl(audio_file, SNDCTL_DSP_STEREO, &sound_stereo);
	ioctl(audio_file, SNDCTL_DSP_SETFMT, &sound_format);
	ioctl(audio_file, SNDCTL_DSP_SPEED, &sound_speed);

	nes_init();
		
	nes_running = 1;

	while (nes_running > 0)
	{ 
		nes_loop(); // frame rate divider and external interrupt
	}

	return 1;
}
