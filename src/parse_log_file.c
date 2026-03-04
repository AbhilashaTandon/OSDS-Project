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
	while (c != EOF){
		second_to_last_char = last_char;
		last_char = c;
		c = fgetc(logfile);
		if(second_to_last_char != 0x08){
			continue;
		}
		if(last_char == 0x80){
			printf("pos");
			for(int i = 0; i < 8; i++){
				second_to_last_char = last_char;
				last_char = c;
				c = fgetc(logfile);
				if(i % 2 == 1){
					printf("%02x%02x ", last_char, second_to_last_char);
				}
			}
			printf("\n");
		}
		else if(last_char == 0x81){
			printf("pres");
			for(int i = 0; i < 8; i++){
				second_to_last_char = last_char;
				last_char = c;
				c = fgetc(logfile);
				if(i % 2 == 1){
					printf("%02x%02x ", last_char, second_to_last_char);
				}
			}
			printf("\n");
		}
		if (ferror(logfile)){
			puts("I/O error when reading");
			break;
		}
		else if (feof(logfile))
		{
			puts("End of file is reached successfully");
			break;
		}

	}

	fclose(logfile);

}
