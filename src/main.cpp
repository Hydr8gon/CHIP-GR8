#include <chrono>
#include <curses.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

uint8_t memory[4096];
uint8_t data_registers[16]; // V
uint16_t address_register; // I
uint16_t stack[16];
int stack_current;
int delay_timer, sound_timer;
bool keys[16];
bool display[64 * 32];
int program_offset = 0x200;

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

void scan_input(bool toggle) {
    char ch = getch();

    if (!toggle)
        memset(keys, false, sizeof(keys));
    for (int i = 0; i < 16; i++) {
        if (ch == keymap[i]) {
            if (keys[i]) {
                keys[i] = false;
            } else {
                memset(keys, false, sizeof(keys));
                keys[i] = true;
            }
        }
    }

    if (ch == ' ') {
        endwin();
        exit(0);
    }
}

void run_cycle() {
    uint16_t opcode = memory[program_offset] << 8 | memory[program_offset + 1];

    switch (opcode & 0xF000) {
        case 0x0000:
            switch (opcode)
            {
                case 0x00E0: // 0x00E0: Clears the screen
                    memset(display, 0, sizeof(display));
                    program_offset += 2;
                    break;

                case 0x00EE: // 0x00EE: Returns from a subroutine
                    program_offset = stack[--stack_current] + 2;
                    break;

                default:
                    wmove(stdscr, 32, 0);
                    printw("Unknown opcode: 0x%X", opcode);
                    break;
            }
            break;

        case 0x1000: // 1NNN: Jumps to address NNN
            program_offset = opcode & 0x0FFF;
            break;

        case 0x2000: // 2NNN: Calls subroutine at NNN
            stack[stack_current++] = program_offset;
            program_offset = opcode & 0x0FFF;
            break;

        case 0x3000: // 3XNN: Skips the next instruction if VX equals NN
            if (data_registers[(opcode & 0x0F00) >> 8] == (opcode & 0x00FF))
                program_offset += 2;
            program_offset += 2;
            break;

        case 0x4000: // 4XNN: Skips the next instruction if VX doesn't equal NN
            if (data_registers[(opcode & 0x0F00) >> 8] != (opcode & 0x00FF))
                program_offset += 2;
            program_offset += 2;
            break;

        case 0x5000: // 5XY0: Skips the next instruction if VX equals VY
            if (data_registers[(opcode & 0x0F00) >> 8] == data_registers[(opcode & 0x00F0) >> 4])
                program_offset += 2;
            program_offset += 2;
            break;

        case 0x6000: // 6XNN: Sets VX to NN
            data_registers[(opcode & 0x0F00) >> 8] = opcode & 0x00FF;
            program_offset += 2;
            break;

        case 0x7000: // 7XNN: Adds NN to VX without setting VF
            data_registers[(opcode & 0x0F00) >> 8] += opcode & 0x00FF;
            program_offset += 2;
            break;

        case 0x8000:
            switch (opcode & 0x000F)
            {
                case 0x0000: // 8XY0: Sets VX to the value of VY
                    data_registers[(opcode & 0x0F00) >> 8] = data_registers[(opcode & 0x00F0) >> 4];
                    program_offset += 2;
                    break;

                case 0x0001: // 8XY1: Sets VX to VX or VY
                    data_registers[(opcode & 0x0F00) >> 8] |= data_registers[(opcode & 0x00F0) >> 4];
                    program_offset += 2;
                    break;

                case 0x0002: // 8XY2: Sets VX to VX and VY
                    data_registers[(opcode & 0x0F00) >> 8] &= data_registers[(opcode & 0x00F0) >> 4];
                    program_offset += 2;
                    break;

                case 0x0003: // 8XY3: Sets VX to VX xor VY
                    data_registers[(opcode & 0x0F00) >> 8] ^= data_registers[(opcode & 0x00F0) >> 4];
                    program_offset += 2;
                    break;

                case 0x0004: // 8XY4: Adds VY to VX and sets VF based on if there was a carry
                    data_registers[0xF] = (data_registers[(opcode & 0x0F00) >> 8] +
                            data_registers[(opcode & 0x00F0) >> 4] > 0xFF);
                    data_registers[(opcode & 0x0F00) >> 8] += data_registers[(opcode & 0x00F0) >> 4];
                    program_offset += 2;
                    break;

                case 0x0005: // 8XY5: Subtracts VY from VX and sets VF based on if there was a borrow
                    data_registers[0xF] = (data_registers[(opcode & 0x0F00) >> 8] >=
                            data_registers[(opcode & 0x00F0) >> 4]);
                    data_registers[(opcode & 0x0F00) >> 8] -= data_registers[(opcode & 0x00F0) >> 4];
                    program_offset += 2;
                    break;

                case 0x0006: // 8XY6: Shifts VX to the right by 1 and stores the least significant bit in VF
                    data_registers[0xF] = data_registers[(opcode & 0x0F00) >> 8] & 0x01;
                    data_registers[(opcode & 0x0F00) >> 8] >>= 1;
                    program_offset += 2;
                    break;

                case 0x0007: // 8XY7: Sets VX to VY minus VX and sets VF based on if there was a borrow
                    data_registers[0xF] = (data_registers[(opcode & 0x00F0) >> 4] >=
                            data_registers[(opcode & 0x0F00) >> 8]);
                    data_registers[(opcode & 0x0F00) >> 8] = data_registers[(opcode & 0x00F0) >> 4] -
                            data_registers[(opcode & 0x0F00) >> 8];
                    program_offset += 2;
                    break;

                case 0x000E: // 8XYE: Shifts VX to the left by 1 and stores the most significant bit in VF
                    data_registers[0xF] = data_registers[(opcode & 0x0F00) >> 8] >> 7;
                    data_registers[(opcode & 0x0F00) >> 8] <<= 1;
                    program_offset += 2;
                    break;

                default:
                    wmove(stdscr, 32, 0);
                    printw("Unknown opcode: 0x%X", opcode);
                    break;
            }
            break;

        case 0x9000: // 9XY0: Skips the next instruction if VX doesn't equal VY
            if (data_registers[(opcode & 0x0F00) >> 8] != data_registers[(opcode & 0x00F0) >> 4])
                program_offset += 2;
            program_offset += 2;
            break;

        case 0xA000: // ANNN: Sets I to NNN
            address_register = opcode & 0x0FFF;
            program_offset += 2;
            break;

        case 0xB000: // BNNN: Jumps to address NNN plus V0
            program_offset = (opcode & 0x0FFF) + data_registers[0];
            break;

        case 0xC000: // CXNN: Sets VX to the result of a random number and NN
            data_registers[(opcode & 0x0F00) >> 8] = (rand() % 255) & (opcode & 0x00FF);
            program_offset += 2;
            break;

        case 0xD000: // DXYN: Draws a sprite stored in VI at (VX, VY) with width 8 and height N
            data_registers[0xF] = 0;
            for (int y = 0; y < (opcode & 0x000F); y++) {
                uint8_t data = memory[address_register + y];
                for (int x = 0; x < 8; x++) {
                    if (data & (0x80 >> x)) {
                        bool *pixel = &display[((data_registers[(opcode & 0x00F0) >> 4] + y + 1) * 64 +
                                data_registers[(opcode & 0x0F00) >> 8] + x) % (64 * 32)];
                        *pixel = !(*pixel);
                        if (!(*pixel))
                            data_registers[0xF] = 1;
                    }
                }
            }

            program_offset += 2;
            break;

        case 0xE000:
            switch (opcode & 0x00FF)
            {
                case 0x009E: // EX9E: Skips the next instruction if the key stored in VX is pressed
                    if (keys[data_registers[(opcode & 0x0F00) >> 8]])
                        program_offset += 2;
                    program_offset += 2;
                    break;
                case 0x00A1: // EXA1: Skips the next instruction if the key stored in VX isn't pressed
                    if (!keys[data_registers[(opcode & 0x0F00) >> 8]])
                        program_offset += 2;
                    program_offset += 2;
                    break;
                default:
                    wmove(stdscr, 32, 0);
                    printw("Unknown opcode: 0x%X", opcode);
                    break;
            }
            break;

        case 0xF000:
            switch (opcode & 0x00FF)
            {
                case 0x0007: // FX07: Sets VX to the value of the delay timer
                    data_registers[(opcode & 0x0F00) >> 8] = delay_timer;
                    program_offset += 2;
                    break;

                case 0x000A: // FX0A: Waits for a key press and then stores the key in VX
                    data_registers[(opcode & 0x0F00) >> 8] = 16;
                    while (data_registers[(opcode & 0x0F00) >> 8] == 16) {
                        scan_input(false);
                        for (int i = 0; i < 16; i++) {
                            if (keys[i])
                                data_registers[(opcode & 0x0F00) >> 8] = i;
                        }
                    }
                    program_offset += 2;
                    break;

                case 0x0015: // FX15: Sets the delay timer to VX
                    delay_timer = data_registers[(opcode & 0x0F00) >> 8];
                    program_offset += 2;
                    break;

                case 0x0018: // FX18: Sets the sound timer to VX
                    sound_timer = data_registers[(opcode & 0x0F00) >> 8];
                    program_offset += 2;
                    break;

                case 0x001E: // FX1E: Adds VX to I
                    address_register += data_registers[(opcode & 0x0F00) >> 8];
                    program_offset += 2;
                    break;

                case 0x0029: // FX29: Sets I to the location of the sprite for the character in VX
                    address_register = 0x050 + data_registers[(opcode & 0x0F00) >> 8] * 5;
                    program_offset += 2;
                    break;

                case 0x0033: // FX33: Stores the binary-coded decimal representation of VX at VI
                    memory[address_register    ] = (data_registers[(opcode & 0x0F00) >> 8] / 100);
                    memory[address_register + 1] = (data_registers[(opcode & 0x0F00) >> 8] / 10) % 10;
                    memory[address_register + 2] = (data_registers[(opcode & 0x0F00) >> 8] % 100) % 10;
                    program_offset += 2;
                    break;

                case 0x0055: // FX55: Stores V0 to VX in memory starting at VI
                    memcpy(&memory[address_register], data_registers, ((opcode & 0x0F00) >> 8) + 1);
                    program_offset += 2;
                    break;

                case 0x0065: // FX65: Fills V0 to VX in memory with values starting at VI
                    memcpy(data_registers, &memory[address_register], ((opcode & 0x0F00) >> 8) + 1);
                    program_offset += 2;
                    break;

                default:
                    wmove(stdscr, 32, 0);
                    printw("Unknown opcode: 0x%X", opcode);
                    break;
            }
            break;

        default:
            wmove(stdscr, 32, 0);
            printw("Unknown opcode: 0x%X", opcode);
            break;
    }

    if (delay_timer > 0)
        delay_timer--;

    if (sound_timer > 0) {
        sound_timer--;
        if (sound_timer == 0)
            beep();
    }
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
    fread(&memory[program_offset], sizeof(uint8_t), romsize, rom);

    fclose(rom);

    // Load the fontset into memory
    memcpy(&memory[0x050], fontset, sizeof(fontset));

    initscr();
    noecho();
    nodelay(stdscr, true);

    while (true) {
        std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

        scan_input(true);

        run_cycle();

        // Draw the screen
        for (int y = 0; y < 32; y++) {
            for (int x = 0; x < 64; x++) {
                if (display[y * 64 + x])
                    attron(A_REVERSE);
                else
                    attroff(A_REVERSE);
                printw("  ");
            }
            wmove(stdscr, y, 0);
        }
        wrefresh(stdscr);

        std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed.count() < 1.0f / 60)
            usleep((1.0f / 60 - elapsed.count()) * 100000);
    }

    endwin();
    return 0;
}
