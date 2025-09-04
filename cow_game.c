#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#define WIDTH 70
#define HEIGHT 25
#define MAX_COWS 5
#define MAX_GRASS 25
#define MAX_LEADERBOARD 5
#define MAX_LIVES 3

// Game entities
typedef struct {
    int x, y;
    int dx, dy; // Direction for AI cows
} Cow;

typedef struct {
    int x, y;
    int eaten;
} Grass;

typedef struct {
    int score;
    char name[20];
} LeaderboardEntry;

// Global game state
char field[HEIGHT][WIDTH];
Cow player = {WIDTH/2, HEIGHT/2, 0, 0};
Cow ai_cows[MAX_COWS];
Grass grass[MAX_GRASS];
LeaderboardEntry leaderboard[MAX_LEADERBOARD];
int score = 0;
int lives = MAX_LIVES;
int game_over = 0;
int grass_count = MAX_GRASS;
int invincible_timer = 0; // Brief invincibility after getting hit

// Function prototypes
void initialize_game();
void draw_field();
void update_ai_cows();
void handle_input();
void check_collisions();
void spawn_grass();
void clear_screen();
int kbhit_custom();
char getch_custom();
void clear_input_buffer();
void load_leaderboard();
void save_leaderboard();
void update_leaderboard(int new_score);
void display_leaderboard();
void draw_cow_face();
void show_game_over_screen();

// Utility functions first
void clear_screen() {
    system("clear"); // Linux/Mac
}

int kbhit_custom() {
    struct termios oldt, newt;
    int ch;
    int oldf;
    
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
    
    ch = getchar();
    
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);
    
    if(ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }
    
    return 0;
}

char getch_custom() {
    struct termios oldt, newt;
    char ch;
    
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    
    ch = getchar();
    
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    
    return ch;
}

void clear_input_buffer() {
    // Clear any pending input to prevent it from interfering with subsequent reads
    while (kbhit_custom()) {
        getch_custom();
    }
    // Additional buffer clearing
    fflush(stdin);
}

void draw_cow_face() {
    printf("        /\\_/\\  /\\_/\\\n");
    printf("       (           )\n");
    printf("        \\ XX   XX /\n");
    printf("         |        |\n");
    printf("         |        |\n");
    printf("         | ( 0   0 ) |\n");
    printf("          \\      /\n");
    printf("           \\____/\n");
}

void load_leaderboard() {
    FILE *file = fopen("cow_scores.txt", "r");
    if (file == NULL) {
        // Initialize empty leaderboard
        for (int i = 0; i < MAX_LEADERBOARD; i++) {
            leaderboard[i].score = 0;
            strcpy(leaderboard[i].name, "Anonymous");
        }
        return;
    }
    
    for (int i = 0; i < MAX_LEADERBOARD; i++) {
        if (fscanf(file, "%d %s", &leaderboard[i].score, leaderboard[i].name) != 2) {
            leaderboard[i].score = 0;
            strcpy(leaderboard[i].name, "Anonymous");
        }
    }
    fclose(file);
}

void save_leaderboard() {
    FILE *file = fopen("cow_scores.txt", "w");
    if (file == NULL) return;
    
    for (int i = 0; i < MAX_LEADERBOARD; i++) {
        fprintf(file, "%d %s\n", leaderboard[i].score, leaderboard[i].name);
    }
    fclose(file);
}

void display_leaderboard() {
    printf("🏆 LEADERBOARD 🏆\n");
    printf("================\n");
    for (int i = 0; i < MAX_LEADERBOARD; i++) {
        if (leaderboard[i].score > 0) {
            printf("%d. %-15s %d pts\n", i+1, leaderboard[i].name, leaderboard[i].score);
        } else {
            printf("%d. %-15s ---\n", i+1, "Empty");
        }
    }
}

void update_leaderboard(int new_score) {
    // Check if score qualifies for leaderboard
    if (new_score <= leaderboard[MAX_LEADERBOARD-1].score) {
        return; // Score too low
    }
    
    printf("\n🎉 NEW HIGH SCORE! 🎉\n");
    printf("You made it to the leaderboard!\n\n");
    
    // Extra input clearing and pause before name entry
    clear_input_buffer();
    usleep(500000); // 0.5 second pause
    clear_input_buffer();
    
    printf("Enter your name (max 19 chars): ");
    fflush(stdout); // Make sure prompt is displayed
    
    // Clear input buffer one more time before reading name
    clear_input_buffer();
    
    char name[20];
    // Use a more controlled input method
    if (fgets(name, sizeof(name), stdin) != NULL) {
        // Remove newline if present
        size_t len = strlen(name);
        if (len > 0 && name[len-1] == '\n') {
            name[len-1] = '\0';
        }
        if (strlen(name) == 0) {
            strcpy(name, "Anonymous");
        }
    } else {
        strcpy(name, "Anonymous");
    }
    
    // Insert new score in correct position
    int insert_pos = MAX_LEADERBOARD;
    for (int i = 0; i < MAX_LEADERBOARD; i++) {
        if (new_score > leaderboard[i].score) {
            insert_pos = i;
            break;
        }
    }
    
    // Shift scores down
    for (int i = MAX_LEADERBOARD-1; i > insert_pos; i--) {
        leaderboard[i] = leaderboard[i-1];
    }
    
    // Insert new score
    if (insert_pos < MAX_LEADERBOARD) {
        leaderboard[insert_pos].score = new_score;
        strcpy(leaderboard[insert_pos].name, name);
    }
    
    save_leaderboard();
    
    printf("\nCongratulations! Your score has been saved!\n");
    usleep(1000000); // 1 second pause to let them see the message
}

void show_game_over_screen() {
    clear_screen();
    printf("\n\n\n");
    
    // Large "GAME OVER" text
    printf("     ██████   █████  ███    ███ ███████      ██████  ██    ██ ███████ ██████  \n");
    printf("    ██       ██   ██ ████  ████ ██          ██    ██ ██    ██ ██      ██   ██ \n");
    printf("    ██   ███ ███████ ██ ████ ██ █████       ██    ██ ██    ██ █████   ██████  \n");
    printf("    ██    ██ ██   ██ ██  ██  ██ ██          ██    ██  ██  ██  ██      ██   ██ \n");
    printf("     ██████  ██   ██ ██      ██ ███████      ██████    ████   ███████ ██   ██ \n");
    
    printf("\n\n");
    
    // Show the cow face
    draw_cow_face();
    
    printf("\n");
    printf("                        You ran out of lives!\n");
    printf("                     The black cows got you...\n");
    printf("\n");
    printf("                    Press any key to continue...\n");
    
    // Wait for any key press
    getch_custom();
}

void initialize_game() {
    // Initialize player in lower left corner (7-char wide, 3 lines tall)
    player.x = 3; // Near left edge but away from fence
    player.y = HEIGHT - 6; // Near bottom but away from fence and accounting for cow height
    
    // Reset lives
    lives = MAX_LIVES;
    invincible_timer = 0;
    
    // Initialize AI cows
    for (int i = 0; i < MAX_COWS; i++) {
        ai_cows[i].x = (rand() % (WIDTH - 14)) + 7; // More space for larger cows
        ai_cows[i].y = (rand() % (HEIGHT - 8)) + 4;
        ai_cows[i].dx = (rand() % 3) - 1; // -1, 0, or 1
        ai_cows[i].dy = (rand() % 3) - 1;
        
        // Make sure AI cows don't start too close to player
        while (abs(ai_cows[i].x - player.x) < 12 && abs(ai_cows[i].y - player.y) < 6) {
            ai_cows[i].x = (rand() % (WIDTH - 14)) + 7;
            ai_cows[i].y = (rand() % (HEIGHT - 8)) + 4;
        }
    }
    
    // Initialize grass (3-char wide)
    for (int i = 0; i < MAX_GRASS; i++) {
        grass[i].x = (rand() % (WIDTH - 8)) + 4;
        grass[i].y = (rand() % (HEIGHT - 6)) + 3;
        grass[i].eaten = 0;
    }
}

void draw_field() {
    // Clear the field
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            field[y][x] = ' ';
        }
    }
    
    // Draw fence
    for (int x = 0; x < WIDTH; x++) {
        field[0][x] = '#';
        field[HEIGHT-1][x] = '#';
    }
    for (int y = 0; y < HEIGHT; y++) {
        field[y][0] = '#';
        field[y][WIDTH-1] = '#';
    }
    
    // Draw grass patches
    for (int i = 0; i < MAX_GRASS; i++) {
        if (!grass[i].eaten && grass[i].x > 0 && grass[i].x < WIDTH-3) {
            field[grass[i].y][grass[i].x] = 'v';
            field[grass[i].y][grass[i].x+1] = 'v';
            field[grass[i].y][grass[i].x+2] = 'v';
        }
    }
    
    // Draw AI cows (all black ASCII art cows)
    for (int i = 0; i < MAX_COWS; i++) {
        if (ai_cows[i].x > 0 && ai_cows[i].x < WIDTH-7 && ai_cows[i].y > 0 && ai_cows[i].y < HEIGHT-4) {
            // Line 1: head
            field[ai_cows[i].y][ai_cows[i].x] = ' ';
            field[ai_cows[i].y][ai_cows[i].x+1] = '^';
            field[ai_cows[i].y][ai_cows[i].x+2] = '_';
            field[ai_cows[i].y][ai_cows[i].x+3] = '_';
            field[ai_cows[i].y][ai_cows[i].x+4] = '^';
            
            // Line 2: face and body
            field[ai_cows[i].y+1][ai_cows[i].x] = '(';
            field[ai_cows[i].y+1][ai_cows[i].x+1] = '@';
            field[ai_cows[i].y+1][ai_cows[i].x+2] = '@';
            field[ai_cows[i].y+1][ai_cows[i].x+3] = ')';
            field[ai_cows[i].y+1][ai_cows[i].x+4] = '\\';
            field[ai_cows[i].y+1][ai_cows[i].x+5] = '_';
            
            // Line 3: legs
            field[ai_cows[i].y+2][ai_cows[i].x+1] = '|';
            field[ai_cows[i].y+2][ai_cows[i].x+2] = '|';
            field[ai_cows[i].y+2][ai_cows[i].x+3] = '-';
            field[ai_cows[i].y+2][ai_cows[i].x+4] = '-';
            field[ai_cows[i].y+2][ai_cows[i].x+5] = 'w';
            field[ai_cows[i].y+2][ai_cows[i].x+6] = '|';
        }
    }
    
    // Draw player cow (black and white ASCII art cow) - blink if invincible
    if (player.x > 0 && player.x < WIDTH-7 && player.y > 0 && player.y < HEIGHT-4 && 
        (invincible_timer == 0 || invincible_timer % 8 < 4)) { // Blink effect during invincibility
        // Line 1: head
        field[player.y][player.x] = ' ';
        field[player.y][player.x+1] = '^';
        field[player.y][player.x+2] = '_';
        field[player.y][player.x+3] = '_';
        field[player.y][player.x+4] = '^';
        
        // Line 2: face and body (black and white)
        field[player.y+1][player.x] = '(';
        field[player.y+1][player.x+1] = '@';  // black eye
        field[player.y+1][player.x+2] = 'O';  // white eye
        field[player.y+1][player.x+3] = ')';
        field[player.y+1][player.x+4] = '\\';
        field[player.y+1][player.x+5] = '_';
        
        // Line 3: legs
        field[player.y+2][player.x+1] = '|';
        field[player.y+2][player.x+2] = '|';
        field[player.y+2][player.x+3] = '-';
        field[player.y+2][player.x+4] = '-';
        field[player.y+2][player.x+5] = 'w';
        field[player.y+2][player.x+6] = '|';
    }
    
    // Print the field
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            printf("%c", field[y][x]);
        }
        printf("\n");
    }
}

void update_ai_cows() {
    // Slow down AI cows - they only move every 4th update (much slower)
    static int move_counter = 0;
    move_counter++;
    if (move_counter < 4) return;  // Changed from 2 to 4 for slower movement
    move_counter = 0;
    
    for (int i = 0; i < MAX_COWS; i++) {
        // Random direction change occasionally (less frequent)
        if (rand() % 20 == 0) {  // Changed from 15 to 20 for less frequent direction changes
            ai_cows[i].dx = (rand() % 3) - 1;
            ai_cows[i].dy = (rand() % 3) - 1;
        }
        
        // Calculate new position
        int new_x = ai_cows[i].x + (ai_cows[i].dx * 2); // Move by 2 instead of 3 for slower movement
        int new_y = ai_cows[i].y + ai_cows[i].dy;
        
        // Check boundaries and reverse direction if hitting fence
        if (new_x <= 0 || new_x >= WIDTH-8) { // Account for 7-char width
            ai_cows[i].dx = -ai_cows[i].dx;
            new_x = ai_cows[i].x + (ai_cows[i].dx * 2);
        }
        if (new_y <= 0 || new_y >= HEIGHT-4) { // Account for 3-line height
            ai_cows[i].dy = -ai_cows[i].dy;
            new_y = ai_cows[i].y + ai_cows[i].dy;
        }
        
        // Update position
        ai_cows[i].x = new_x;
        ai_cows[i].y = new_y;
    }
}

void handle_input() {
    char key = getch_custom();
    int new_x = player.x;
    int new_y = player.y;
    
    // Handle arrow keys (escape sequences)
    if (key == 27) { // ESC character
        char seq1 = getch_custom();
        if (seq1 == '[') {
            char seq2 = getch_custom();
            switch (seq2) {
                case 'A': // Up arrow
                    new_y--;
                    break;
                case 'B': // Down arrow
                    new_y++;
                    break;
                case 'D': // Left arrow
                    new_x -= 2; // Move by 2 since cow is 2 chars wide
                    break;
                case 'C': // Right arrow
                    new_x += 2; // Move by 2 since cow is 2 chars wide
                    break;
                default:
                    // If it's just ESC without arrow sequence, quit
                    if (seq2 == 27) {
                        game_over = 1;
                    }
                    return;
            }
        } else {
            // Just ESC pressed, quit game
            game_over = 1;
            return;
        }
    } else {
        // Handle regular keys
        switch (key) {
            case 'w': case 'W': // Up
                new_y--;
                break;
            case 's': case 'S': // Down
                new_y++;
                break;
            case 'a': case 'A': // Left
                new_x -= 3;
                break;
            case 'd': case 'D': // Right
                new_x += 3;
                break;
            case ' ': // Space bar - eat grass
                // Check for grass at player position (7-char wide, 3-lines tall)
                for (int i = 0; i < MAX_GRASS; i++) {
                    if (!grass[i].eaten && 
                        grass[i].x >= player.x-3 && grass[i].x <= player.x+6 && 
                        grass[i].y >= player.y && grass[i].y <= player.y+2) {
                        grass[i].eaten = 1;
                        score += 10;
                        grass_count--;
                        break;
                    }
                }
                return;
            case 'q': case 'Q': // Alternative quit key
                game_over = 1;
                return;
            default:
                return;
        }
    }
    
    // Check boundaries (account for 7-char wide, 3-line tall cow)
    if (new_x > 0 && new_x < WIDTH-8 && new_y > 0 && new_y < HEIGHT-4) {
        player.x = new_x;
        player.y = new_y;
    }
}

void check_collisions() {
    // Skip collision detection if invincible
    if (invincible_timer > 0) {
        return;
    }
    
    // Check collision with AI cows (both are 7-char wide, 3-lines tall)
    for (int i = 0; i < MAX_COWS; i++) {
        // Check if the cow sprites overlap
        if (abs(player.y - ai_cows[i].y) <= 2 && // Vertical overlap (3 lines tall)
            abs(player.x - ai_cows[i].x) <= 6) { // Horizontal overlap (7 chars wide)
            
            lives--; // Lose a life
            invincible_timer = 60; // 60 frames of invincibility (about 5 seconds at 80ms per frame)
            
            if (lives <= 0) {
                game_over = 1;
            }
            return;
        }
    }
    
    // Check if all grass is eaten (win condition)
    if (grass_count == 0) {
        score += 100; // Bonus for clearing all grass
        // Respawn grass for continued play
        for (int i = 0; i < MAX_GRASS; i++) {
            grass[i].eaten = 0;
            grass[i].x = (rand() % (WIDTH - 8)) + 4;
            grass[i].y = (rand() % (HEIGHT - 6)) + 3;
        }
        grass_count = MAX_GRASS;
    }
}

void spawn_grass() {
    // More frequently spawn new grass since we have more grass now
    if (rand() % 30 == 0 && grass_count < MAX_GRASS) {
        for (int i = 0; i < MAX_GRASS; i++) {
            if (grass[i].eaten) {
                grass[i].x = (rand() % (WIDTH - 8)) + 4;
                grass[i].y = (rand() % (HEIGHT - 6)) + 3;
                grass[i].eaten = 0;
                grass_count++;
                break;
            }
        }
    }
}

int main() {
    srand(time(NULL));
    load_leaderboard();
    initialize_game();
    
    printf("=== ASCII COW PASTURE GAME ===\n\n");
    draw_cow_face();
    printf("\n");
    printf("Your black & white cow starts in the lower left corner!\n");
    printf("Move with arrow keys or WASD:\n");
    printf("  ^__^\n");
    printf(" (@O)\\_\n");
    printf("  ||--w|\n");
    printf("Press SPACE to eat grass (vvv)\n");
    printf("Avoid the all-black cows - you have 3 lives!\n");
    printf("Press ESC or Q to quit\n\n");
    
    display_leaderboard();
    printf("\nPress any key to start...\n");
    getch_custom();
    getch_custom();
    
    while (!game_over) {
        clear_screen();
        draw_field();
        printf("Score: %d | Grass eaten: %d | Lives: %d\n", score, MAX_GRASS - grass_count, lives);
        
        if (kbhit_custom()) {
            handle_input();
        }
        
        update_ai_cows();
        check_collisions();
        spawn_grass();
        
        // Decrease invincibility timer
        if (invincible_timer > 0) {
            invincible_timer--;
        }
        
        usleep(80000); // Game speed (80ms)
    }
    
    // Clear any remaining input before game over screen
    clear_input_buffer();
    
    // Show game over screen
    show_game_over_screen();
    
    // Clear input again before leaderboard
    clear_input_buffer();
    
    // Show leaderboard and handle high score entry
    clear_screen();
    printf("\n");
    draw_cow_face();
    printf("\n");
    printf("FINAL RESULTS\n");
    printf("=============\n");
    printf("Score: %d\n", score);
    printf("Grass eaten: %d\n", MAX_GRASS - grass_count);
    
    // Update leaderboard
    update_leaderboard(score);
    printf("\n");
    display_leaderboard();
    printf("\n");
    draw_cow_face();
    printf("\n");
    printf("Press any key to exit...\n");
    getch_custom();
    
    return 0;
}
