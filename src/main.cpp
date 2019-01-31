#include <chrono>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "GL/glut.h"

uint8_t memory[4096];
uint8_t data_registers[16]; // V
uint16_t address_register; // I
uint16_t stack[16];
int stack_current;
int delay_timer, sound_timer;
int program_counter = 0x200;
bool keys[16];
uint8_t pixels[64 * 32];

const uint8_t fontset[80] = { 
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

const char keymap[] = {
    'x', '1', '2', '3',
    'q', 'w', 'e', 'a',
    's', 'd', 'z', 'c',
    '4', 'r', 'f', 'v'
};

void run_cycle() {
    uint16_t opcode = memory[program_counter] << 8 | memory[program_counter + 1];

    switch (opcode & 0xF000) {
        case 0x0000:
            switch (opcode)
            {
                case 0x00E0: // 00E0: Clear the screen
                    memset(pixels, 0, sizeof(pixels));
                    program_counter += 2;
                    break;

                case 0x00EE: // 00EE: Return from a subroutine
                    program_counter = stack[--stack_current] + 2;
                    break;

                default:
                    printf("Unknown opcode: 0x%X\n", opcode);
                    exit(1);
            }
            break;

        case 0x1000: // 1NNN: Jump to address NNN
            program_counter = opcode & 0x0FFF;
            break;

        case 0x2000: // 2NNN: Call subroutine at NNN
            stack[stack_current++] = program_counter;
            program_counter = opcode & 0x0FFF;
            break;

        case 0x3000: // 3XNN: Skip the next instruction if VX equals NN
            if (data_registers[(opcode & 0x0F00) >> 8] == (opcode & 0x00FF))
                program_counter += 2;
            program_counter += 2;
            break;

        case 0x4000: // 4XNN: Skip the next instruction if VX doesn't equal NN
            if (data_registers[(opcode & 0x0F00) >> 8] != (opcode & 0x00FF))
                program_counter += 2;
            program_counter += 2;
            break;

        case 0x5000: // 5XY0: Skip the next instruction if VX equals VY
            if (data_registers[(opcode & 0x0F00) >> 8] == data_registers[(opcode & 0x00F0) >> 4])
                program_counter += 2;
            program_counter += 2;
            break;

        case 0x6000: // 6XNN: Set VX to NN
            data_registers[(opcode & 0x0F00) >> 8] = opcode & 0x00FF;
            program_counter += 2;
            break;

        case 0x7000: // 7XNN: Add NN to VX without setting VF
            data_registers[(opcode & 0x0F00) >> 8] += opcode & 0x00FF;
            program_counter += 2;
            break;

        case 0x8000:
            switch (opcode & 0x000F)
            {
                case 0x0000: // 8XY0: Set VX to the value of VY
                    data_registers[(opcode & 0x0F00) >> 8] = data_registers[(opcode & 0x00F0) >> 4];
                    program_counter += 2;
                    break;

                case 0x0001: // 8XY1: Set VX to VX or VY
                    data_registers[(opcode & 0x0F00) >> 8] |= data_registers[(opcode & 0x00F0) >> 4];
                    program_counter += 2;
                    break;

                case 0x0002: // 8XY2: Set VX to VX and VY
                    data_registers[(opcode & 0x0F00) >> 8] &= data_registers[(opcode & 0x00F0) >> 4];
                    program_counter += 2;
                    break;

                case 0x0003: // 8XY3: Set VX to VX xor VY
                    data_registers[(opcode & 0x0F00) >> 8] ^= data_registers[(opcode & 0x00F0) >> 4];
                    program_counter += 2;
                    break;

                case 0x0004: // 8XY4: Add VY to VX and set VF based on if there was a carry
                    data_registers[0xF] = (data_registers[(opcode & 0x0F00) >> 8] +
                            data_registers[(opcode & 0x00F0) >> 4] > 0xFF);
                    data_registers[(opcode & 0x0F00) >> 8] += data_registers[(opcode & 0x00F0) >> 4];
                    program_counter += 2;
                    break;

                case 0x0005: // 8XY5: Subtract VY from VX and set VF based on if there was a borrow
                    data_registers[0xF] = (data_registers[(opcode & 0x0F00) >> 8] >=
                            data_registers[(opcode & 0x00F0) >> 4]);
                    data_registers[(opcode & 0x0F00) >> 8] -= data_registers[(opcode & 0x00F0) >> 4];
                    program_counter += 2;
                    break;

                case 0x0006: // 8XY6: Shift VX to the right by 1 and store the least significant bit in VF
                    data_registers[0xF] = data_registers[(opcode & 0x0F00) >> 8] & 0x01;
                    data_registers[(opcode & 0x0F00) >> 8] >>= 1;
                    program_counter += 2;
                    break;

                case 0x0007: // 8XY7: Sets VX to VY minus VX and sets VF based on if there was a borrow
                    data_registers[0xF] = (data_registers[(opcode & 0x00F0) >> 4] >=
                            data_registers[(opcode & 0x0F00) >> 8]);
                    data_registers[(opcode & 0x0F00) >> 8] = data_registers[(opcode & 0x00F0) >> 4] -
                            data_registers[(opcode & 0x0F00) >> 8];
                    program_counter += 2;
                    break;

                case 0x000E: // 8XYE: Shift VX to the left by 1 and store the most significant bit in VF
                    data_registers[0xF] = data_registers[(opcode & 0x0F00) >> 8] >> 7;
                    data_registers[(opcode & 0x0F00) >> 8] <<= 1;
                    program_counter += 2;
                    break;

                default:
                    printf("Unknown opcode: 0x%X\n", opcode);
                    exit(1);
            }
            break;

        case 0x9000: // 9XY0: Skip the next instruction if VX doesn't equal VY
            if (data_registers[(opcode & 0x0F00) >> 8] != data_registers[(opcode & 0x00F0) >> 4])
                program_counter += 2;
            program_counter += 2;
            break;

        case 0xA000: // ANNN: Set I to NNN
            address_register = opcode & 0x0FFF;
            program_counter += 2;
            break;

        case 0xB000: // BNNN: Jump to address NNN plus V0
            program_counter = (opcode & 0x0FFF) + data_registers[0];
            break;

        case 0xC000: // CXNN: Set VX to the result of a random number and NN
            data_registers[(opcode & 0x0F00) >> 8] = (rand() % 255) & (opcode & 0x00FF);
            program_counter += 2;
            break;

        case 0xD000: // DXYN: Draw a sprite stored in VI at (VX, VY) with width 8 and height N
            data_registers[0xF] = 0;
            for (int y = 0; y < (opcode & 0x000F); y++) {
                uint8_t data = memory[address_register + y];
                for (int x = 0; x < 8; x++) {
                    if (data & (0x80 >> x)) {
                        uint8_t *pixel = &pixels[((data_registers[(opcode & 0x00F0) >> 4] + y) *
                                64 + data_registers[(opcode & 0x0F00) >> 8] + x) % (64 * 32)];
                        *pixel = ~(*pixel);
                        if (!(*pixel))
                            data_registers[0xF] = 1;
                    }
                }
            }

            program_counter += 2;
            break;

        case 0xE000:
            switch (opcode & 0x00FF)
            {
                case 0x009E: // EX9E: Skip the next instruction if the key stored in VX is pressed
                    if (keys[data_registers[(opcode & 0x0F00) >> 8]])
                        program_counter += 2;
                    program_counter += 2;
                    break;
                case 0x00A1: // EXA1: Skip the next instruction if the key stored in VX isn't pressed
                    if (!keys[data_registers[(opcode & 0x0F00) >> 8]])
                        program_counter += 2;
                    program_counter += 2;
                    break;
                default:
                    printf("Unknown opcode: 0x%X\n", opcode);
                    exit(1);
            }
            break;

        case 0xF000:
            switch (opcode & 0x00FF)
            {
                case 0x0007: // FX07: Set VX to the value of the delay timer
                    data_registers[(opcode & 0x0F00) >> 8] = delay_timer;
                    program_counter += 2;
                    break;

                case 0x000A: // FX0A: Wait for a key press and then store the key in VX
                    for (int i = 0; i < 16; i++) {
                        if (keys[i]) {
                            data_registers[(opcode & 0x0F00) >> 8] = i;
                            program_counter += 2;
                        }
                    }
                    break;

                case 0x0015: // FX15: Set the delay timer to VX
                    delay_timer = data_registers[(opcode & 0x0F00) >> 8];
                    program_counter += 2;
                    break;

                case 0x0018: // FX18: Set the sound timer to VX
                    sound_timer = data_registers[(opcode & 0x0F00) >> 8];
                    program_counter += 2;
                    break;

                case 0x001E: // FX1E: Add VX to I
                    address_register += data_registers[(opcode & 0x0F00) >> 8];
                    program_counter += 2;
                    break;

                case 0x0029: // FX29: Set I to the location of the sprite for the character in VX
                    address_register = 0x050 + data_registers[(opcode & 0x0F00) >> 8] * 5;
                    program_counter += 2;
                    break;

                case 0x0033: // FX33: Store the binary-coded decimal representation of VX at VI
                    memory[address_register    ] = (data_registers[(opcode & 0x0F00) >> 8] / 100);
                    memory[address_register + 1] = (data_registers[(opcode & 0x0F00) >> 8] / 10) % 10;
                    memory[address_register + 2] = (data_registers[(opcode & 0x0F00) >> 8] % 100) % 10;
                    program_counter += 2;
                    break;

                case 0x0055: // FX55: Store V0 to VX in memory starting at VI
                    memcpy(&memory[address_register], data_registers, ((opcode & 0x0F00) >> 8) + 1);
                    program_counter += 2;
                    break;

                case 0x0065: // FX65: Fill V0 to VX in memory with values starting at VI
                    memcpy(data_registers, &memory[address_register], ((opcode & 0x0F00) >> 8) + 1);
                    program_counter += 2;
                    break;

                default:
                    printf("Unknown opcode: 0x%X\n", opcode);
                    exit(1);
            }
            break;

        default:
            printf("Unknown opcode: 0x%X\n", opcode);
            exit(1);
    }

    if (delay_timer > 0)
        delay_timer--;

    if (sound_timer > 0) {
        sound_timer--;
        if (sound_timer == 0) {
            printf("\a");
            fflush(stdout);
        }
    }
}

void draw() {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 64, 32, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, pixels);

    glBegin(GL_QUADS);
    glTexCoord2i(1, 1); glVertex2f( 1, -1);
    glTexCoord2i(0, 1); glVertex2f(-1, -1);
    glTexCoord2i(0, 0); glVertex2f(-1,  1);
    glTexCoord2i(1, 0); glVertex2f( 1,  1);
    glEnd();

    glFlush();
}

void key_down(unsigned char key, int x, int y) {
    for (int i = 0; i < sizeof(keys); i++) {
        if (key == keymap[i])
            keys[i] = true;
    }
}

void key_up(unsigned char key, int x, int y) {
    for (int i = 0; i < sizeof(keys); i++)
        if (key == keymap[i])
            keys[i] = false;
}

void loop() {
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

    run_cycle();
    draw();

    std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
    if (elapsed.count() < 1.0f / 60)
        usleep((1.0f / 60 - elapsed.count()) * 100000);

    glutPostRedisplay();
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Please specify a ROM to load.\n");
        return 0;
    }

    FILE *rom = fopen(argv[1], "rb");
    if (!rom) {
        printf("Failed to open ROM!\n");
        return 0;
    }

    // Load the ROM into memory
    fseek(rom, 0, SEEK_END);
    int romsize = ftell(rom);
    fseek(rom, 0, SEEK_SET);
    fread(&memory[program_counter], sizeof(uint8_t), romsize, rom);

    fclose(rom);

    // Load the fontset into memory
    memcpy(&memory[0x050], fontset, sizeof(fontset));

    glutInit(&argc, argv);
    glutInitWindowSize(256, 128);
    glutCreateWindow("CHIP-GR8");
    glEnable(GL_TEXTURE_2D);

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glutDisplayFunc(loop);
    glutKeyboardFunc(key_down);
    glutKeyboardUpFunc(key_up);
    glutMainLoop();

    return 0;
}
