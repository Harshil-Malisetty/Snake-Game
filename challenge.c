#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>
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
#define BUTTON_HEIGHT 40
#define BUTTON_PADDING 10

// Checkbox dimensions
#define CHECKBOX_SIZE 20
#define CHECKBOX_PADDING 10

// Max number of obstacles and foods
#define MAX_OBSTACLES 30
#define MAX_FOODS 5

extern Mix_Chunk *apple_eat_sound;
Mix_Chunk *apple_eat_sound = NULL;  // Global declaration

// Game states
typedef enum {
    MENU,
    PLAYING,
    GAME_OVER
} GameState;

// Game feature flags
typedef struct {
    bool movingFruit;
    bool multiFruit;
    bool timed;
    bool obstacles;
    bool speed;
    bool chaos;
} GameFeatures;

typedef struct {
    int x, y;
} Segment;

typedef struct {
    Segment body[100];
    int length;
    int dx, dy;
    bool alive;
} Snake;

typedef struct {
    int x, y;
    int value;   // Point value
    int type;    // Visual type
    bool moving; // Whether it moves
    int dx, dy;  // Direction for moving fruits
} Food;

typedef struct {
    int x, y;
    int dx, dy;  // Direction for moving obstacles
    bool moving; // Whether it moves
} Obstacle;

typedef struct {
    SDL_Rect rect;
    char text[30];
    bool hover;
    bool checked;  // For checkboxes
    bool isCheckbox;
} Button;

typedef struct {
    bool timed;
    int timeRemaining; // In seconds
    int maxTime;       // Starting time

    bool hasObstacles;
    Obstacle obstacles[MAX_OBSTACLES];
    int obstacleCount;
    bool movingObstacles;
    int obstacleMoveInterval;
    Uint32 lastObstacleMove;

    bool movingFruit;
    int fruitMoveInterval; // How often the fruit moves (in milliseconds)
    Uint32 lastFruitMove;  // Time of last fruit movement

    bool multiFruit;
    Food foods[MAX_FOODS];
    int foodCount;

    bool speed;
    int updateDelay; // Basic snake speed

    char modeName[50]; // Name of the current mode configuration
} GameConfig;

// Function prototypes
void draw_grid(SDL_Renderer *renderer);
void draw_snake(SDL_Renderer *renderer, Snake *snake);
void draw_food(SDL_Renderer *renderer, Food *food,
               SDL_Texture *apple_texture, SDL_Texture *banana_texture,
               SDL_Texture *grapes_texture);


void draw_obstacles(SDL_Renderer *renderer, GameConfig *config);
void draw_segment(SDL_Renderer *renderer, int x, int y, char segment, int width, int height, int thickness);
void draw_digit(SDL_Renderer *renderer, int x, int y, int digit, int width, int height, int thickness);
void draw_score(SDL_Renderer *renderer, int score, TTF_Font *font);
void move_snake(Snake *snake);
bool check_food_collision(Snake *snake, Food *food, Mix_Chunk *apple_eat_sound);
bool check_obstacle_collision(Snake *snake, GameConfig *config);
void place_food(Food *food, Snake *snake, GameConfig *config);
void place_obstacles(GameConfig *config, Snake *snake);
void grow_snake(Snake *snake);
void init_button(Button *button, int x, int y, const char *text, bool isCheckbox);
void draw_button(SDL_Renderer *renderer, Button *button, TTF_Font *font);
void draw_checkbox(SDL_Renderer *renderer, Button *checkbox, TTF_Font *font);
bool is_point_in_rect(int x, int y, SDL_Rect *rect);
void draw_text(SDL_Renderer *renderer, TTF_Font *font, const char *text, int x, int y, SDL_Color color);
void draw_text_centered(SDL_Renderer *renderer, TTF_Font *font, const char *text, int x, int y, SDL_Color color);
void draw_challenge_menu(SDL_Renderer *renderer, Button checkboxes[], int checkboxCount,
    Button *chaosButton, Button *playButton, Button *exitButton, TTF_Font *font);
void draw_game_over_screen(SDL_Renderer *renderer, int score, Button *playAgainButton, Button *exitButton, TTF_Font *font);
void reset_game(Snake *snake, GameConfig *config, int *score);
void draw_ui_area(SDL_Renderer *renderer, int score, GameConfig *config, TTF_Font *font);
void move_foods(GameConfig *config);
void move_obstacles(GameConfig *config);
void configure_game(GameConfig *config, GameFeatures *features, Snake *snake);
void initialize_multi_fruits(GameConfig *config, Snake *snake);
void update_game(Snake *snake, GameConfig *config, int *score, Uint32 currentTime);
void generate_mode_name(GameConfig *config, GameFeatures *features);

// Drawing functions
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
            int dx = radius - w; // Horizontal distance from center
            int dy = radius - h; // Vertical distance from center
            if ((dx * dx + dy * dy) <= (radius * radius)) { // Check if inside circle
                SDL_RenderDrawPoint(renderer, x + dx, y + dy);
            }
        }
    }
}

void draw_snake(SDL_Renderer *renderer, Snake *snake) {
    int radius = CELL_SIZE / 2; // Circle radius

    // Draw body segments in green
    SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255);
    for (int i = 1; i < snake->length; i++) {
        int x = snake->body[i].x * CELL_SIZE + radius;
        int y = snake->body[i].y * CELL_SIZE + UI_HEIGHT + radius;
        drawCircle(renderer, x, y, radius);
    }

    // Draw head in brighter green
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    int head_x = snake->body[0].x * CELL_SIZE + radius;
    int head_y = snake->body[0].y * CELL_SIZE + UI_HEIGHT + radius;
    drawCircle(renderer, head_x, head_y, radius);

    // Draw eyes (small white circles)
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    int eye_offset_x = radius / 2;  // Horizontal eye spacing
    int eye_offset_y = radius / 3;  // Vertical eye spacing
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

void draw_food(SDL_Renderer *renderer, Food *food,
               SDL_Texture *apple_texture, SDL_Texture *banana_texture,
               SDL_Texture *grapes_texture) {
    SDL_Rect rect = {
        food->x * CELL_SIZE,
        food->y * CELL_SIZE + UI_HEIGHT,
        CELL_SIZE,
        CELL_SIZE
    };

    // Select the correct texture
    SDL_Texture *texture = NULL;
    switch (food->type) {
        case 0: // Regular food (Red) → Apple
        case 3: // Rare food (Blue) → Apple
            texture = apple_texture;
            break;
        case 1: // Bonus food (Gold) → Banana
            texture = banana_texture;
            break;
        case 2: // Special food (Purple) → Grapes
            texture = grapes_texture;
            break;
        default:
            texture = apple_texture; // Fallback to apple
    }

    // Draw the selected texture
    if (texture) {
        SDL_RenderCopy(renderer, texture, NULL, &rect);
    }
}


void draw_obstacles(SDL_Renderer *renderer, GameConfig *config) {
    if (!config->hasObstacles) return;

    for (int i = 0; i < config->obstacleCount; i++) {
        // Regular obstacles are gray, moving obstacles are dark red
        if (config->obstacles[i].moving) {
            SDL_SetRenderDrawColor(renderer, 150, 50, 50, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
        }

        SDL_Rect rect = {
            config->obstacles[i].x * CELL_SIZE,
            config->obstacles[i].y * CELL_SIZE + UI_HEIGHT,
            CELL_SIZE,
            CELL_SIZE
        };
        SDL_RenderFillRect(renderer, &rect);
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

// Function to draw the UI area with score and game mode specific info
void draw_ui_area(SDL_Renderer *renderer, int score, GameConfig *config, TTF_Font *font) {
    // Background for UI area
    SDL_SetRenderDrawColor(renderer, 30, 30, 40, 255);
    SDL_Rect ui_rect = {0, 0, WINDOW_WIDTH, UI_HEIGHT};
    SDL_RenderFillRect(renderer, &ui_rect);

    // Draw a border between UI area and game grid
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
    SDL_RenderDrawLine(renderer, 0, UI_HEIGHT, WINDOW_WIDTH, UI_HEIGHT);

    // Draw score text with SDL_ttf
    char score_text[32];
    sprintf(score_text, "SCORE: %d", score);

    SDL_Color white = {255, 255, 255, 255};
    draw_text(renderer, font, score_text, UI_PADDING, UI_HEIGHT / 2 - 10, white);

    // Draw game mode name
    draw_text(renderer, font, config->modeName,
              WINDOW_WIDTH / 2 - 100, UI_HEIGHT / 2 - 10, white);

    // Draw time remaining for timed mode
    if (config->timed) {
        char time_text[20];
        sprintf(time_text, "TIME: %ds", config->timeRemaining);
        draw_text(renderer, font, time_text, WINDOW_WIDTH - 150, UI_HEIGHT / 2 - 10, white);
    }
}

// Legacy function for backwards compatibility
void draw_score(SDL_Renderer *renderer, int score, TTF_Font *font) {
    GameConfig config = {0};
    strcpy(config.modeName, "CLASSIC");
    draw_ui_area(renderer, score, &config, font);
}

// Game logic functions
void move_snake(Snake *snake) {
    // Move body segments
    for (int i = snake->length - 1; i > 0; i--) {
        snake->body[i] = snake->body[i - 1];
    }

    // Move head
    snake->body[0].x += snake->dx;
    snake->body[0].y += snake->dy;

    // Check wall collision
    if (snake->body[0].x < 0 || snake->body[0].x >= GRID_WIDTH ||
        snake->body[0].y < 0 || snake->body[0].y >= GRID_HEIGHT) {
        snake->alive = false;
    }

    // Check self collision
    for (int i = 1; i < snake->length; i++) {
        if (snake->body[0].x == snake->body[i].x && snake->body[0].y == snake->body[i].y) {
            snake->alive = false;
            break;
        }
    }
}

bool check_food_collision(Snake *snake, Food *food, Mix_Chunk *apple_eat_sound) {
    if (snake->body[0].x == food->x && snake->body[0].y == food->y) {
        Mix_PlayChannel(-1, apple_eat_sound, 0);  // Play eating sound
        return true;
    }
    return false;
}


bool check_obstacle_collision(Snake *snake, GameConfig *config) {
    if (!config->hasObstacles) return false;

    for (int i = 0; i < config->obstacleCount; i++) {
        if (snake->body[0].x == config->obstacles[i].x &&
            snake->body[0].y == config->obstacles[i].y) {
            return true;
        }
    }
    return false;
}

void place_food(Food *food, Snake *snake, GameConfig *config) {
    bool valid_position = false;
    int x, y;

    while (!valid_position) {
        x = rand() % GRID_WIDTH;
        y = rand() % GRID_HEIGHT;
        valid_position = true;

        // Check if the position is not occupied by the snake
        for (int i = 0; i < snake->length; i++) {
            if (x == snake->body[i].x && y == snake->body[i].y) {
                valid_position = false;
                break;
            }
        }

        // Check if the position is not occupied by an obstacle
        if (valid_position && config->hasObstacles) {
            for (int i = 0; i < config->obstacleCount; i++) {
                if (x == config->obstacles[i].x && y == config->obstacles[i].y) {
                    valid_position = false;
                    break;
                }
            }
        }

        // Check if the position is not occupied by another food item
        if (valid_position && config->multiFruit) {
            for (int i = 0; i < config->foodCount; i++) {
                if (x == config->foods[i].x && y == config->foods[i].y) {
                    valid_position = false;
                    break;
                }
            }
        }
    }

    food->x = x;
    food->y = y;

    // For moving fruit
    if (config->movingFruit && food->moving) {
        // Randomly assign an initial direction
        do {
            food->dx = (rand() % 3) - 1; // -1, 0, or 1
            food->dy = (rand() % 3) - 1; // -1, 0, or 1
        } while (food->dx == 0 && food->dy == 0); // Ensure it's not stationary
    }
}

void place_obstacles(GameConfig *config, Snake *snake) {
    if (!config->hasObstacles) return;

    config->obstacleCount = rand() % (MAX_OBSTACLES / 2) + (MAX_OBSTACLES / 2); // 15-30 obstacles

    for (int i = 0; i < config->obstacleCount; i++) {
        bool valid_position = false;
        int x, y;

        while (!valid_position) {
            x = rand() % GRID_WIDTH;
            y = rand() % GRID_HEIGHT;
            valid_position = true;

            // Check if the position is not occupied by the snake
            for (int j = 0; j < snake->length; j++) {
                if (x == snake->body[j].x && y == snake->body[j].y) {
                    valid_position = false;
                    break;
                }
            }

            // Check if the position is not occupied by food
            for (int j = 0; j < config->foodCount; j++) {
                if (x == config->foods[j].x && y == config->foods[j].y) {
                    valid_position = false;
                    break;
                }
            }

            // Check if the position is not occupied by another obstacle
            for (int j = 0; j < i; j++) {
                if (x == config->obstacles[j].x && y == config->obstacles[j].y) {
                    valid_position = false;
                    break;
                }
            }

            // Make sure there's enough space around the snake's head
            if (abs(x - snake->body[0].x) < 3 && abs(y - snake->body[0].y) < 3) {
                valid_position = false;
            }
        }

        config->obstacles[i].x = x;
        config->obstacles[i].y = y;

        // For moving obstacles
        if (config->movingObstacles && rand() % 3 == 0) { // 1/3 chance to be moving
            config->obstacles[i].moving = true;
            // Randomly assign an initial direction
            do {
                config->obstacles[i].dx = (rand() % 3) - 1; // -1, 0, or 1
                config->obstacles[i].dy = (rand() % 3) - 1; // -1, 0, or 1
            } while (config->obstacles[i].dx == 0 && config->obstacles[i].dy == 0);
        } else {
            config->obstacles[i].moving = false;
        }
    }
}

void grow_snake(Snake *snake) {
    if (snake->length < 100) {
        snake->body[snake->length] = snake->body[snake->length - 1];
        snake->length++;
    }
}

void init_button(Button *button, int x, int y, const char *text, bool isCheckbox) {
    if (isCheckbox) {
        button->rect.x = x;
        button->rect.y = y;
        button->rect.w = CHECKBOX_SIZE;
        button->rect.h = CHECKBOX_SIZE;
    } else {
        button->rect.x = x;
        button->rect.y = y;
        button->rect.w = BUTTON_WIDTH;
        button->rect.h = BUTTON_HEIGHT;
    }

    strncpy(button->text, text, sizeof(button->text) - 1);
    button->hover = false;
    button->checked = false;
    button->isCheckbox = isCheckbox;
}

void draw_button(SDL_Renderer *renderer, Button *button, TTF_Font *font) {
    if (button->isCheckbox) {
        draw_checkbox(renderer, button, font);
        return;
    }

    // Draw button background
    if (button->hover) {
        SDL_SetRenderDrawColor(renderer, 100, 100, 200, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 60, 60, 150, 255);
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

void draw_checkbox(SDL_Renderer *renderer, Button *checkbox, TTF_Font *font) {
    // Draw checkbox border
    SDL_SetRenderDrawColor(renderer, 150, 150, 200, 255);
    SDL_RenderDrawRect(renderer, &checkbox->rect);

    // Draw checkbox background (filled if checked)
    if (checkbox->hover && !checkbox->checked) {
        SDL_SetRenderDrawColor(renderer, 80, 80, 150, 255);
        SDL_Rect inner = {
            checkbox->rect.x + 2,
            checkbox->rect.y + 2,
            checkbox->rect.w - 4,
            checkbox->rect.h - 4
        };
        SDL_RenderFillRect(renderer, &inner);
    } else if (checkbox->checked) {
        SDL_SetRenderDrawColor(renderer, 100, 200, 100, 255);
        SDL_Rect inner = {
            checkbox->rect.x + 2,
            checkbox->rect.y + 2,
            checkbox->rect.w - 4,
            checkbox->rect.h - 4
        };
        SDL_RenderFillRect(renderer, &inner);
    }

    // Draw checkbox label
    SDL_Color text_color = {255, 255, 255, 255};
    draw_text(renderer, font, checkbox->text,
              checkbox->rect.x + checkbox->rect.w + CHECKBOX_PADDING,
              checkbox->rect.y + checkbox->rect.h / 2 - 10,
              text_color);
}

bool is_point_in_rect(int x, int y, SDL_Rect *rect) {
    return (x >= rect->x && x < rect->x + rect->w &&
            y >= rect->y && y < rect->y + rect->h);
}

void draw_text(SDL_Renderer *renderer, TTF_Font *font, const char *text, int x, int y, SDL_Color color) {
    SDL_Surface *surface = TTF_RenderText_Blended(font, text, color);
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);

    SDL_Rect dest = {x, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, NULL, &dest);

    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

void draw_text_centered(SDL_Renderer *renderer, TTF_Font *font, const char *text, int x, int y, SDL_Color color) {
    SDL_Surface *surface = TTF_RenderText_Blended(font, text, color);
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);

    SDL_Rect dest = {x - surface->w / 2, y - surface->h / 2, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, NULL, &dest);

    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}
void draw_challenge_menu(SDL_Renderer *renderer, Button checkboxes[], int checkboxCount,
    Button *chaosButton, Button *playButton, Button *exitButton, TTF_Font *font) {
// Draw background
SDL_SetRenderDrawColor(renderer, 20, 20, 30, 255);
SDL_RenderClear(renderer);

// Draw title
SDL_Color white = {255, 255, 255, 255};
draw_text_centered(renderer, font, "SNAKE GAME CHALLENGES", WINDOW_WIDTH / 2, 60, white);

// Draw checkboxes
for (int i = 0; i < checkboxCount; i++) {
draw_checkbox(renderer, &checkboxes[i], font);
}

// Draw chaos button
draw_button(renderer, chaosButton, font);

// Draw play and exit buttons
draw_button(renderer, playButton, font);
draw_button(renderer, exitButton, font);
}
void draw_game_over_screen(SDL_Renderer *renderer, int score, Button *playAgainButton, Button *exitButton, TTF_Font *font) {
    // Draw background
    SDL_SetRenderDrawColor(renderer, 20, 20, 30, 255);
    SDL_RenderClear(renderer);

    // Draw game over text
    SDL_Color white = {255, 255, 255, 255};
    draw_text_centered(renderer, font, "GAME OVER", WINDOW_WIDTH / 2, WINDOW_HEIGHT / 3, white);

    // Draw score
    char score_text[32];
    sprintf(score_text, "SCORE: %d", score);
    draw_text_centered(renderer, font, score_text, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2, white);

    // Draw buttons
    draw_button(renderer, playAgainButton, font);
    draw_button(renderer, exitButton, font);
}

void reset_game(Snake *snake, GameConfig *config, int *score) {
    // Reset snake
    snake->length = 3;
    snake->body[0].x = GRID_WIDTH / 2;
    snake->body[0].y = GRID_HEIGHT / 2;
    snake->body[1].x = GRID_WIDTH / 2 - 1;
    snake->body[1].y = GRID_HEIGHT / 2;
    snake->body[2].x = GRID_WIDTH / 2 - 2;
    snake->body[2].y = GRID_HEIGHT / 2;
    snake->dx = 1;
    snake->dy = 0;
    snake->alive = true;

    // Reset score
    *score = 0;

    // Reset time for timed mode
    if (config->timed) {
        config->timeRemaining = config->maxTime;
    }

    // Place obstacles
    if (config->hasObstacles) {
        place_obstacles(config, snake);
    }

    // Place food items
    if (config->multiFruit) {
        initialize_multi_fruits(config, snake);
    } else {
        config->foodCount = 1;
        config->foods[0].type = 0;  // Regular food
        config->foods[0].value = 1;
        config->foods[0].moving = config->movingFruit;
        place_food(&config->foods[0], snake, config);
    }
}

void move_foods(GameConfig *config) {
    if (!config->movingFruit) return;

    for (int i = 0; i < config->foodCount; i++) {
        if (config->foods[i].moving) {
            int new_x = config->foods[i].x + config->foods[i].dx;
            int new_y = config->foods[i].y + config->foods[i].dy;

            // Check if the food would go out of bounds and change direction if needed
            if (new_x < 0 || new_x >= GRID_WIDTH) {
                config->foods[i].dx *= -1;
                new_x = config->foods[i].x + config->foods[i].dx;
            }

            if (new_y < 0 || new_y >= GRID_HEIGHT) {
                config->foods[i].dy *= -1;
                new_y = config->foods[i].y + config->foods[i].dy;
            }

            // Check if the food would collide with an obstacle
            bool collision = false;
            if (config->hasObstacles) {
                for (int j = 0; j < config->obstacleCount; j++) {
                    if (new_x == config->obstacles[j].x && new_y == config->obstacles[j].y) {
                        collision = true;
                        break;
                    }
                }
            }

            // If no collision, update the position
            if (!collision) {
                config->foods[i].x = new_x;
                config->foods[i].y = new_y;
            } else {
                // Otherwise, change direction
                config->foods[i].dx *= -1;
                config->foods[i].dy *= -1;
            }
        }
    }
}

void move_obstacles(GameConfig *config) {
    if (!config->movingObstacles) return;

    for (int i = 0; i < config->obstacleCount; i++) {
        if (config->obstacles[i].moving) {
            int new_x = config->obstacles[i].x + config->obstacles[i].dx;
            int new_y = config->obstacles[i].y + config->obstacles[i].dy;

            // Check if the obstacle would go out of bounds and change direction if needed
            if (new_x < 0 || new_x >= GRID_WIDTH) {
                config->obstacles[i].dx *= -1;
                new_x = config->obstacles[i].x + config->obstacles[i].dx;
            }

            if (new_y < 0 || new_y >= GRID_HEIGHT) {
                config->obstacles[i].dy *= -1;
                new_y = config->obstacles[i].y + config->obstacles[i].dy;
            }

            // Check for collisions with other obstacles
            bool collision = false;
            for (int j = 0; j < config->obstacleCount; j++) {
                if (i != j && new_x == config->obstacles[j].x && new_y == config->obstacles[j].y) {
                    collision = true;
                    break;
                }
            }

            // Check for collisions with food
            for (int j = 0; j < config->foodCount; j++) {
                if (new_x == config->foods[j].x && new_y == config->foods[j].y) {
                    collision = true;
                    break;
                }
            }

            // If no collision, update the position
            if (!collision) {
                config->obstacles[i].x = new_x;
                config->obstacles[i].y = new_y;
            } else {
                // Otherwise, change direction
                config->obstacles[i].dx *= -1;
                config->obstacles[i].dy *= -1;
            }
        }
    }
}

void configure_game(GameConfig *config, GameFeatures *features, Snake *snake) {
    // Reset config to defaults
    memset(config, 0, sizeof(GameConfig));

    // Apply feature settings
    config->movingFruit = features->movingFruit;
    config->multiFruit = features->multiFruit;
    config->timed = features->timed;
    config->hasObstacles = features->obstacles;
    config->movingObstacles = features->obstacles && features->movingFruit; // Only if both are selected

    // Set base speed
    if (features->speed) {
        config->updateDelay = 100; // Faster speed
    } else {
        config->updateDelay = 150; // Normal speed
    }

    // Configure timed mode
    if (config->timed) {
        config->maxTime = 60; // 60 seconds
        config->timeRemaining = config->maxTime;
    }

    // Configure food movement
    if (config->movingFruit) {
        config->fruitMoveInterval = 500; // Move every 500ms
    }

    // Configure obstacle movement
    if (config->movingObstacles) {
        config->obstacleMoveInterval = 800; // Move every 800ms
    }

    // Generate a name for this mode configuration
    generate_mode_name(config, features);
}

void initialize_multi_fruits(GameConfig *config, Snake *snake) {
    if (!config->multiFruit) {
        config->foodCount = 1;
        config->foods[0].type = 0;  // Regular food
        config->foods[0].value = 1;
        config->foods[0].moving = config->movingFruit;
        place_food(&config->foods[0], snake, config);
        return;
    }

    // For multi-fruit mode, place 3-5 fruits
    config->foodCount = rand() % 3 + 3; // 3-5 fruits

    for (int i = 0; i < config->foodCount; i++) {
        config->foods[i].type = rand() % 4; // 0-3 different types

        // Set point value based on type
        switch (config->foods[i].type) {
            case 0: config->foods[i].value = 1; break;  // Regular
            case 1: config->foods[i].value = 2; break;  // Bonus
            case 2: config->foods[i].value = 3; break;  // Special
            case 3: config->foods[i].value = 5; break;  // Rare
        }

        // Determine if this fruit should move (if moving fruit is enabled)
        if (config->movingFruit) {
            // Higher value fruits are more likely to move
            config->foods[i].moving = (rand() % 5 < config->foods[i].type + 2);
        } else {
            config->foods[i].moving = false;
        }

        place_food(&config->foods[i], snake, config);
    }
}

void update_game(Snake *snake, GameConfig *config, int *score, Uint32 currentTime) {
    // Update moving fruits
    if (config->movingFruit && currentTime - config->lastFruitMove > config->fruitMoveInterval) {
        move_foods(config);
        config->lastFruitMove = currentTime;
    }

    // Update moving obstacles
    if (config->movingObstacles && currentTime - config->lastObstacleMove > config->obstacleMoveInterval) {
        move_obstacles(config);
        config->lastObstacleMove = currentTime;
    }

    // Update timer
    if (config->timed && config->timeRemaining > 0) {
        config->timeRemaining = config->maxTime - (currentTime / 1000);
        if (config->timeRemaining <= 0) {
            snake->alive = false;
        }
    }
}

void generate_mode_name(GameConfig *config, GameFeatures *features) {
    strcpy(config->modeName, "");

    // Check if chaos mode (everything enabled)
    if (features->movingFruit && features->multiFruit && features->timed &&
        features->obstacles && features->speed) {
        strcpy(config->modeName, "CHAOS MODE");
        return;
    }

    // Otherwise, build the name based on enabled features
    if (!features->movingFruit && !features->multiFruit && !features->timed &&
        !features->obstacles && !features->speed) {
        strcpy(config->modeName, "CLASSIC");
        return;
    }

    // Build the name from enabled features
    bool addedFeature = false;

    if (features->speed) {
        strcat(config->modeName, "SPEED");
        addedFeature = true;
    }

    if (features->timed) {
        if (addedFeature) strcat(config->modeName, "+");
        strcat(config->modeName, "TIMED");
        addedFeature = true;
    }

    if (features->obstacles) {
        if (addedFeature) strcat(config->modeName, "+");
        if (features->movingFruit) {
            strcat(config->modeName, "MVG-");
        }
        strcat(config->modeName, "OBSTACLE");
        addedFeature = true;
    }

    if (features->multiFruit) {
        if (addedFeature) strcat(config->modeName, "+");
        strcat(config->modeName, "MULTI-FRUIT");
        addedFeature = true;
    } else if (features->movingFruit) {
        if (addedFeature) strcat(config->modeName, "+");
        strcat(config->modeName, "MVG-FRUIT");
    }
}

// Main function for the Challenge Menu
int main(int argc, char *argv[]) {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }
    Mix_Chunk *apple_eat_sound = NULL;
    // Initialize SDL_mixer
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        printf("SDL_mixer could not initialize! SDL_mixer Error: %s\n", Mix_GetError());
        return 1;
    }
    // Load apple eating sound
    apple_eat_sound = Mix_LoadWAV("apple_eat.wav");
    if (!apple_eat_sound) {
        printf("Failed to load apple eating sound! SDL_mixer Error: %s\n", Mix_GetError());
    }

    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
    printf("SDL_image could not initialize! IMG_Error: %s\n", IMG_GetError());
    SDL_Quit();
    return 1;
    }


    // Initialize SDL_ttf
    if (TTF_Init() != 0) {
        printf("TTF_Init Error: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }

    // Create window
    SDL_Window *window = SDL_CreateWindow("Snake Game Challenges",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          WINDOW_WIDTH,
                                          WINDOW_HEIGHT,
                                          SDL_WINDOW_SHOWN);
    if (window == NULL) {
        printf("SDL_CreateWindow Error: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // Create renderer
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1,
                                               SDL_RENDERER_ACCELERATED |
                                               SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL) {
        printf("SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    SDL_Texture *apple_texture = IMG_LoadTexture(renderer, "apple.png");
    SDL_Texture *banana_texture = IMG_LoadTexture(renderer, "banana.png");
    SDL_Texture *grapes_texture = IMG_LoadTexture(renderer, "grapes.png");

    if (!apple_texture || !banana_texture || !grapes_texture) {
        printf("Failed to load fruit textures: %s\n", IMG_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        return 1;
    }



    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        printf("SDL_mixer Error: %s\n", Mix_GetError());
        return 1;
    Mix_Chunk *apple_eat_sound = Mix_LoadWAV("apple_eat.wav");
    }
    else {
        printf("SDL_mixer initialized successfully!\n");
    }


    // Load apple texture

    if (!apple_texture) {
        printf("Failed to load apple texture: %s\n", IMG_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
    return 1;
    }

    // Load font
    TTF_Font *font = TTF_OpenFont("dejavu-fonts-ttf-2.37/ttf/DejaVuSans.ttf", 24);
    if (font == NULL) {
        printf("TTF_OpenFont Error: %s\n", TTF_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // Initialize random number generator
    srand(time(NULL));

    // Create game objects
    Snake snake = {0};
    GameConfig config = {0};
    GameFeatures features = {0};
    int score = 0;
    GameState gameState = MENU;

    // Create menu buttons
    Button checkboxes[5]; // 5 challenge options
    init_button(&checkboxes[0], WINDOW_WIDTH / 2 - 100, 120, "Moving Fruit", true);
    init_button(&checkboxes[1], WINDOW_WIDTH / 2 - 100, 160, "Multi-Fruit", true);
    init_button(&checkboxes[2], WINDOW_WIDTH / 2 - 100, 200, "Timed Mode", true);
    init_button(&checkboxes[3], WINDOW_WIDTH / 2 - 100, 240, "Speed Mode", true);
    init_button(&checkboxes[4], WINDOW_WIDTH / 2 - 100, 280, "Moving Obstacle", true);

    Button chaosButton;
    init_button(&chaosButton, WINDOW_WIDTH / 2 - 100, 330, "CHAOS MODE (Everything!)", false);

    Button playButton;
    init_button(&playButton, WINDOW_WIDTH / 2 - 100, 400, "PLAY", false);

    Button exitButton;
    init_button(&exitButton, WINDOW_WIDTH / 2 - 100, 450, "EXIT", false);

    Button playAgainButton;
    init_button(&playAgainButton, WINDOW_WIDTH / 2 - 100, 400, "PLAY AGAIN", false);

    Uint32 lastUpdate = 0;
    Uint32 lastFPSUpdate = 0;
    int frames = 0;
    int fps = 0;

    // Main game loop
    bool running = true;
    SDL_Event event;

    while (running) {
        // Process events
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_KEYDOWN:
                    if (gameState == PLAYING) {
                        switch (event.key.keysym.sym) {
                            case SDLK_UP:
                                if (snake.dy != 1) { // Prevent moving directly backwards
                                    snake.dx = 0;
                                    snake.dy = -1;
                                }
                                break;
                            case SDLK_DOWN:
                                if (snake.dy != -1) {
                                    snake.dx = 0;
                                    snake.dy = 1;
                                }
                                break;
                            case SDLK_LEFT:
                                if (snake.dx != 1) {
                                    snake.dx = -1;
                                    snake.dy = 0;
                                }
                                break;
                            case SDLK_RIGHT:
                                if (snake.dx != -1) {
                                    snake.dx = 1;
                                    snake.dy = 0;
                                }
                                break;
                            case SDLK_ESCAPE:
                                gameState = MENU;
                                break;
                        }
                    }
                    break;
                case SDL_MOUSEMOTION:
                    // Update button hover state
                    if (gameState == MENU) {
                        int mouseX = event.motion.x;
                        int mouseY = event.motion.y;

                        for (int i = 0; i < 5; i++) {
                            checkboxes[i].hover = is_point_in_rect(mouseX, mouseY, &checkboxes[i].rect);
                        }

                        chaosButton.hover = is_point_in_rect(mouseX, mouseY, &chaosButton.rect);
                        playButton.hover = is_point_in_rect(mouseX, mouseY, &playButton.rect);
                        exitButton.hover = is_point_in_rect(mouseX, mouseY, &exitButton.rect);
                    } else if (gameState == GAME_OVER) {
                        int mouseX = event.motion.x;
                        int mouseY = event.motion.y;

                        playAgainButton.hover = is_point_in_rect(mouseX, mouseY, &playAgainButton.rect);
                        exitButton.hover = is_point_in_rect(mouseX, mouseY, &exitButton.rect);
                    }
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        int mouseX = event.button.x;
                        int mouseY = event.button.y;

                        if (gameState == MENU) {
                            // Check challenge checkboxes
                            for (int i = 0; i < 5; i++) {
                                if (is_point_in_rect(mouseX, mouseY, &checkboxes[i].rect)) {
                                    checkboxes[i].checked = !checkboxes[i].checked;
                                }
                            }

                            // Check chaos button
                            if (is_point_in_rect(mouseX, mouseY, &chaosButton.rect)) {
                                // Enable all features
                                for (int i = 0; i < 5; i++) {
                                    checkboxes[i].checked = true;
                                }
                            }

                            // Check play button
                            if (is_point_in_rect(mouseX, mouseY, &playButton.rect)) {
                                // Configure game features based on checkboxes
                                features.movingFruit = checkboxes[0].checked;
                                features.multiFruit = checkboxes[1].checked;
                                features.timed = checkboxes[2].checked;
                                features.speed = checkboxes[3].checked;
                                features.obstacles = checkboxes[4].checked;
                                features.chaos = checkboxes[0].checked &&
                                                checkboxes[1].checked &&
                                                checkboxes[2].checked &&
                                                checkboxes[3].checked &&
                                                checkboxes[4].checked;

                                // Configure the game based on selected features
                                configure_game(&config, &features, &snake);

                                // Reset the game
                                reset_game(&snake, &config, &score);

                                // Set last update times for moving objects
                                config.lastFruitMove = SDL_GetTicks();
                                config.lastObstacleMove = SDL_GetTicks();

                                // Switch to playing state
                                gameState = PLAYING;
                            }

                            // Check exit button
                            if (is_point_in_rect(mouseX, mouseY, &exitButton.rect)) {
                                running = false;
                            }
                        } else if (gameState == GAME_OVER) {
                            // Check play again button
                            if (is_point_in_rect(mouseX, mouseY, &playAgainButton.rect)) {
                                gameState = MENU;
                            }

                            // Check exit button
                            if (is_point_in_rect(mouseX, mouseY, &exitButton.rect)) {
                                running = false;
                            }
                        }
                    }
                    break;
            }
        }

        Uint32 currentTime = SDL_GetTicks();

        // Update game state
        if (gameState == PLAYING) {
            // Update at appropriate intervals based on speed setting
            if (currentTime - lastUpdate > config.updateDelay) {
                // Move the snake
                move_snake(&snake);

                // Check for obstacle collision
                if (check_obstacle_collision(&snake, &config)) {
                    snake.alive = false;
                }

                // Check for food collision and handle multiple food types
                // Check for food collision and handle multiple food types
                for (int i = 0; i < config.foodCount; i++) {
                    if (check_food_collision(&snake, &config.foods[i], apple_eat_sound)) {
                // Play apple eating sound for all food types
                        Mix_PlayChannel(-1, apple_eat_sound, 0);

                // Increase score based on food value
                        score += config.foods[i].value;

                // Grow snake
                        grow_snake(&snake);

        // Replace eaten food
                        place_food(&config.foods[i], &snake, &config);
                    }
                }


                // Update game elements (moving fruits, obstacles, timer)
                update_game(&snake, &config, &score, currentTime);

                // Check if game over
                if (!snake.alive) {
                    gameState = GAME_OVER;
                }

                lastUpdate = currentTime;
            }
        }

        // Calculate FPS
        frames++;
        if (currentTime - lastFPSUpdate >= 1000) {
            fps = frames;
            frames = 0;
            lastFPSUpdate = currentTime;
        }

        // Clear screen
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // Render game elements based on game state
        switch (gameState) {
            case MENU:
                draw_challenge_menu(renderer, checkboxes, 5, &chaosButton, &playButton, &exitButton, font);
                break;


            case PLAYING:
                draw_ui_area(renderer, score, &config, font);
                draw_grid(renderer);

                // Draw all food items
                for (int i = 0; i < config.foodCount; i++) {
                    draw_food(renderer, &config.foods[i], apple_texture, banana_texture, grapes_texture);
                }




                // Draw obstacles if enabled
                if (config.hasObstacles) {
                    draw_obstacles(renderer, &config);
                }

                // Draw snake
                draw_snake(renderer, &snake);
                break;

            case GAME_OVER:
                draw_game_over_screen(renderer, score, &playAgainButton, &exitButton, font);
                break;
        }

        // Display FPS in debug mode (optional)
        if (0) { // Set to 1 to enable FPS display
            char fps_text[16];
            sprintf(fps_text, "FPS: %d", fps);
            SDL_Color white = {255, 255, 255, 255};
            draw_text(renderer, font, fps_text, 10, 10, white);
        }

        // Present render
        SDL_RenderPresent(renderer);

        // Cap frame rate (optional)
        SDL_Delay(1);
    }

    // Cleanup resources
    Mix_FreeChunk(apple_eat_sound);
    Mix_CloseAudio();
    SDL_DestroyTexture(banana_texture);
    SDL_DestroyTexture(grapes_texture);
    SDL_DestroyTexture(apple_texture);
    IMG_Quit();
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    return 0;
}
