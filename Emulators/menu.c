#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>

int main()
{
	system("echo \"0000000000000\" > buttons1.val");
	system("echo \"0000000000000\" > buttons2.val");
	system("echo \"0000000000000\" > joystick1.val");
	system("echo \"0000000000000\" > joystick2.val");
	system("echo \"0000000000000\" > keyboard1.val");
	system("echo \"0000000000000\" > keyboard2.val");


	int select = 0;
	int total = 0;
	char list[1000][64];

	system("ls -R --group-directories-first ROMS > list.val");
	
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
				if (i < 0) printf("  \n");
				else if (i >= total) printf("  \n");
				else if (i == select)
				{
					if (strstr(list[select], ":") != NULL) printf(">:%s\n", list[select]);
					else if (strstr(list[select], ".") == NULL) printf(">-%s-\n", list[select]);
					else printf("> %s\n", list[select]);
				}
				else
				{
					if (strstr(list[i], ":") != NULL) printf(" :%s\n", list[i]);
					else if (strstr(list[i], ".") == NULL) printf(" -%s-\n", list[i]);
					else printf("  %s\n", list[i]);
				}
			}
		}

		for (int i=0; i<13; i++) button[i] = '0';

		for (int round=0; round<6; round++)
		{
			if (round == 0)
			{
				file = open("buttons1.val", O_RDONLY);
				if (file >= 0)
				{	
					read(file, &buffer, 13);
					close(file);
				}
			}
			else if (round == 1)
			{
				file = open("buttons2.val", O_RDONLY);
				if (file >= 0)
				{	
					read(file, &buffer, 13);
					close(file);
				}
			}
			else if (round == 2)
			{
				file = open("joystick1.val", O_RDONLY);
				if (file >= 0)
				{	
					read(file, &buffer, 13);
					close(file);
				}
			}
			else if (round == 3)
			{
				file = open("joystick2.val", O_RDONLY);
				if (file >= 0)
				{	
					read(file, &buffer, 13);
					close(file);
				}
			}
			else if (round == 4)
			{
				file = open("keyboard1.val", O_RDONLY);
				if (file >= 0)
				{	
					read(file, &buffer, 13);
					close(file);
				}
			}
			else if (round == 5)
			{
				file = open("keyboard2.val", O_RDONLY);
				if (file >= 0)
				{	
					read(file, &buffer, 13);
					close(file);
				}
			}

			for (int i=0; i<13; i++) if (buffer[i] != '0') button[i] = '1';
		}

		if (button[5] == '1' || button[6] == '1') // select/start
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
		else if (button[7] == '1') // A
		{
			char command[512];
			for (int i=0; i<512; i++) command[i] = 0;

			char path[256];
			for (int i=0; i<256; i++) path[i] = 0;
			
			for (int i=select; i>=0; i--)
			{
				if (strstr(list[i], ":") != NULL)
				{
					sprintf(path, "%s", list[i]);

					int remove = 0;

					for (int j=0; j<256; j++)
					{
						if (path[j] == ':') remove = 1;

						if (remove > 0)
						{
							path[j] = 0;
						}
					}

					break;
				}
			}

			if (strstr(list[select], ".NES") != NULL ||
				strstr(list[select], ".nes") != NULL)
			{
				sprintf(command, "echo './PICnes/PICnes.o %s/%s' > game.sh", path, list[select]); // alter some...

				system(command);

				running = 0;	
			}
			else if (strstr(list[select], ".GBA") != NULL ||
				strstr(list[select], ".gba") != NULL) // this must come before check for .GB or .gb!!!
			{
				sprintf(command, "echo './gdkGBA/gdkGBA.o %s/%s' > game.sh", path, list[select]);
			
				system(command);

				running = 0;
			}
			else if (strstr(list[select], ".GB") != NULL ||
				strstr(list[select], ".gb") != NULL ||
				strstr(list[select], ".GBC") != NULL ||
				strstr(list[select], ".gbc") != NULL)
			{
				sprintf(command, "echo './PeanutGB/PeanutGB.o %s/%s' > game.sh", path, list[select]);
				
				system(command);

				running = 0;
			}
			else // directory?
			{
				sprintf(command, "%s:", list[select]);

				for (int i=0; i<1000; i++)
				{
					if (strstr(list[i], command) != NULL)
					{
						select = i;

						break;
					}
				}

				prev_clock = clock();

				redraw = 1;
			}
		}
		else if (button[8] == '1') // B
		{
			prev_clock = clock();

			select = 0;

			redraw = 1;
		}
	}

	return 1;
}
