#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
//#include <conio.h>
//#include <windows.h>
#include "rs232.h"
#include "serial.h"
#define MAX_LINES 1027
#define bdrate 115200               /* 115200 baud */
#define MAX_CHARACTERS 128
#define MAX_WORDS 100
#define MAX_BUFFER_SIZE 4096

// Declare structure to contain 3 integer values
struct StrokesCoord
{
    float X, Y;
    int Pen;
};

// Declare structure to contain a pointer to StrokeCoord structure
struct Character_Font
{
    int ASCII_VALUE, nChCoord;
    struct StrokesCoord *XYP_Strokes;
};

struct Character_Font *Font_Data;
struct StrokesCoord *Current_Ch_Coord;

// Funtion definitions
bool Extract_Single_Stroke_Data(FILE *SingleStrokeFont);
bool Extract_Text_String(FILE *test, char *Text_Buffer, size_t Buffer_Size);
void ASCII_Character_Search_XYP_Offset(char *text, float Text_Height, char *buffer);
float Scale_Factor(float Text_Height);
void SendCommands (char *gcode_buffer );

int main()
{
    //char mode[]= {'8','N','1',0};
    char buffer[100];

    // If we cannot open the port then give up immediately
    if ( CanRS232PortBeOpened() == -1 )
    {
        printf ("\nUnable to open the COM port (specified in serial.h) ");
        exit (0);
    }

    // Time to wake up the robot
    printf ("\nAbout to wake up the robot\n");

    // We do this by sending a new-line
    sprintf (buffer, "\n");
     // printf ("Buffer to send: %s", buffer); // For diagnostic purposes only, normally comment out
    PrintBuffer (buffer);
    Sleep(100);

    // This is a special case - we wait  until we see a dollar ($)
    WaitForDollar();

    printf ("\nThe robot is now ready to draw\n");

        //These commands get the robot into 'ready to draw mode' and need to be sent before any writing commands
    sprintf (buffer, "G1 X0 Y0 F1000\n");
    SendCommands(buffer);
    sprintf (buffer, "M3\n");
    SendCommands(buffer);
    sprintf (buffer, "S0\n");
    SendCommands(buffer);

    // PrintBuffer(gcode_buffer);

    // Allocate memory for character_Font array and StrokeCoord array
    Font_Data = (struct Character_Font *)malloc(MAX_CHARACTERS * sizeof(struct Character_Font));
    if (Font_Data == NULL) 
    {
        printf("Error: Unable to allocate memory for font data.\n");
        return 1;
    }

    Current_Ch_Coord = (struct StrokesCoord *)malloc(MAX_LINES * sizeof(struct StrokesCoord));
    if (Current_Ch_Coord == NULL) 
    {
        printf("Error: Unable to allocate memory for character coordinates.\n");
        free(Font_Data);
        return 1;
    }

    char Text_Buffer[MAX_BUFFER_SIZE];
    char gcode_buffer[MAX_BUFFER_SIZE];
    float Text_Height;

    // Extract SingleStroke file containing font data
    FILE *StrokeFontPointer= fopen("SingleStrokeFont.txt", "r");
    if (!Extract_Single_Stroke_Data(StrokeFontPointer))
    {
        printf("Error: Failed to load font data.\n");
        free(Font_Data);
        free(Current_Ch_Coord);
        return 1;
    }

    // Prompts user to enter text file to be drawn
    printf("Enter the name of the text file to process: ");
    char Text_filename[300];
    scanf("%299s", Text_filename);

    FILE *TextPointer=fopen(Text_filename, "r");

    // Fail safe in case of failure to open text file
    if(!TextPointer)
    {
        printf("Could not open text file\n ");
        free(Font_Data);
        free(Current_Ch_Coord);
        return 1;
    }

    // Extract data from input text file
    if(!Extract_Text_String(TextPointer, Text_Buffer, sizeof(Text_Buffer)))
    {
        printf("Error: Failed to read text file.\n");
        free(Font_Data);
        free(Current_Ch_Coord);
        fclose(TextPointer);
        return 1;
    }
    fclose(TextPointer);

    // Prompts user for height of text
    printf("Enter the height of the text between 4 and 10mm: ");
    scanf("%f", &Text_Height);
    if (Text_Height < 4 || Text_Height > 10) {   // Ensures a height betwen 4 and 10mm 
        printf("Error: Text height must be between 4 and 10mm.\n");  // is entered for the robot to read
        free(Font_Data);
        free(Current_Ch_Coord);
        return 1;
    }

    
    Scale_Factor(Text_Height);

    // G-Code generation, Offsets and ASCII character search
    ASCII_Character_Search_XYP_Offset(Text_Buffer, (float)Text_Height, gcode_buffer);

    // SendCommands(gcode_buffer);
    PrintBuffer(gcode_buffer);

    // Before we exit the program we need to close the COM port
    CloseRS232Port();
    printf("Com port now closed\n");

    free(Font_Data);
    free(Current_Ch_Coord);
    return 0;
}

// Function definitions
bool Extract_Single_Stroke_Data(FILE *SingleStrokeFont) 
{
    // Stops operation if file fails to open
    if(SingleStrokeFont == NULL)
    {
        printf("\nThe file could not be opened for reading, exting");
        return false;
    }
    int Current_Char = -1;
    int Stroke_Index = 0;
    int X,Y,Pen;

    // Extracting stroke coordinates and storing it in a structure and allocating memory
    while(fscanf(SingleStrokeFont, "%d %d %d", &X, &Y, &Pen) !=EOF)
    {
        if(X==999)
     {
        Current_Char=Y;
        Stroke_Index=0;
        Font_Data[Current_Char].ASCII_VALUE=Y;
        Font_Data[Current_Char].nChCoord=Pen;
        Font_Data[Current_Char].XYP_Strokes = (struct StrokesCoord *)malloc((size_t)(Font_Data[Current_Char].nChCoord) * sizeof(struct StrokesCoord));
     }

    if(Font_Data[Current_Char].XYP_Strokes == NULL)
    {
        printf("Unable to store memory\n");
        fclose(SingleStrokeFont);
        return false;
     }
     else if(Current_Char>=0 && Stroke_Index<Font_Data[Current_Char].nChCoord)
     {
        Font_Data[Current_Char].XYP_Strokes[Stroke_Index].X=(float)X;
        Font_Data[Current_Char].XYP_Strokes[Stroke_Index].Y=(float)Y;
        Font_Data[Current_Char].XYP_Strokes[Stroke_Index].Pen=Pen;
        Stroke_Index++;
     }
    }
    fclose(SingleStrokeFont);
    return true;
}

bool Extract_Text_String(FILE *test_data, char *Text_Buffer, size_t Buffer_Size) 
{
    // Expecting file to be passed directly, fails if not
    if (test_data == NULL) 
    {
        printf("Error: Could not open text file.\n");
        return false;
    }

    size_t length = fread(Text_Buffer, 1, Buffer_Size - 1, test_data);
    Text_Buffer[length] = '\0';

    fclose(test_data);
    return true;
}

void ASCII_Character_Search_XYP_Offset(char *text, float Text_Height, char *gcode_buffer)
{
    float scale_factor = Scale_Factor((float)Text_Height);
    int X_offset = 0;
    int Y_offset = 0;
    const int MAX_WIDTH = 100;

    sprintf(gcode_buffer, "F1000\nM3\nS0\n");
    printf("G Codes generating...\n");

    for (size_t i = 0; text[i] != '\0'; i++) 
    {
        char c = text[i];

        if (c == '\n') 
        {
            Y_offset -= 5; // Line spacing in negative Y direction
            X_offset = 0;  // to create a new line
            continue;
        }
        if (c == ' ')  // Start of a new word, adjusts X to allow space between words
        {
            X_offset += (int)(18 * scale_factor); // Space width between characters
            if (X_offset > MAX_WIDTH)        // Check if the current line exceeds the width
            {          
                X_offset = 0;                   // Move to the next line
                Y_offset -= 5;                  
            }
            continue;
        }

        int ASCII_VALUE = (int)c; // Ensures character falls in line with ASCII requirements
        if (ASCII_VALUE < 0 || ASCII_VALUE >= MAX_CHARACTERS || Font_Data[ASCII_VALUE].nChCoord == 0) 
        {
            printf("Warning: Unrecognised character '%c'.\n", c); // Failsafe if somehow the wrong character 
            continue;                                             // ASCII value is read from the file
        }

        struct Character_Font *char_font = &Font_Data[ASCII_VALUE];

        for (int j = 0; j < char_font->nChCoord; j++) 
        {
            struct StrokesCoord *XYP_Strokes = &char_font->XYP_Strokes[j];

            int x = X_offset + (int)(XYP_Strokes->X * scale_factor);
            int y = Y_offset + (int)(XYP_Strokes->Y * scale_factor);

            if (XYP_Strokes->Pen == 1) // Pen is down therefore draws line
            {
                sprintf(gcode_buffer + strlen(gcode_buffer), "G1 X%d Y%d\n", x, y);
            } else 
            {   // Ensure pen is up when moving to next stroke
                sprintf(gcode_buffer + strlen(gcode_buffer), "G0 X%d Y%d\n", x, y);
            }
        }

        // Adds a empty line space between each character G Codes
        sprintf(gcode_buffer + strlen(gcode_buffer), "\n");

       
        int Char_Width = (int)(18 * scale_factor);
        if (X_offset + Char_Width > MAX_WIDTH)       
        {          
            X_offset = 0;                   // Move to the next line
            Y_offset -= 5;
        }
        X_offset += Char_Width;
    }

    sprintf(gcode_buffer + strlen(gcode_buffer), "G0 X0 Y0\nS0\n");
}

float Scale_Factor(float Text_Height) //
{
    return Text_Height / 18.0f;
}


// Send the data to the robot - note in 'PC' mode you need to hit space twice
// as the dummy 'WaitForReply' has a getch() within the function.
void SendCommands (char *buffer )
{
    // printf ("Buffer to send: %s", buffer); // For diagnostic purposes only, normally comment out
    PrintBuffer (buffer);
    WaitForReply();
    Sleep(100); // Can omit this when using the writing robot but has minimal effect
    // getch(); // Omit this once basic testing with emulator has taken place
}
