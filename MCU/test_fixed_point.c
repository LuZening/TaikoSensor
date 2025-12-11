#include <stdio.h>
#include <stdint.h>
#include <string.h>

void fixed_point_to_string(char *buffer, uint16_t fixed_value)
{
    uint16_t integer_part = fixed_value / 100;
    uint16_t decimal_part = fixed_value % 100;

    if (decimal_part == 0) {
        sprintf(buffer, "%u", integer_part);
    } else if (decimal_part < 10) {
        sprintf(buffer, "%u.0%u", integer_part, decimal_part);
    } else {
        sprintf(buffer, "%u.%u", integer_part, decimal_part);
    }
}

int main() {
    char buf[16];
    
    fixed_point_to_string(buf, 80);
    printf("80 (0.80): %s\n", buf);
    
    fixed_point_to_string(buf, 120);
    printf("120 (1.20): %s\n", buf);
    
    fixed_point_to_string(buf, 100);
    printf("100 (1.00): %s\n", buf);
    
    fixed_point_to_string(buf, 5);
    printf("5 (0.05): %s\n", buf);
    
    return 0;
}
