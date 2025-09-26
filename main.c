#include <stdio.h>
#include <sys/ioctl.h>
#include <linux/kd.h>
#include <termios.h>

#include "peanut_gb.c"

int main(const int argc, const char **argv)
{
	char game_path[256];
	char game_name[64];

	for (int i=0; i<256; i++) game_path[i] = 0;
	for (int i=0; i<64; i++) game_name[i] = 0;

	if (argc < 2)
	{
		FILE *game_file = NULL;

		game_file = fopen("/home/username/PeanutGB/game.val", "rt");
		if (!game_file)
		{
			printf("Cannot find game.val, needs argument\n");
			return 0;
		}

		fscanf(game_file, "%s", game_name);

		fclose(game_file);

		sprintf(game_path, "/home/username/PeanutGB/%s", game_name);
	}
	else
	{
		sprintf(game_path, "%s", argv[1]);
	}

	printf("Game Path: %s\n", game_path);

	char size_buffer[3];
	int size_file = open("/sys/class/graphics/fb0/virtual_size", O_RDONLY);
	read(size_file, &size_buffer, 3);
	close(size_file);

	if (size_buffer[0] == '3' && size_buffer[1] == '2' && size_buffer[2] == '0')
	{
		scanline_handheld = 1;
	}
	else
	{
		scanline_handheld = 0;
	}

	FILE *input = NULL;

	input = fopen(game_path, "rt");

	if (!input)
	{
		printf("Error Opening File\n");
		return 0;
	}

	int bytes = 1;
	unsigned char buffer = 0;
	unsigned long location = 0;

	while (bytes > 0)
	{
		bytes = fscanf(input, "%c", &buffer);

		if (bytes > 0)
		{
			cart_rom[location] = buffer;

			location++;
		}
	}

	fclose(input);

	//struct termios term;
	//tcgetattr(fileno(stdin), &term);
	//term.c_lflag &= ~ECHO;
	//term.c_lflag &= ~ICANON;
	//tcsetattr(fileno(stdin), 0, &term); // turn off echo

	char tty_name[16];

	system("tty > /home/username/PeanutGB/temp.val");
	system("echo \"                \" >> /home/username/PeanutGB/temp.val");

	int tty_file = open("/home/username/PeanutGB/temp.val", O_RDWR);
	read(tty_file, &tty_name, 16);
	close(tty_file);

	system("rm /home/username/PeanutGB/temp.val");

	for (int i=0; i<16; i++)
	{
		if (tty_name[i] <= ' ') tty_name[i] = 0;
	}

	tty_file = open(tty_name, O_RDWR);
	ioctl(tty_file, KDSETMODE, KD_GRAPHICS); // turn off tty

	// this needs 'sudo' perhaps do it in rc.local or something?
	//system("modprobe snd-pcm-oss"); // opens up /dev/dsp
	// this sets the volume, change accordingly
	//system("amixer set Master 50%");

	//PeanutGB(0, 
	//	"/home/username/PeanutGB/keyboard.val", // DMG
	//	"/home/username/PeanutGB/joystick.val");
	PeanutGB(1, 
		"/home/username/PeanutGB/keyboard.val", 
		"/home/username/PeanutGB/joystick.val"); // GBC

	ioctl(tty_file, KDSETMODE, KD_TEXT); // turn on tty
	close(tty_file);

	//term.c_lflag |= ECHO;
	//term.c_lflag |= ICANON;
	//tcsetattr(fileno(stdin), 0, &term); // turn on echo

	return 1;
}
