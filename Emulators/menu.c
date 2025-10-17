#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>

int main()
{
	int select = 0;
	int total = 0;
	char list[1000][64];

	system("ls ROMS > list.val");
	
	FILE *input = NULL;

	input = fopen("list.val", "rt");
	if (!input)
	{
		printf("Cannot open list.val\n");
		return 0;
	}

	int bytes = 1;
	char single[256];

	while (bytes > 0)
	{
		bytes = fscanf(input, "%s", single);
		
		if (bytes > 0)
		{
			for (int i=0; i<64; i++)
			{
				list[total][i] = single[i];
			}

			total++;		
		}
	}

	fclose(input);

	system("rm list.val");

	system("echo '' > game.val");

	int file = 0;
	char buffer[13] = { '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0' };
	char button[13] = { '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0' };	

	int redraw = 1;

	unsigned long prev_clock = 0;

	unsigned char running = 1;

	int buttons_choice = 0;

	while (running > 0) // escape
	{
		if (redraw > 0)
		{
			redraw = 0;

			system("clear");
			printf("RPi02W-Emulators\n");
			printf("Using PICnes, PeanutGB, and gdkGBA\n");
			printf("D-Pad: WSAD Directions\n");
			printf("Buttons: KJIUHL = ABXYLR\n");
			printf("Enter/Backspace = Start/Select\n");
			printf("Escape to Exit\n");
			printf("Select Game:\n\n");

			for (int i=select-5; i<=select+5; i++)
			{
				if (i < 0) printf(" \n");
				else if (i >= total) printf(" \n");
				else if (i == select) printf(">%s\n", list[select]);
				else printf(" %s\n", list[i]);
			}
		}

		for (int i=0; i<13; i++) button[i] = '0';

		file = open("keyboard.val", O_RDONLY);
		read(file, &buffer, 13);
		close(file);

		if (buffer[5] == '1' ||
			buffer[6] == '1' ||
			buffer[7] == '1' ||
			buffer[8] == '1') // regular buttons
		{
			buttons_choice = 1; // keyboard
		}

		for (int i=0; i<13; i++) if (buffer[i] != '0') button[i] = '1';
		
		file = open("joystick.val", O_RDONLY);
		read(file, &buffer, 13);
		close(file);

		if (buffer[5] == '1' ||
			buffer[6] == '1' ||
			buffer[7] == '1' ||
			buffer[8] == '1') // regular buttons
		{
			buttons_choice = 2; // joystick
		}

		for (int i=0; i<13; i++) if (buffer[i] != '0') button[i] = '1';
		
		file = open("buttons.val", O_RDONLY);
		read(file, &buffer, 13);
		close(file);

		if (buffer[5] == '1' ||
			buffer[6] == '1' ||
			buffer[7] == '1' ||
			buffer[8] == '1') // regular buttons
		{
			buttons_choice = 0; // onboard buttons
		}

		for (int i=0; i<13; i++) if (buffer[i] != '0') button[i] = '1';

		if (button[0] == '1') // menu
		{
			system("echo '' > game.sh");

			running = 0;
		}
		else if (button[1] == '1' && clock() > prev_clock + 100000) // up
		{
			prev_clock = clock();

			if (select > 0) select--;

			redraw = 1;
		}
		else if (button[2] == '1' && clock() > prev_clock + 100000) // down
		{
			prev_clock = clock();

			if (select < total-1) select++;

			redraw = 1;
		}
		else if (button[5] == '1' ||
			button[6] == '1' ||
			button[7] == '1' ||
			button[8] == '1') // regular buttons
		{
			char command[512];
			for (int i=0; i<512; i++) command[i] = 0;

			if (strstr(list[select], ".NES") != NULL ||
				strstr(list[select], ".nes") != NULL)
			{
				if (buttons_choice == 0) // onboard buttons
				{
					sprintf(command, "echo './PICnes/PICnes.o ROMS/%s buttons.val' > game.sh", list[select]);
				}
				else if (buttons_choice == 1) // keyboard
				{
					sprintf(command, "echo './PICnes/PICnes.o ROMS/%s keyboard.val' > game.sh", list[select]);
				}
				else if (buttons_choice == 2) // joystick
				{
					sprintf(command, "echo './PICnes/PICnes.o ROMS/%s joystick.val' > game.sh", list[select]);
				}
			}
			else if (strstr(list[select], ".GBA") != NULL ||
				strstr(list[select], ".gba") != NULL) // this must come before check for .GB or .gb!!!
			{
				if (buttons_choice == 0) // onboard buttons
				{	
					sprintf(command, "echo './gdkGBA/gdkGBA.o ROMS/%s buttons.val' > game.sh", list[select]);
				}
				else if (buttons_choice == 1) // keyboard
				{	
					sprintf(command, "echo './gdkGBA/gdkGBA.o ROMS/%s keyboard.val' > game.sh", list[select]);
				}
				else if (buttons_choice == 2) // joystick
				{	
					sprintf(command, "echo './gdkGBA/gdkGBA.o ROMS/%s joystick.val' > game.sh", list[select]);
				}
			}
			else if (strstr(list[select], ".GB") != NULL ||
				strstr(list[select], ".gb") != NULL ||
				strstr(list[select], ".GBC") != NULL ||
				strstr(list[select], ".gbc") != NULL)
			{
				if (buttons_choice == 0) // onboard buttons
				{
					sprintf(command, "echo './PeanutGB/PeanutGB.o ROMS/%s buttons.val' > game.sh", list[select]);
				}
				else if (buttons_choice == 1) // keyboard
				{
					sprintf(command, "echo './PeanutGB/PeanutGB.o ROMS/%s keyboard.val' > game.sh", list[select]);
				}
				else if (buttons_choice == 2) // joystick
				{
					sprintf(command, "echo './PeanutGB/PeanutGB.o ROMS/%s joystick.val' > game.sh", list[select]);
				}
			}
	
			system(command);

			running = 0;
		}
	}

	system("echo \"0000000000000\" > keyboard.val");
	system("echo \"0000000000000\" > joystick.val");
	system("echo \"0000000000000\" > buttons.val");

	return 1;
}
