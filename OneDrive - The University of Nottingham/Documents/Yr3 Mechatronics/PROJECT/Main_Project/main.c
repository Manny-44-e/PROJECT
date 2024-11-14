#include <stdio.h>
#include <stdlib.h>

int main()
{

    // Simple example in the use of sprintf

    // Define some variables
    float f1, f2, f3;
    int i1, i2,i3;;
    char buffer[50];    // The string we are going to 'print' to

    // Just some values so we have things to print to the string
    f1 = 1.7;
    f2 = 6.6;
    f3 = 9.3;

    i1 = 0;
    i2 = 1;
    i3 = 2;

    // Sample with floats
    printf ("Example using floats\n");
    sprintf (buffer,"%f %f %f\n", f1, f2, f3);
    printf ("%s", buffer);

    // Sample with integers (slightly different way to display a string)
    printf ("\nExample using integers\n");
    sprintf (buffer,"%d %d %d\n", i1, i2, i3);
    puts (buffer);

    // More 'complex' formatting where we include text and variables
    // this will be like the code you need
    printf ("\nAdding to generate a sting of the form you will be sending\n");
    sprintf (buffer,"G%d X:%.2f Y:%.2f\n", i1, f2, f3);
    puts (buffer);

    return 0;
}