#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <time.h>

#include "raylib.h"

#define ROM_START 0x200
#define FONT_START 0x050


#define DISPLAY_WIDTH 64
#define DISPLAY_HEIGHT 32

#define DISPLAY_SCALE 20


#define CYCLES_PER_FRAME 9

#define VF 15


struct Chip8 {
	uint8_t memory[4096];
	uint8_t V[16];
	uint16_t I;
	uint16_t PC;
	uint16_t stack[16];
	uint8_t SP;
	uint8_t delay_timer;
	uint8_t sound_timer;
	uint8_t display[64 * 32];
	uint8_t keys[16];
  int8_t waitKey;
};

const uint8_t FONT_SET[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9 

    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};


void runWindow(struct Chip8* chip);

void CHIP8_Init(struct Chip8* chip);
int CHIP8_LoadRom(struct Chip8* chip, char* romPath);
void CHIP8_Cycle(struct Chip8* chip);

void getRomPath(char* buffer, size_t size);
void parseRomPath(char* path);



int main()
{
    srand(time(NULL));

    
    struct Chip8 chip;

    
    CHIP8_Init(&chip);


    char romPath[512];

    getRomPath(romPath, sizeof(romPath));

    if (CHIP8_LoadRom(&chip, romPath) == EXIT_FAILURE)
    {
        return EXIT_FAILURE;
    }
    

    runWindow(&chip);


    return EXIT_SUCCESS;
}



void runWindow(struct Chip8* chip)
{
    InitAudioDevice();


    Sound beep = LoadSound("beep.wav");

    Color bgColor = { 0, 0, 0, 255 }; //{ 20, 20, 20, 255 };

    Color pixelOnColor = { 235, 225, 204, 255 }; //{ 235, 218, 180, 255 }; //{ 230, 230, 230, 255 };
    Color pixelOffColor = { 18, 17, 16, 255 }; //{ 18, 17, 16, 255 }; //{ 12, 12, 12, 255 };

    int cellSize = DISPLAY_SCALE;

    const int KEY_MAP[16] = {
    KEY_X,   KEY_ONE, KEY_TWO, KEY_THREE,   // 0 1 2 3
    KEY_Q,   KEY_W,   KEY_E,   KEY_A,       // 4 5 6 7
    KEY_S,   KEY_D,   KEY_Z,   KEY_C,       // 8 9 A B
    KEY_FOUR, KEY_R,  KEY_F,   KEY_V        // C D E F
    };


    InitWindow(
        DISPLAY_WIDTH * (DISPLAY_SCALE),
        DISPLAY_HEIGHT * (DISPLAY_SCALE),
        "CHIP-8 Emulator"
    );

    SetTargetFPS(60);


    while (!WindowShouldClose())
    {
        for (int k = 0; k < 16; k++)
        {
            chip->keys[k] = IsKeyDown(KEY_MAP[k]) ? 1 : 0;
        }

        for (int i = 0; i < CYCLES_PER_FRAME; i++)
        {
            CHIP8_Cycle(chip);
        }


        BeginDrawing();


        ClearBackground(bgColor);

        for (int x = 0; x < DISPLAY_WIDTH; x++)
        {
            for (int y = 0; y < DISPLAY_HEIGHT; y++)
            {
                Color currentCellColor = (chip->display[y * DISPLAY_WIDTH + x] == 1) ? pixelOnColor : pixelOffColor;

                DrawRectangle(
                    x * (cellSize),
                    y * (cellSize),
                    cellSize,
                    cellSize,
                    currentCellColor
                );
            }
        }

        
        EndDrawing();

        
        if (chip->sound_timer > 0)
        {
            if (!IsSoundPlaying(beep)) PlaySound(beep);
        }
        else
        {
            if (IsSoundPlaying(beep)) StopSound(beep);
        }

        if (chip->delay_timer > 0) chip->delay_timer--;
        if (chip->sound_timer > 0) chip->sound_timer--;
    }


    UnloadSound(beep);
    CloseAudioDevice();

    CloseWindow();
}



void CHIP8_Init(struct Chip8* chip)
{
	memset(chip, 0, sizeof(*chip));

    memcpy(
        chip->memory + FONT_START, // It's the law
        FONT_SET, 
        sizeof(FONT_SET)
    );

    chip->PC = 0x200;
    chip->waitKey = -1;
}

int CHIP8_LoadRom(struct Chip8* chip, char* romPath)
{
    FILE* rom = fopen(romPath, "rb");

    if (rom == NULL)
    {
        perror(romPath);
        return EXIT_FAILURE;
    }


    fseek(rom, 0, SEEK_END);

    size_t romSize = ftell(rom);

    if (romSize == 0)
    {
        fprintf(stderr, "Error: ROM is empty!\n");

        fclose(rom);
        return EXIT_FAILURE;
    }
    else if(romSize > (4096 - ROM_START))
    {
        fprintf(stderr, "Error: ROM size too large!\n");

        fclose(rom);
        return EXIT_FAILURE;
    }

    fseek(rom, 0, SEEK_SET);


    fread(chip->memory + ROM_START, 1, romSize, rom);


    fclose(rom);
    return EXIT_SUCCESS;
}

void CHIP8_Cycle(struct Chip8* chip)
{
    uint8_t opcodeHighBits = chip->memory[chip->PC];
    uint8_t opcodeLowBits = chip->memory[chip->PC + 1];

    uint16_t opcode = (opcodeHighBits << 8) | opcodeLowBits;

    uint8_t opcodeFirstNibble = opcode >> 12; // (First 4 bits of the opcode)


    chip->PC += 2;


    switch (opcodeFirstNibble)
    {
        case 0x0:
        {
            /*
                00E0 & 00EE (Clear & Return)

                00E0 clears the screen
                00EE pops the top address off the stack into PC (the reverse of 2NNN)
            */

            if (opcode == 0x00E0)
            {
                memset(chip->display, 0, sizeof(chip->display));
            }
            else if (opcode == 0x00EE && chip->SP > 0)
            {
                chip->SP--;
                chip->PC = chip->stack[chip->SP];
            }

            break;
        }

        case 0x1:
        {
            /*
                1NNN (Jump)
                
                Just sets PC to NNN
            */


            uint16_t nnn = (opcode & 0x0FFF);

            chip->PC = nnn;

            break;
        }

        case 0x2:
        {
            /*
                2NNN (Call)

                Pushes PC onto the stack, then sets PC to NNN
                The saved value on the stack is the return address
            */


            if (chip->SP < 16)
            {
                uint16_t nnn = (opcode & 0x0FFF);

                chip->stack[chip->SP] = chip->PC;

                chip->SP++;

                chip->PC = nnn;
            }
            
            break;
        }

        case 0x3:
        {
            /*
                3XNN (Skip)

                Opposite of 4XNN
                Skips one instruction if V[x] == NN
            */

            
            uint8_t x = (opcode & 0x0F00) >> 8;
            uint8_t nn = (opcode & 0x00FF);

            if (chip->V[x] == nn)
            {
                chip->PC += 2;
            }

            break;
        }

        case 0x4:
        {
            /*
                4XNN (Skip)

                Opposite of 3XNN
                Skips one instruction if V[x] != NN
            */


            uint8_t x = (opcode & 0x0F00) >> 8;
            uint8_t nn = (opcode & 0x00FF);

            if (chip->V[x] != nn)
            {
                chip->PC += 2;
            }

            break;
        }

        case 0x5:
        {
            /*
                5XY0 (Skip)
               
                Opposite of 9XY0
                Skips one instruction if V[x] == V[y]                              
            */


            uint8_t x = (opcode & 0x0F00) >> 8;
            uint8_t y = (opcode & 0x00F0) >> 4;

            if (chip->V[x] == chip->V[y])
            {
                chip->PC += 2;
            }

            break;
        }

        case 0x6:
        {
            /*
                6XNN (Set)

                Just sets V[x] to NN
            */


            uint8_t x = (opcode & 0x0F00) >> 8;
            uint8_t nn = (opcode & 0x00FF);

            chip->V[x] = nn;

            break;
        }

        case 0x7:
        {
            /*
                7XNN (Add)

                Just adds NN to V[x]
            */


            uint8_t x = (opcode & 0x0F00) >> 8;
            uint8_t nn = (opcode & 0x00FF);

            chip->V[x] += nn;

            break;
        }

        case 0x8:
        {
            /*
                8XYN (Logic & Arithmetic)

                They perform some arithmetic or logic gating between V[x] & V[y]
                Specific instruction depends on what the last nibble is
            */


            uint8_t x = (opcode & 0x0F00) >> 8;
            uint8_t y = (opcode & 0x00F0) >> 4;
            uint8_t n = (opcode & 0x000F);

            switch (n)
            {
                case 0x0000:
                {
                    /*
                        8XY0 (Set)

                        Sets V[x] to V[y]
                    */


                    chip->V[x] = chip->V[y];

                    break;
                }
                case 0x0001:
                {
                    /*
                        8XY1 (OR)

                        ORs the bits of V[x] & V[y]
                        The result is stored in V[x]
                    */


                    chip->V[x] = (chip->V[x] | chip->V[y]);

                    break;
                }
                case 0x0002:
                {
                    /*
                        8XY2 (AND)

                        ANDs the bits of V[x] & V[y]
                        The result is stored in V[x]
                    */


                    chip->V[x] = (chip->V[x] & chip->V[y]);

                    break;
                }
                case 0x0003:
                {
                    /*
                        8XY3 (XOR)

                        XORs the bits of V[x] & V[y]
                        The result is stored in V[x]
                    */


                    chip->V[x] = (chip->V[x] ^ chip->V[y]);

                    break;
                }
                case 0x0004:
                {
                    /*
                        8XY4 (Add)

                        Adds V[x] & V[y] together
                        The result is stored in V[x]
                    */


                    uint16_t additionResult = chip->V[x] + chip->V[y];

                    chip->V[x] = (uint8_t)additionResult;

                    chip->V[VF] = (additionResult > 255) ? 1 : 0;

                    break;
                }
                case 0x0005:
                {
                    /*
                        8XY5 (Subtract)

                        V[x] = V[x] - V[y], Same as 8XY7 but with the operands swapped
                        VF = 1 if there was no borrow (V[x] >= V[y]), otherwise 0
                    */


                    uint8_t minuend = chip->V[x];
                    uint8_t subtrahend = chip->V[y];

                    uint8_t subtractionResult = minuend - subtrahend;

                    chip->V[x] = subtractionResult;

                    chip->V[VF] = (minuend >= subtrahend) ? 1 : 0;

                    break;
                }
                case 0x0006:
                {
                    /*
                        8XY6 (Shift Right)

                        Opposite of 8XYE
                        Shifts V[x] right by 1 (Ignores V[y])
                        VF = the bit shifted out
                    */


                    uint8_t shiftedOutBit = chip->V[x] & 0x01;

                    chip->V[x] >>= 1;

                    chip->V[VF] = shiftedOutBit;

                    break;
                }
                case 0x0007:
                {
                    /*
                        8XY7 (Subtract)

                        V[x] = V[y] - V[x], Same as 8XY5 but with the operands swapped
                        VF = 1 if there was no borrow (V[x] >= V[y]), otherwise 0
                    */

                    uint8_t minuend = chip->V[y];
                    uint8_t subtrahend = chip->V[x];

                    uint8_t subtractionResult = minuend - subtrahend;

                    chip->V[x] = subtractionResult;

                    chip->V[VF] = (minuend >= subtrahend) ? 1 : 0;

                    break;
                }
                case 0x000E:
                {
                    /*
                        8XYE (Shift Left)

                        Opposite of 8XY6
                        Shifts V[x] left by 1 (Ignores V[y])
                        VF = the bit shifted out
                    */

                    uint8_t shiftedOutBit = (chip->V[x] & 0x80) >> 7;

                    chip->V[x] <<= 1;

                    chip->V[VF] = shiftedOutBit;

                    break;
                }
                default:
                {
                    fprintf(stderr, "\nUnknown opcode %04X\n", opcode);

                    break;
                }
            }

            break;
        }

        case 0x9:
        {
            /*
                9XY0 (Skip)

                Opposite of 5XY0
                Skips one instruction if V[x] != V[y]
            */


            uint8_t x = (opcode & 0x0F00) >> 8;
            uint8_t y = (opcode & 0x00F0) >> 4;

            if (chip->V[x] != chip->V[y])
            {
                chip->PC += 2;
            }

            break;
        }

        case 0xA:
        {
            /*
                ANNN (Set I)

                Sets Index register to NNN
            */


            uint16_t nnn = (opcode & 0x0FFF);

            chip->I = nnn;

            break;
        }

        case 0xB:
        {
            /*
                BNNN (Jump with Offset)

                Sets PC to NNN + V[0]
            */


            uint16_t nnn = (opcode & 0x0FFF);

            chip->PC = (nnn + chip->V[0]) & 0xFFF;

            break;
        }

        case 0xC:
        {
            /*
                CXNN (Random)

                V[x] = (random byte 0 to 255) & NN
            */


            uint8_t nn = (opcode & 0x00FF);
            uint8_t x = (opcode & 0x0F00) >> 8;

            chip->V[x] = (rand() % 256) & nn;

            break;
        }

        case 0xD:
        {
            /*
                DXYN (Draw)

                Draw an 8-wide, N-tall sprite, from memory at I, at column V[x] & row V[y]
                Each bit in the sprite XORs the screen bit under it, VF = 1 if any pixel was turned off
                The start position wraps around, but the sprite itself just cuts off at the edges
            */

            uint8_t x = chip->V[(opcode & 0x0F00) >> 8];
            uint8_t y = chip->V[(opcode & 0x00F0) >> 4];

            uint8_t spriteStartPixelPosX = x % DISPLAY_WIDTH;
            uint8_t spriteStartPixelPosY = y % DISPLAY_HEIGHT;

            uint8_t n = (opcode & 0x000F);

            chip->V[VF] = 0;


            for (int rows = 0; rows < n; rows++)
            {
                uint8_t spriteRowByte = chip->memory[chip->I + rows];


                int spriteCurrentPixelPosY = spriteStartPixelPosY + rows;

                if (spriteCurrentPixelPosY >= DISPLAY_HEIGHT)
                {
                    break;
                }


                for (int bits = 0; bits < 8; bits++)
                {
                    uint8_t spriteRowBit = (spriteRowByte >> (7 - bits)) & 1;


                    int spriteCurrentPixelPosX = spriteStartPixelPosX + bits;

                    if (spriteCurrentPixelPosX >= DISPLAY_WIDTH)
                    {
                        break;
                    }


                    int indexToDrawPixel = (spriteCurrentPixelPosY * DISPLAY_WIDTH) + spriteCurrentPixelPosX;

                    if (spriteRowBit == 1 && chip->display[indexToDrawPixel] == 1)
                    {
                        chip->display[indexToDrawPixel] = 0;
                        chip->V[VF] = 1;
                    }
                    else if (spriteRowBit == 1 && chip->display[indexToDrawPixel] == 0)
                    {
                        chip->display[indexToDrawPixel] = 1;
                    }
                }
            }

            break;
        }

        case 0xE:
        {
            /*
                EX9E & EXA1 (Skip if Key)

                EX9E skips one instruction if key corresponding to V[x] is held
                EXA1 skips one instruction if key corresponding to V[x] is not held
            */


            uint8_t x = (opcode & 0x0F00) >> 8;
            uint8_t nn = (opcode & 0x00FF);

            if (nn == 0x009E && chip->keys[chip->V[x] & 0xF] == 1)
            {
                chip->PC += 2;
            }
            else if (nn == 0x00A1 && chip->keys[chip->V[x] & 0xF] != 1)
            {
                chip->PC += 2;
            }

            break;
        }

        case 0xF:
        {
            /*
                FXNN

                Miscellaneous instructions
            */


            uint8_t x = (opcode & 0x0F00) >> 8;
            uint8_t nn = (opcode & 0x00FF);

            switch (nn)
            {
                case 0x0007:
                {
                    /*
                        FX07 (Timer)

                        Opposite of FX15
                        Sets V[x] to the delay timer
                    */


                    chip->V[x] = chip->delay_timer;

                    break;
                }

                case 0x0015:
                {
                    /*
                        FX15 (Timer)

                        Opposite of FX07
                        Sets the delay timer to V[x] 
                    */


                    chip->delay_timer = chip->V[x];

                    break;
                }

                case 0x0018:
                {
                    /*
                        FX18 (Timer)

                        Sets the sound timer to V[x]
                    */


                    chip->sound_timer = chip->V[x];
                    
                    break;
                }

                case 0x001E:
                {
                    /*
                        FX1E (Add to Index)

                        Add V[x] to index register
                        VF = 1, if I overflows
                    */


                    chip->I += chip->V[x];

                    if (chip->I > 0xFFF)
                    {
                        chip->V[VF] = 1;
                    }

                    chip->I &= 0xFFF;

                    break;
                }

                case 0x000A:
                {
                    /*
                        FX0A (Get Key)

                        Halts execution of new instructions until a key has been pressed & released
                    */


                    if (chip->waitKey == -1)
                    {
                        for (int k = 0; k < 16; k++)
                        {
                            if (chip->keys[k])
                            {
                                chip->waitKey = k;

                                break;
                            }
                        }

                        chip->PC -= 2;
                    }
                    else if (chip->keys[chip->waitKey])
                    {
                        chip->PC -= 2;
                    }
                    else
                    {
                        chip->V[x] = chip->waitKey;

                        chip->waitKey = -1;
                    }

                    break;
                }

                case 0x0029:
                {
                    /*
                        FX29 (Get Font Character)

                        Sets I to the address of a char's font data corresponding to the one stored in V[x] 
                    */


                    uint8_t lastNibble = chip->V[x] & 0x0F;

                    chip->I = FONT_START + lastNibble * 5;

                    break;
                }

                case 0x0033:
                {
                    /*
                        FX33 (Split)

                        Splits the number in V[x] into individual parts
                        Then stores them in memory relative to I
                    */


                    chip->memory[chip->I] = chip->V[x] / 100;
                    chip->memory[chip->I + 1] = (chip->V[x] / 10) % 10;
                    chip->memory[chip->I + 2] = chip->V[x] % 10;

                    break;
                }

                case 0x0055:
                {
                    /*
                        FX55 (Store Memory)

                        Reverse of FX65
                        Writes V0 to VX into memory starting at I
                    */


                    for (int i = 0; i <= x; i++)
                    {
                        chip->memory[chip->I + i] = chip->V[i];
                    }

                    break;
                }

                case 0x0065:
                {
                    /*
                        FX65 (Load Memory)

                        Reverse of FX55
                        Writes stored data from memory starting at I into V0 to VX
                    */


                    for (int i = 0; i <= x; i++)
                    {
                        chip->V[i] = chip->memory[chip->I + i];
                    }

                    break;
                }

                default:
                {
                    fprintf(stderr, "\nUnknown opcode %04X\n", opcode);

                    break;
                }
            }

            break;
        }

        default:
        {
            fprintf(stderr, "\nUnknown opcode %04X\n", opcode);

            break;
        }
    }
}



void getRomPath(char* buffer, size_t size)
{
    printf("Enter ROM Path : ");

    if (fgets(buffer, size, stdin) == NULL)
    {
        buffer[0] = '\0';

        return;
    }

    parseRomPath(buffer);
}


void parseRomPath(char* path)
{
    path[strcspn(path, "\r\n")] = '\0';


    char* write = path;

    for (char* read = path; *read != '\0'; read++)
    {
        if (*read != '"')
        {
            *write++ = *read;
        }
    }

    *write = '\0';
}
