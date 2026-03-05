#include <ctype.h>
#include <stdio.h>

int main(void)
{
	FILE* logfile = fopen("log.txt", "rb");
	if(!logfile){
		perror("Could not open log file.\n");
	}

	unsigned char last_char = 0;
	unsigned char second_to_last_char = 0;
	unsigned char c = 0;
	for(int i = 0; c != EOF; i++){
		if (ferror(logfile)){
			puts("\nI/O error when reading");
			break;
		}
		else if (feof(logfile))
		{
			puts("\nEnd of file is reached successfully");
			break;
		}

		second_to_last_char = last_char;
		last_char = c;
		c = fgetc(logfile);


		if(second_to_last_char == 0x08){
			if(last_char == 0x80){
				printf("\npen position  ");
			}
			else if(last_char == 0x81){
				printf("\npen pressure  ");
			}
			else if(last_char == 0x82){
				printf("\nbottom button ");
			}
			else if(last_char == 0x84){
				printf("\ntop button    ");
			}
			else if(last_char == 0xe0){
				printf("\nbutton press  ");
			}
		}

		if(i % 2 == 0){
			printf("%02x%02x ", last_char, second_to_last_char);
			//little endian
		}

	}

	fclose(logfile);

}
