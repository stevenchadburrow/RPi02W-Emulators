#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <linux/joystick.h>
#include <unistd.h>

int main()
{
	system("echo \"0000000000000\" > /home/username/LakeSnes/joystick.val");

	char button[13];

	for (int i=0; i<13; i++) button[i] = '0';

	char string[256];

	for (int i=0; i<256; i++) string[i] = 0;

	int joy_fd;
    struct js_event joy_ev;

	joy_fd = open("/dev/input/js0", O_RDONLY);

	if (joy_fd > 0)
	{
		while (1)
		{
			read(joy_fd, &joy_ev, sizeof(struct js_event));

			//printf("%08X %02X %02X\n", js.value, js.type, js.number);

			if (joy_ev.type == 1 && joy_ev.number == 8 && 
				(unsigned char)joy_ev.value > 0) // center button for EXIT
			{
				button[0] = '1';
			}
			else if (joy_ev.type == 2 && joy_ev.number == 7) // up/down
			{
				if ((unsigned char)joy_ev.value == 0x00) // release
				{
					button[1] = '0';
					button[2] = '0';
				}
				else if ((unsigned char)joy_ev.value == 0x01) // up
				{
					button[1] = '1';
					button[2] = '0';
				}
				else if ((unsigned char)joy_ev.value == 0xFF) // down
				{
					button[1] = '0';
					button[2] = '1';
				}
			}
			else if (joy_ev.type == 2 && joy_ev.number == 6) // left/right
			{
				if ((unsigned char)joy_ev.value == 0x00) // release
				{
					button[3] = '0';
					button[4] = '0';
				}
				else if ((unsigned char)joy_ev.value == 0x01) // left
				{
					button[3] = '1';
					button[4] = '0';
				}
				else if ((unsigned char)joy_ev.value == 0xFF) // right
				{
					button[3] = '0';
					button[4] = '1';
				}
			}
			else if (joy_ev.type == 1 && joy_ev.number == 6) // back for SELECT
			{
				button[5] = (char)((unsigned char)joy_ev.value + '0');
			}
			else if (joy_ev.type == 1 && joy_ev.number == 7) // start for START
			{
				button[6] = (char)((unsigned char)joy_ev.value + '0');
			}
			else if (joy_ev.type == 1 && joy_ev.number == 1) // b for A
			{
				button[7] = (char)((unsigned char)joy_ev.value + '0');
			}
			else if (joy_ev.type == 1 && joy_ev.number == 0) // a for B
			{
				button[8] = (char)((unsigned char)joy_ev.value + '0');
			}
			else if (joy_ev.type == 1 && joy_ev.number == 3) // y for X
			{
				button[9] = (char)((unsigned char)joy_ev.value + '0');
			}
			else if (joy_ev.type == 1 && joy_ev.number == 2) // x for Y
			{
				button[10] = (char)((unsigned char)joy_ev.value + '0');
			}
			else if (joy_ev.type == 1 && joy_ev.number == 4) // l for L
			{
				button[11] = (char)((unsigned char)joy_ev.value + '0');
			}
			else if (joy_ev.type == 1 && joy_ev.number == 5) // r for R
			{
				button[12] = (char)((unsigned char)joy_ev.value + '0');
			}

			for (int i=0; i<256; i++) string[i] = 0;
	
			sprintf(string, "echo \"%c%c%c%c%c%c%c%c%c%c%c%c%c\" > /home/username/LakeSnes/joystick.val",
				button[0], button[1], button[2], button[3],
				button[4], button[5], button[6], button[7], button[8],
				button[9], button[10], button[11], button[12]);

			//printf("%s\n", string);
			system(string);

			if (button[0] != '0') break;
		}
	}

	if (joy_fd > 0) close(joy_fd);

    return 0;
}
