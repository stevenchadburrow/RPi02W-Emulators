#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <pigpio.h>

// must compile with:  -lpigpio

int main()
{
	system("echo \"0000000000000\" > buttons.val");

	char button[13];

	for (int i=0; i<13; i++) button[i] = '0';

	char string[256];

	for (int i=0; i<256; i++) string[i] = 0;

    if (gpioInitialise() == PI_INIT_FAILED)
	{
        printf("ERROR: Failed to initialize the GPIO interface.\n");
        return 0;
    }

	gpioSetMode(4, PI_INPUT); // du
	gpioSetPullUpDown(4, PI_PUD_UP);
	gpioSetMode(5, PI_INPUT); // dr?
	gpioSetPullUpDown(5, PI_PUD_UP);
	gpioSetMode(6, PI_INPUT); // dl
	gpioSetPullUpDown(6, PI_PUD_UP);
	gpioSetMode(7, PI_INPUT); // dd?
	gpioSetPullUpDown(7, PI_PUD_UP);

	gpioSetMode(22, PI_INPUT); // a
	gpioSetPullUpDown(22, PI_PUD_UP);
	gpioSetMode(23, PI_INPUT); // b
	gpioSetPullUpDown(23, PI_PUD_UP);
	gpioSetMode(24, PI_INPUT); // x
	gpioSetPullUpDown(24, PI_PUD_UP);
	gpioSetMode(25, PI_INPUT); // y
	gpioSetPullUpDown(25, PI_PUD_UP);

	gpioSetMode(26, PI_INPUT); // lb
	gpioSetPullUpDown(26, PI_PUD_UP);
	gpioSetMode(27, PI_INPUT); // rb
	gpioSetPullUpDown(27, PI_PUD_UP);

    gpioSetMode(16, PI_INPUT); // select
	gpioSetPullUpDown(16, PI_PUD_UP);
	gpioSetMode(17, PI_INPUT); // start
	gpioSetPullUpDown(17, PI_PUD_UP);
	gpioSetMode(18, PI_INPUT); // menu
	gpioSetPullUpDown(18, PI_PUD_UP);

	while (1)
	{
		for (int i=0; i<13; i++) button[i] = '0';

		if (gpioRead(18) == 0) // menu
		{
			button[0] = '1';
		}

		if (gpioRead(4) == 0) // du
		{
			button[1] = '1';
		}

		if (gpioRead(7) == 0) // dr?
		{
			button[2] = '1';
		}

		if (gpioRead(6) == 0) // dl
		{
			button[3] = '1';
		}

		if (gpioRead(5) == 0) // dd?
		{
			button[4] = '1';
		}

		if (gpioRead(16) == 0) // select
		{
			button[5] = '1';
		}

		if (gpioRead(17) == 0) // start
		{
			button[6] = '1';
		}

		if (gpioRead(22) == 0) // a
		{
			button[7] = '1';
		}

		if (gpioRead(23) == 0) // b
		{
			button[8] = '1';
		}

		if (gpioRead(24) == 0) // x
		{
			button[9] = '1';
		}

		if (gpioRead(25) == 0) // y
		{
			button[10] = '1';
		}

		if (gpioRead(26) == 0) // lb
		{
			button[11] = '1';
		}

		if (gpioRead(27) == 0) // rb
		{
			button[12] = '1';
		}

		for (int i=0; i<256; i++) string[i] = 0;

		sprintf(string, "echo \"%c%c%c%c%c%c%c%c%c%c%c%c%c\" > buttons.val",
			button[0], button[1], button[2], button[3],
			button[4], button[5], button[6], button[7], button[8],
			button[9], button[10], button[11], button[12]);

		//printf("%s\n", string);
		system(string);

		if (button[0] != '0') break;
	}

	gpioTerminate();

    return 0;
}
