#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

const char *quick_game = "TOBU.GB"; // ROMS/TOBUDX.GBC

int main()
{
	int select = 0;
	int total = 0;
	char list[1000][64];

	system("ls /home/username/PeanutGB/ROMS > /home/username/PeanutGB/list.val");
	
	FILE *input = NULL;

	input = fopen("/home/username/PeanutGB/list.val", "rt");
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

	system("rm /home/username/PeanutGB/list.val");

	system("echo '0' > /home/username/PeanutGB/game.val");

	int file = 0;
	char buffer[13] = { '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0' };
	char button[13] = { '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0' };	

	int redraw = 1;

	unsigned long prev_clock = 0;

	while (button[0] == '0') // escape
	{
		if (redraw > 0)
		{
			redraw = 0;

			system("clear");
			printf("PeanutGB-RPi02W\n");
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

		file = open("/home/username/PeanutGB/keyboard.val", O_RDONLY);
		read(file, &buffer, 13);
		close(file);

		for (int i=0; i<13; i++) if (buffer[i] != '0') button[i] = '1';
		
		file = open("/home/username/PeanutGB/joystick.val", O_RDONLY);
		read(file, &buffer, 13);
		close(file);

		for (int i=0; i<13; i++) if (buffer[i] != '0') button[i] = '1';

		if (button[1] == '1' && clock() > prev_clock + 100000) // W
		{
			prev_clock = clock();

			if (select > 0) select--;

			redraw = 1;
		}
		else if (button[2] == '1' && clock() > prev_clock + 100000) // S
		{
			prev_clock = clock();

			if (select < total-1) select++;

			redraw = 1;
		}
		else if (button[12] == '1') // R
		{
			char command[512];
			for (int i=0; i<512; i++) command[i] = 0;
			sprintf(command, "echo '%s' > /home/username/PeanutGB/game.val", quick_game);
			system(command);

			button[0] = '1'; // escape
		}
		else if (button[5] == '1' ||
			button[6] == '1' ||
			button[7] == '1' ||
			button[8] == '1')
		{
			char command[512];
			for (int i=0; i<512; i++) command[i] = 0;
			sprintf(command, "echo 'ROMS/%s' > /home/username/PeanutGB/game.val", list[select]);
			system(command);

			button[0] = '1'; // escape
		}
	}

	return 1;
}
