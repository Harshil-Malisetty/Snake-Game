#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>

// Original grid dimensions
#define CELL_SIZE 20
#define GRID_WIDTH 32  // 640 / 20
#define GRID_HEIGHT 24 // 480 / 20

// UI dimensions
#define UI_HEIGHT 60  // Height of the UI area above the grid
#define UI_PADDING 10 // Padding inside UI area

// New window dimensions
#define WINDOW_WIDTH (GRID_WIDTH * CELL_SIZE)
#define WINDOW_HEIGHT (GRID_HEIGHT * CELL_SIZE + UI_HEIGHT)

// Score display constants
#define SCORE_DIGIT_WIDTH 10
#define SCORE_DIGIT_HEIGHT 20
#define SCORE_PADDING 5
#define SCORE_SEGMENT_THICKNESS 3

// Button dimensions
#define BUTTON_WIDTH 200
#define BUTTON_HEIGHT 50
#define BUTTON_PADDING 20

// Highscore file name
#define HIGHSCORE_FILE "highscore.dat"

// Game duration in milliseconds (2 minutes)
#define GAME_DURATION 120000 // 2 minutes in milliseconds

// Define the number of fruits that should be present
#define FRUIT_COUNT 5

Mix_Chunk *obstacle_hit_sound = NULL;
SDL_Texture *appleTexture = NULL;  // Global variable for the apple texture

// Game states
typedef enum {
    MENU,
    PLAYING,
    GAME_OVER
} GameState;

typedef struct {
    int x, y;
} Segment;

typedef struct {
    Segment body[100];
    int length;
    int dx, dy;
    bool alive;
    int score;
    SDL_Color color;
    char name[10];
} Snake;

typedef struct {
    int x, y;
    bool active;
} Food;

typedef struct {
    SDL_Rect rect;
    char text[20];
    bool hover;
} Button;

// Function prototypes
void draw_grid(SDL_Renderer *renderer);
void draw_snake(SDL_Renderer *renderer, Snake *snake);
void draw_foods(SDL_Renderer *renderer, Food foods[], int count, SDL_Texture *apple_texture);


void draw_segment(SDL_Renderer *renderer, int x, int y, char segment, int width, int height, int thickness);
void draw_digit(SDL_Renderer *renderer, int x, int y, int digit, int width, int height, int thickness);
void draw_score(SDL_Renderer *renderer, Snake *snakeA, Snake *snakeB, int time_left, TTF_Font *font);
void move_snake(Snake *snake, Snake *other_snake);
bool check_food_collision(Snake *snake, Food *food, Mix_Chunk *apple_eat_sound);

void place_food(Food *food, Snake *snakeA, Snake *snakeB);
void ensure_minimum_fruits(Food foods[], int count, Snake *snakeA, Snake *snakeB);
void grow_snake(Snake *snake);
void init_button(Button *button, int x, int y, const char *text);
void draw_button(SDL_Renderer *renderer, Button *button, TTF_Font *font);
bool is_point_in_rect(int x, int y, SDL_Rect *rect);
void draw_text(SDL_Renderer *renderer, TTF_Font *font, const char *text, int x, int y, SDL_Color color);
void draw_text_centered(SDL_Renderer *renderer, TTF_Font *font, const char *text, int x, int y, SDL_Color color);
void draw_welcome_screen(SDL_Renderer *renderer, Button *playButton, TTF_Font *font);
void draw_game_over_screen(SDL_Renderer *renderer, Snake *snakeA, Snake *snakeB, Button *playAgainButton, Button *exitButton, TTF_Font *font);
void reset_game(Snake *snakeA, Snake *snakeB, Food foods[], int count);
void draw_ui_area(SDL_Renderer *renderer, Snake *snakeA, Snake *snakeB, int time_left, TTF_Font *font);
void format_time(int milliseconds, char *buffer);

// Main function remains at the bottom

void draw_grid(SDL_Renderer *renderer) {
    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);

    // Draw grid inside the game area only
    for (int x = 0; x <= WINDOW_WIDTH; x += CELL_SIZE) {
        SDL_RenderDrawLine(renderer, x, UI_HEIGHT, x, WINDOW_HEIGHT);
    }

    for (int y = UI_HEIGHT; y <= WINDOW_HEIGHT; y += CELL_SIZE) {
        SDL_RenderDrawLine(renderer, 0, y, WINDOW_WIDTH, y);
    }

    // Draw a more prominent border around the grid
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
    SDL_Rect border = {0, UI_HEIGHT, WINDOW_WIDTH, WINDOW_HEIGHT - UI_HEIGHT};
    SDL_RenderDrawRect(renderer, &border);
}

void drawCircle(SDL_Renderer *renderer, int x, int y, int radius) {
    for (int w = 0; w < radius * 2; w++) {
        for (int h = 0; h < radius * 2; h++) {
            int dx = radius - w;
            int dy = radius - h;
            if ((dx * dx + dy * dy) <= (radius * radius)) {
                SDL_RenderDrawPoint(renderer, x + dx, y + dy);
            }
        }
    }
}

void draw_snake(SDL_Renderer *renderer, Snake *snake) {
    if (!snake->alive) return;  // Don't draw dead snakes

    int radius = CELL_SIZE / 2; // Half of cell size for circular appearance

    // Draw body segments in slightly darker shade
    SDL_SetRenderDrawColor(renderer,
                          snake->color.r * 0.8,
                          snake->color.g * 0.8,
                          snake->color.b * 0.8,
                          255);
    for (int i = 1; i < snake->length; i++) {
        int x = snake->body[i].x * CELL_SIZE + radius;
        int y = snake->body[i].y * CELL_SIZE + UI_HEIGHT + radius;
        drawCircle(renderer, x, y, radius);
    }

    // Draw head in the original color
    SDL_SetRenderDrawColor(renderer,
                          snake->color.r,
                          snake->color.g,
                          snake->color.b,
                          255);
    int head_x = snake->body[0].x * CELL_SIZE + radius;
    int head_y = snake->body[0].y * CELL_SIZE + UI_HEIGHT + radius;
    drawCircle(renderer, head_x, head_y, radius);

    // Draw eyes (small white circles)
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    int eye_offset_x = radius / 2;  // Horizontal spacing of the eyes
    int eye_offset_y = radius / 3;  // Vertical spacing
    int eye_radius = radius / 4;

    int left_eye_x = head_x - eye_offset_x;
    int right_eye_x = head_x + eye_offset_x;
    int eye_y = head_y - eye_offset_y;

    drawCircle(renderer, left_eye_x, eye_y, eye_radius);  // Left eye
    drawCircle(renderer, right_eye_x, eye_y, eye_radius); // Right eye

    // Draw pupils (small black dots)
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    int pupil_radius = eye_radius / 2;
    drawCircle(renderer, left_eye_x, eye_y, pupil_radius);  // Left pupil
    drawCircle(renderer, right_eye_x, eye_y, pupil_radius); // Right pupil
}


// Modified to draw multiple foods
void draw_foods(SDL_Renderer *renderer, Food foods[], int count, SDL_Texture *apple_texture) {
    for (int i = 0; i < count; i++) {
        if (foods[i].active) {
            SDL_Rect rect = {
                foods[i].x * CELL_SIZE,
                foods[i].y * CELL_SIZE + UI_HEIGHT,
                CELL_SIZE,
                CELL_SIZE
            };
            SDL_RenderCopy(renderer, apple_texture, NULL, &rect);
        }
    }
}


// Function to draw a digit segment for the score display
void draw_segment(SDL_Renderer *renderer, int x, int y, char segment, int width, int height, int thickness) {
    SDL_Rect rect;

    switch(segment) {
        case 'a': // Top horizontal
            rect = (SDL_Rect){x, y, width, thickness};
            break;
        case 'b': // Top-right vertical
            rect = (SDL_Rect){x + width - thickness, y, thickness, height / 2};
            break;
        case 'c': // Bottom-right vertical
            rect = (SDL_Rect){x + width - thickness, y + height / 2, thickness, height / 2};
            break;
        case 'd': // Bottom horizontal
            rect = (SDL_Rect){x, y + height - thickness, width, thickness};
            break;
        case 'e': // Bottom-left vertical
            rect = (SDL_Rect){x, y + height / 2, thickness, height / 2};
            break;
        case 'f': // Top-left vertical
            rect = (SDL_Rect){x, y, thickness, height / 2};
            break;
        case 'g': // Middle horizontal
            rect = (SDL_Rect){x, y + height / 2 - thickness / 2, width, thickness};
            break;
        default:
            return;
    }

    SDL_RenderFillRect(renderer, &rect);
}

// Function to draw a digit (0-9) for the score display
void draw_digit(SDL_Renderer *renderer, int x, int y, int digit, int width, int height, int thickness) {
    // Define which segments to light up for each digit (a-g)
    const char* segments[] = {
        "abcdef",  // 0
        "bc",      // 1
        "abged",   // 2
        "abgcd",   // 3
        "fbgc",    // 4
        "afgcd",   // 5
        "afgcde",  // 6
        "abc",     // 7
        "abcdefg", // 8
        "abfgcd"   // 9
    };

    if (digit < 0 || digit > 9) return;

    const char* active_segments = segments[digit];
    size_t len = strlen(active_segments);

    for (size_t i = 0; i < len; i++) {
        draw_segment(renderer, x, y, active_segments[i], width, height, thickness);
    }
}

// Format time in MM:SS format
void format_time(int milliseconds, char *buffer) {
    int seconds = milliseconds / 1000;
    int minutes = seconds / 60;
    seconds %= 60;
    sprintf(buffer, "%02d:%02d", minutes, seconds);
}

// Function to draw the UI area with scores and timer
void draw_ui_area(SDL_Renderer *renderer, Snake *snakeA, Snake *snakeB, int time_left, TTF_Font *font) {
    // Background for UI area
    SDL_SetRenderDrawColor(renderer, 30, 30, 40, 255);
    SDL_Rect ui_rect = {0, 0, WINDOW_WIDTH, UI_HEIGHT};
    SDL_RenderFillRect(renderer, &ui_rect);

    // Draw a border between UI area and game grid
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
    SDL_RenderDrawLine(renderer, 0, UI_HEIGHT, WINDOW_WIDTH, UI_HEIGHT);

    // Player A score
    char scoreA_text[32];
    sprintf(scoreA_text, "PLAYER A: %d", snakeA->score);

    SDL_Color playerA_color = snakeA->color;
    draw_text(renderer, font, scoreA_text, UI_PADDING, UI_HEIGHT / 2 - 10, playerA_color);

    // Timer in the middle
    char time_text[32];
    format_time(time_left, time_text);

    SDL_Color white = {255, 255, 255, 255};

    // Calculate position for timer (centered)
    SDL_Surface *surface = TTF_RenderText_Solid(font, time_text, white);
    int timer_x = WINDOW_WIDTH / 2 - surface->w / 2;
    SDL_FreeSurface(surface);

    draw_text(renderer, font, time_text, timer_x, UI_HEIGHT / 2 - 10, white);

    // Player B score
    char scoreB_text[32];
    sprintf(scoreB_text, "PLAYER B: %d", snakeB->score);

    SDL_Color playerB_color = snakeB->color;

    // Calculate position for Player B score (right-aligned)
    surface = TTF_RenderText_Solid(font, scoreB_text, playerB_color);
    int scoreB_x = WINDOW_WIDTH - UI_PADDING - surface->w;
    SDL_FreeSurface(surface);

    draw_text(renderer, font, scoreB_text, scoreB_x, UI_HEIGHT / 2 - 10, playerB_color);
}

// Modified score function now displays both players' scores and the timer
void draw_score(SDL_Renderer *renderer, Snake *snakeA, Snake *snakeB, int time_left, TTF_Font *font) {
    draw_ui_area(renderer, snakeA, snakeB, time_left, font);
}

void move_snake(Snake *snake, Snake *other_snake) {
    if (!snake->alive) return;  // Don't move dead snakes

    // Move body segments
    for (int i = snake->length - 1; i > 0; i--) {
        snake->body[i] = snake->body[i - 1];
    }

    // Move head
    snake->body[0].x += snake->dx;
    snake->body[0].y += snake->dy;

    // Check wall collision
// Check wall collision
    if (snake->body[0].x < 0 || snake->body[0].x >= GRID_WIDTH ||
        snake->body[0].y < 0 || snake->body[0].y >= GRID_HEIGHT) {
            Mix_PlayChannel(-1, obstacle_hit_sound, 0);  // Play sound on collision
        snake->alive = false;
        return;
    }


    // Check self collision
    for (int i = 1; i < snake->length; i++) {
        if (snake->body[0].x == snake->body[i].x && snake->body[0].y == snake->body[i].y) {
            Mix_PlayChannel(-1, obstacle_hit_sound, 0);
            snake->alive = false;
            return;
        }
    }

    // Check collision with other snake
    if (other_snake->alive) {
        for (int i = 0; i < other_snake->length; i++) {
            if (snake->body[0].x == other_snake->body[i].x &&
                snake->body[0].y == other_snake->body[i].y) {
                Mix_PlayChannel(-1, obstacle_hit_sound, 0);
                snake->alive = false;
                return;
            }
        }
    }
}



bool check_food_collision(Snake *snake, Food *food, Mix_Chunk *apple_eat_sound) {
    if (snake->alive && food->active && snake->body[0].x == food->x && snake->body[0].y == food->y) {
        Mix_PlayChannel(-1, apple_eat_sound, 0);  // Play apple_eat sound
        return true;
    }
    return false;
}


void place_food(Food *food, Snake *snakeA, Snake *snakeB) {
    bool valid_position;

    do {
        valid_position = true;
        food->x = rand() % GRID_WIDTH;
        food->y = rand() % GRID_HEIGHT;

        // Check if food is not on snake A
        for (int i = 0; i < snakeA->length; i++) {
            if (food->x == snakeA->body[i].x && food->y == snakeA->body[i].y) {
                valid_position = false;
                break;
            }
        }

        // Check if food is not on snake B
        if (valid_position) {
            for (int i = 0; i < snakeB->length; i++) {
                if (food->x == snakeB->body[i].x && food->y == snakeB->body[i].y) {
                    valid_position = false;
                    break;
                }
            }
        }
    } while (!valid_position);

    food->active = true;
}

// New function to ensure minimum number of fruits are present
// New function to ensure minimum number of fruits are present
void ensure_minimum_fruits(Food foods[], int count, Snake *snakeA, Snake *snakeB) {
    int active_count = 0;

    // Count active fruits
    for (int i = 0; i < count; i++) {
        if (foods[i].active) {
            active_count++;
        }
    }

    // If more than 2 exist, deactivate extras
    while (active_count > 2) {
        for (int i = 0; i < count && active_count > 2; i++) {
            if (foods[i].active) {
                foods[i].active = 0; // Remove extra fruits
                active_count--;
            }
        }
    }

    // If less than 2, spawn new ones
    while (active_count < 2) {
        for (int i = 0; i < count && active_count < 2; i++) {
            if (!foods[i].active) {
                place_food(&foods[i], snakeA, snakeB);
                foods[i].active = 1;
                active_count++;
            }
        }
    }
}


void grow_snake(Snake *snake) {
    // Add a new segment at the current tail position
    // This will be moved on the next frame
    snake->body[snake->length] = snake->body[snake->length - 1];
    snake->length++;
}

void init_button(Button *button, int x, int y, const char *text) {
    button->rect.x = x;
    button->rect.y = y;
    button->rect.w = BUTTON_WIDTH;
    button->rect.h = BUTTON_HEIGHT;
    strncpy(button->text, text, sizeof(button->text) - 1);
    button->text[sizeof(button->text) - 1] = '\0'; // Ensure null-termination
    button->hover = false;
}

void draw_button(SDL_Renderer *renderer, Button *button, TTF_Font *font) {
    // Draw button background
    if (button->hover) {
        SDL_SetRenderDrawColor(renderer, 100, 100, 200, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 70, 70, 150, 255);
    }
    SDL_RenderFillRect(renderer, &button->rect);

    // Draw button border
    SDL_SetRenderDrawColor(renderer, 150, 150, 200, 255);
    SDL_RenderDrawRect(renderer, &button->rect);

    // Draw button text
    SDL_Color text_color = {255, 255, 255, 255};
    draw_text_centered(renderer, font, button->text,
                      button->rect.x + button->rect.w / 2,
                      button->rect.y + button->rect.h / 2,
                      text_color);
}

bool is_point_in_rect(int x, int y, SDL_Rect *rect) {
    return (x >= rect->x && x < rect->x + rect->w &&
            y >= rect->y && y < rect->y + rect->h);
}

void draw_text(SDL_Renderer *renderer, TTF_Font *font, const char *text, int x, int y, SDL_Color color) {
    SDL_Surface *surface = TTF_RenderText_Solid(font, text, color);
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);

    SDL_Rect dest = {x, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, NULL, &dest);

    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

void draw_text_centered(SDL_Renderer *renderer, TTF_Font *font, const char *text, int x, int y, SDL_Color color) {
    SDL_Surface *surface = TTF_RenderText_Solid(font, text, color);
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);

    SDL_Rect dest = {x - surface->w / 2, y - surface->h / 2, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, NULL, &dest);

    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

void draw_welcome_screen(SDL_Renderer *renderer, Button *playButton, TTF_Font *font) {
    // Draw background
    SDL_SetRenderDrawColor(renderer, 20, 20, 30, 255);
    SDL_RenderClear(renderer);

    // Draw title
    SDL_Color title_color = {255, 255, 100, 255};
    draw_text_centered(renderer, font, "SNAKE BATTLE", WINDOW_WIDTH / 2, 100, title_color);

    // Draw instructions
    SDL_Color text_color = {200, 200, 200, 255};
    draw_text_centered(renderer, font, "Player A: WASD to move", WINDOW_WIDTH / 2, 180, text_color);
    draw_text_centered(renderer, font, "Player B: Arrow keys to move", WINDOW_WIDTH / 2, 210, text_color);
    draw_text_centered(renderer, font, "Game time: 2 minutes", WINDOW_WIDTH / 2, 240, text_color);
    draw_text_centered(renderer, font, "Collect fruits to score points", WINDOW_WIDTH / 2, 270, text_color);
    draw_text_centered(renderer, font, "Avoid walls and other snakes", WINDOW_WIDTH / 2, 300, text_color);

    // Draw play button
    draw_button(renderer, playButton, font);
}

void draw_game_over_screen(SDL_Renderer *renderer, Snake *snakeA, Snake *snakeB, Button *playAgainButton, Button *exitButton, TTF_Font *font) {
    // Draw semi-transparent overlay
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
    SDL_Rect overlay = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
    SDL_RenderFillRect(renderer, &overlay);

    // Draw game over text
    SDL_Color title_color = {255, 100, 100, 255};
    draw_text_centered(renderer, font, "GAME OVER", WINDOW_WIDTH / 2, 100, title_color);

    // Draw scores
    SDL_Color text_color = {255, 255, 255, 255};
    char score_text[100];
    sprintf(score_text, "Player A: %d", snakeA->score);
    draw_text_centered(renderer, font, score_text, WINDOW_WIDTH / 2, 150, snakeA->color);

    sprintf(score_text, "Player B: %d", snakeB->score);
    draw_text_centered(renderer, font, score_text, WINDOW_WIDTH / 2, 180, snakeB->color);

    // Draw winner
    if (snakeA->score > snakeB->score) {
        draw_text_centered(renderer, font, "Player A Wins!", WINDOW_WIDTH / 2, 230, snakeA->color);
    } else if (snakeB->score > snakeA->score) {
        draw_text_centered(renderer, font, "Player B Wins!", WINDOW_WIDTH / 2, 230, snakeB->color);
    } else {
        draw_text_centered(renderer, font, "It's a Draw!", WINDOW_WIDTH / 2, 230, text_color);
    }

    // Draw buttons
    draw_button(renderer, playAgainButton, font);
    draw_button(renderer, exitButton, font);
}

void reset_game(Snake *snakeA, Snake *snakeB, Food foods[], int count) {
    // Reset Snake A
    snakeA->length = 3;
    snakeA->dx = 1;
    snakeA->dy = 0;
    snakeA->alive = true;
    snakeA->score = 0;

    // Position Snake A at the left side of the grid
    for (int i = 0; i < snakeA->length; i++) {
        snakeA->body[i].x = 5 - i;
        snakeA->body[i].y = 5;
    }

    // Reset Snake B
    snakeB->length = 3;
    snakeB->dx = -1;
    snakeB->dy = 0;
    snakeB->alive = true;
    snakeB->score = 0;

    // Position Snake B at the right side of the grid
    for (int i = 0; i < snakeB->length; i++) {
        snakeB->body[i].x = GRID_WIDTH - 6 + i;
        snakeB->body[i].y = GRID_HEIGHT - 6;
    }

    // Reset all foods
    for (int i = 0; i < count; i++) {
        foods[i].active = false;
    }

    // Place initial fruits
    ensure_minimum_fruits(foods, count, snakeA, snakeB);
}

int main(int argc, char *argv[]) {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    // Initialize SDL_ttf
    if (TTF_Init() == -1) {
        printf("SDL_ttf could not initialize! SDL_ttf Error: %s\n", TTF_GetError());
        return 1;
    }

    // Create window
    SDL_Window *window = SDL_CreateWindow("Multiplayer Snake Game",
                                         SDL_WINDOWPOS_UNDEFINED,
                                         SDL_WINDOWPOS_UNDEFINED,
                                         WINDOW_WIDTH,
                                         WINDOW_HEIGHT,
                                         SDL_WINDOW_SHOWN);
    if (window == NULL) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }




    // Create renderer
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Texture *apple_texture = IMG_LoadTexture(renderer, "apple.png");
    if (!apple_texture) {
        printf("Failed to load apple texture: %s\n", SDL_GetError());
        return 1; // Handle the error
        }


    // Load font
    TTF_Font *font = TTF_OpenFont("font.ttf", 24);
    if (font == NULL) {
        printf("Failed to load font! SDL_ttf Error: %s\n", TTF_GetError());
        // Try to load default font if first attempt fails
        font = TTF_OpenFont("dejavu-fonts-ttf-2.37/ttf/DejaVuSans.ttf", 24);
        if (font == NULL) {
            printf("Failed to load default font! SDL_ttf Error: %s\n", TTF_GetError());
            return 1;
        }
    }

    // Seed random number generator
    srand(time(NULL));

    // Initialize Snake A (WASD controls)
    Snake snakeA;
    snakeA.length = 3;
    snakeA.dx = 1;
    snakeA.dy = 0;
    snakeA.alive = true;
    snakeA.score = 0;
    snakeA.color = (SDL_Color){50, 200, 50, 255}; // Green
    strcpy(snakeA.name, "Player A");

    // Position Snake A at the left side of the grid
    for (int i = 0; i < snakeA.length; i++) {
        snakeA.body[i].x = 5 - i;
        snakeA.body[i].y = 5;
    }

    // Initialize Snake B (Arrow keys)
    Snake snakeB;
    snakeB.length = 3;
    snakeB.dx = -1;
    snakeB.dy = 0;
    snakeB.alive = true;
    snakeB.score = 0;
    snakeB.color = (SDL_Color){50, 50, 200, 255}; // Blue
    strcpy(snakeB.name, "Player B");

    // Position Snake B at the right side of the grid
    for (int i = 0; i < snakeB.length; i++) {
        snakeB.body[i].x = GRID_WIDTH - 6 + i;
        snakeB.body[i].y = GRID_HEIGHT - 6;
    }

    // Initialize foods
    Food foods[FRUIT_COUNT * 2]; // Extra space for more fruits
    for (int i = 0; i < FRUIT_COUNT * 2; i++) {
        foods[i].active = false;
    }

    // Place initial fruits
    ensure_minimum_fruits(foods, FRUIT_COUNT * 2, &snakeA, &snakeB);

    // Initialize buttons
    Button playButton, playAgainButton, exitButton;
    init_button(&playButton, WINDOW_WIDTH / 2 - BUTTON_WIDTH / 2, 350, "PLAY");
    init_button(&playAgainButton, WINDOW_WIDTH / 2 - BUTTON_WIDTH / 2 - 110, 300, "PLAY AGAIN");
    init_button(&exitButton, WINDOW_WIDTH / 2 - BUTTON_WIDTH / 2 + 110, 300, "EXIT");

    // Game state
    GameState state = MENU;

    // Game loop variables
    bool quit = false;
    SDL_Event e;

    Uint32 frame_time = SDL_GetTicks();
    Uint32 move_time = frame_time;
    Uint32 game_start_time = 0;
    int time_left = GAME_DURATION;

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        printf("SDL_mixer Error: %s\n", Mix_GetError());
        return 1;
    }
    Mix_Chunk *apple_eat_sound = Mix_LoadWAV("apple_eat.wav");
    Mix_Chunk *obstacle_hit_sound = Mix_LoadWAV("obstacle_hit.wav");

    if (!apple_eat_sound || !obstacle_hit_sound) {
        printf("Mix_LoadWAV Error: %s\n", Mix_GetError());
        return 1;
    }




    while (!quit) {
        // Handle events
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = true;
            }
            else if (e.type == SDL_MOUSEMOTION) {
                int mouse_x = e.motion.x;
                int mouse_y = e.motion.y;

                if (state == MENU) {
                    playButton.hover = is_point_in_rect(mouse_x, mouse_y, &playButton.rect);
                }
                else if (state == GAME_OVER) {
                    playAgainButton.hover = is_point_in_rect(mouse_x, mouse_y, &playAgainButton.rect);
                    exitButton.hover = is_point_in_rect(mouse_x, mouse_y, &exitButton.rect);
                }
            }
            else if (e.type == SDL_MOUSEBUTTONDOWN) {
                int mouse_x = e.button.x;
                int mouse_y = e.button.y;

                if (state == MENU) {
                    if (is_point_in_rect(mouse_x, mouse_y, &playButton.rect)) {
                        state = PLAYING;
                        game_start_time = SDL_GetTicks();
                    }
                }
                else if (state == GAME_OVER) {
                    if (is_point_in_rect(mouse_x, mouse_y, &playAgainButton.rect)) {
                        reset_game(&snakeA, &snakeB, foods, FRUIT_COUNT * 2);
                        state = PLAYING;
                        game_start_time = SDL_GetTicks();
                    }
                    else if (is_point_in_rect(mouse_x, mouse_y, &exitButton.rect)) {
                        quit = true;
                    }
                }
            }
            else if (e.type == SDL_KEYDOWN) {
                      if (state == PLAYING) {
                    // Player A controls (WASD)
                    switch (e.key.keysym.sym) {
                        case SDLK_w:
                            if (snakeA.dy != 1) { // Prevent 180-degree turn
                                snakeA.dx = 0;
                                snakeA.dy = -1;
                            }
                            break;
                        case SDLK_s:
                            if (snakeA.dy != -1) {
                                snakeA.dx = 0;
                                snakeA.dy = 1;
                            }
                            break;
                        case SDLK_a:
                            if (snakeA.dx != 1) {
                                snakeA.dx = -1;
                                snakeA.dy = 0;
                            }
                            break;
                        case SDLK_d:
                            if (snakeA.dx != -1) {
                                snakeA.dx = 1;
                                snakeA.dy = 0;
                            }
                            break;

                        // Player B controls (Arrow Keys)
                        case SDLK_UP:
                            if (snakeB.dy != 1) {
                                snakeB.dx = 0;
                                snakeB.dy = -1;
                            }
                            break;
                        case SDLK_DOWN:
                            if (snakeB.dy != -1) {
                                snakeB.dx = 0;
                                snakeB.dy = 1;
                            }
                            break;
                        case SDLK_LEFT:
                            if (snakeB.dx != 1) {
                                snakeB.dx = -1;
                                snakeB.dy = 0;
                            }
                            break;
                        case SDLK_RIGHT:
                            if (snakeB.dx != -1) {
                                snakeB.dx = 1;
                                snakeB.dy = 0;
                            }
                            break;
                    }
                }
            }
        }

        // Update game state
        Uint32 current_time = SDL_GetTicks();

        if (state == PLAYING) {
            // Update time left
            time_left = GAME_DURATION - (current_time - game_start_time);

            // Check if time's up
            if (time_left <= 0) {
                state = GAME_OVER;
                time_left = 0;
            }

            // Move snakes at a fixed rate (150ms)
            if (current_time - move_time >= 150) {
                move_time = current_time;

                // Move snakes
                move_snake(&snakeA, &snakeB);
                move_snake(&snakeB, &snakeA);

                // Check for fruit collisions
                for (int i = 0; i < FRUIT_COUNT * 2; i++) {
                    if (foods[i].active) {
                        // Check if Snake A ate food
                    if (check_food_collision(&snakeA, &foods[i], apple_eat_sound)) {
                        foods[i].active = false;
                        snakeA.score += 10;
                        grow_snake(&snakeA);
                        }

                        // Check if Snake B ate food
                        else if (check_food_collision(&snakeB, &foods[i], apple_eat_sound)) {
                            foods[i].active = false;
                            snakeB.score += 10;
                            grow_snake(&snakeB);
                        }
                    }
                }

                // Ensure minimum number of fruits
                ensure_minimum_fruits(foods, FRUIT_COUNT * 2, &snakeA, &snakeB);

                // Check if game is over (both snakes dead)
                if (!snakeA.alive && !snakeB.alive) {
                    state = GAME_OVER;
                }
            }
        }

        // Clear screen
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // Render based on game state
        if (state == MENU) {
            draw_welcome_screen(renderer, &playButton, font);
        }
        else if (state == PLAYING) {
            // Draw UI area with scores and timer
            draw_score(renderer, &snakeA, &snakeB, time_left, font);

            // Draw grid
            draw_grid(renderer);

            // Draw foods
            draw_foods(renderer, foods, FRUIT_COUNT * 2, apple_texture);


            // Draw snakes
            draw_snake(renderer, &snakeA);
            draw_snake(renderer, &snakeB);
        }
        else if (state == GAME_OVER) {
            // Draw the game screen in the background
            draw_score(renderer, &snakeA, &snakeB, time_left, font);
            draw_grid(renderer);
            draw_foods(renderer, foods, FRUIT_COUNT * 2, apple_texture);

            draw_snake(renderer, &snakeA);
            draw_snake(renderer, &snakeB);

            // Draw game over screen
            draw_game_over_screen(renderer, &snakeA, &snakeB, &playAgainButton, &exitButton, font);
        }

        // Update screen
        SDL_RenderPresent(renderer);

        // Cap frame rate
        Uint32 frame_time_elapsed = SDL_GetTicks() - frame_time;
        if (frame_time_elapsed < 16) { // Target ~60 FPS
            SDL_Delay(16 - frame_time_elapsed);
        }
        frame_time = SDL_GetTicks();
    }

    // Clean up resources
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    return 0;
}
