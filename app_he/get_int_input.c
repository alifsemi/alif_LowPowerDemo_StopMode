#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define STRING_MAX_LEN    7

int32_t get_int_input()
{
    int len = 0;
    char char_in;
    char string_in[STRING_MAX_LEN+1] = {0};

    while(1) {
        scanf("%c", &char_in);

        /* line feed OR carriage return */
        if ((10 == char_in) || (13 == char_in)) {
            /* only break if string len is greater than 0 */
            if(len > 0) break;
        }

        /* backspace OR delete */
        if ((8 == char_in) || (127 == char_in)) {
            len--;
            if (len < 0) {
                len = 0;
            } else {
                printf("\b \b");
            }
            string_in[len] = 0;
        }

        /* max string length reached, do not add to string */
        if(len == STRING_MAX_LEN) {
            continue;
        }

        /* character is a valid number */
        if((char_in >= '0') && (char_in <= '9')) {
            printf("%c", char_in);
            string_in[len] = char_in;
            len++;
        }
    }

    return atoi(string_in);
}
