#include "input_manager.h"
#include <time.h>

#define MAX_KEYS 256
#define MAX_BUFFER_SIZE 1024

static bool key_states[MAX_KEYS] = {false};
static bool key_pressed_last_frame[MAX_KEYS] = {false};
static bool unlock = true;

static struct termios original_termios;  // Global to store original terminal settings

/// @brief Save the current terminal settings
void save_original_mode() {
    if (tcgetattr(STDIN_FILENO, &original_termios) < 0) {
        perror("tcgetattr");
        exit(EXIT_FAILURE);
    }
}

/// @brief Restore the terminal to its original mode
void restore_terminal_mode() {
    if (tcsetattr(STDIN_FILENO, TCSANOW, &original_termios) < 0) {
        perror("tcsetattr");
        exit(EXIT_FAILURE);
    }
    wprintf(L"\33[?25h");                // Re-enable cursor visibility
    wprintf(L"\033[?1003l\033[?1006l");  // Enable Mouse events
    wprintf(L"\033[H\033[J");            // Clear the screen
    wprintf(L"\033[0m");                 // Reset color
    fflush(stdout);
}

/// @brief Signal handler to restore terminal on unexpected termination
void handle_signal(int signo) {
    restore_terminal_mode();
    exit(EXIT_FAILURE);
}

/// @brief Ensure restoration on program exit
void setup_terminal_restoration() {
    save_original_mode();
    atexit(restore_terminal_mode);   // Ensure restoration on normal exit
    signal(SIGINT, handle_signal);   // Handle Ctrl+C (SIGINT)
    signal(SIGTERM, handle_signal);  // Handle termination signals
}

// Function to set the terminal to raw mode
void set_raw_mode() {
    struct termios t;

    // Get the current terminal attributes
    if (tcgetattr(STDIN_FILENO, &t) < 0) {
        perror("tcgetattr");
        exit(EXIT_FAILURE);
    }

    // Modify the terminal attributes for raw mode
    t.c_lflag &= ~(ICANON | ECHO);  // Disable canonical mode and echo
    t.c_cc[VMIN] = 1;               // Minimum number of characters to read
    t.c_cc[VTIME] = 0;              // Timeout (deciseconds) for read

    tcsetattr(STDIN_FILENO, TCSANOW, &t);
}

// Function to set the file descriptor to non-blocking mode
void set_nonblocking_mode(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        exit(EXIT_FAILURE);
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL");
        exit(EXIT_FAILURE);
    }
}

void init_terminal() {
    setup_terminal_restoration();

    set_raw_mode();
    set_nonblocking_mode(STDIN_FILENO);

    srand(time(NULL));

    setlocale(LC_CTYPE, "");
    wprintf(L"\33[?25l");                // Disable cursor
    wprintf(L"\033[H\033[J");            // Clear
    wprintf(L"\033[?1003h\033[?1006h");  // Enable mouse motion events
    fflush(stdout);
}

typedef struct MouseEvent {
    int target_x;
    int target_y;
    bool left_just_pressed;
    bool right_just_pressed;
    int left_click;
    int right_click;
} MouseEvent;

/**
 * @brief Parses an SGR mouse event from the given buffer.
 *
 * @param buffer The buffer containing the SGR mouse event data.
 * @param target_x Pointer to an integer where the x-coordinate of the mouse event will be stored.
 * @param target_y Pointer to an integer where the y-coordinate of the mouse event will be stored.
 * @param left_just_pressed Pointer to a boolean that indicates whether the left mouse button was just pressed.
 * @param right_just_pressed Pointer to a boolean that indicates whether the right mouse button was just pressed.
 * @param left_click Pointer to an integer that is set to one after the first left click and then reset to 0 after the release (prevent holding)
 * @param right_click Pointer to an integer that is set to one after the first right click and then reset to 0 after the release (prevent holding)
 */
void parse_sgr_mouse_event(const char* buffer, MouseEvent* event) {
    int button, x, y;
    char event_type;

    event->left_click = 0;
    event->right_click = 0;

    if (sscanf(buffer, "\033[<%d;%d;%d%c", &button, &x, &y, &event_type) == 4) {
        if (button >= 64) return;

        int btn = button & 0x03;

        if ((button & 0x03) == 0) {
            if (event_type == 'M' && !event->left_just_pressed) {
                event->left_click = 1;
                event->left_just_pressed = true;
                event->target_x = (x / 2) * 2;
                event->target_y = y;
            } else if (event_type == 'm') {
                event->left_just_pressed = false;
            }
        } else if (btn == 2) {
            if (event_type == 'M' && !event->right_just_pressed) {
                event->right_click = 1;
                event->right_just_pressed = true;
            } else if (event_type == 'm') {
                event->right_just_pressed = false;
            }
        }
    }
}

/**
 * @brief Checks if the given buffer contains a mouse event.
 *
 * @param buffer The buffer to check.
 * @param length The length of the buffer.
 * @return true if the buffer contains a mouse event, false otherwise.
 */
bool is_mouse_event(const char* buffer, size_t length) {
    return length >= 3 && buffer[0] == '\033' && buffer[1] == '[' && buffer[2] == '<';
}

/**
 * @brief Checks if the given buffer contains an arrow key event.
 *
 * @param buffer The buffer to check.
 * @param length The length of the buffer.
 * @return true if the buffer contains an arrow key event, false otherwise.
 */
bool is_arrow_key(const char* buffer, size_t length) {
    return length >= 3 && buffer[0] == '\033' && buffer[1] == '[' && (buffer[2] == 'A' || buffer[2] == 'B' || buffer[2] == 'C' || buffer[2] == 'D');
}

/**
 * @brief Gets the length of a mouse event in the given buffer.
 *
 * @param buffer The buffer containing the mouse event data.
 * @param length The length of the buffer.
 * @return The length of the mouse event, or 0 if the event is incomplete.
 */
size_t get_mouse_event_length(const char* buffer, size_t length) {
    size_t i = 3;  // Start after "\033[<"

    while (i < length && (isdigit((int)buffer[i]) || buffer[i] == ';')) {
        i++;
    }

    if (i < length && (buffer[i] == 'M' || buffer[i] == 'm')) {
        return i + 1;  // Include the final 'M' or 'm'
    }

    return 0;  // Incomplete mouse event
}

bool get_key_state(unsigned char key) {
    return key_pressed_last_frame[key];
}

void release_key(unsigned char key) {
    key_pressed_last_frame[key] = false;
}

void lock_inputs() {
    unlock = false;
}

void unlock_inputs() {
    unlock = true;
}

void process_input(player* p, Render_Buffer* screen,
                   void (*mouse_left_event_callback)(Render_Buffer* screen, player* p, int x, int y),
                   void (*mouse_right_event_callback)(Render_Buffer* screen, player* p),
                   void (*arrow_key_callback)(Render_Buffer* screen, player* p, int arrow_key),
                   void (*printable_char_callback)(Render_Buffer* screen, player* p, int c)) {
    char buffer[128];
    char input_buffer[MAX_BUFFER_SIZE];

    size_t input_buffer_length = 0;

    MouseEvent event = {0, 1, false, false, 0, 0};

    while (1) {
        ssize_t bytes_read = read(STDIN_FILENO, buffer, sizeof(buffer));

        if (bytes_read <= 0) {
            usleep(1000);  // Prevent CPU saturation
            continue;
        }

        // Append new data to the input buffer
        if (input_buffer_length + bytes_read < MAX_BUFFER_SIZE) {
            memcpy(input_buffer + input_buffer_length, buffer, bytes_read);

            input_buffer_length += bytes_read;
        } else {
            fprintf(stderr, "Input buffer overflow\n");

            input_buffer_length = 0;  // Clear the buffer in case of overflow

            continue;
        }

        // Process the buffer for complete sequences
        size_t processed = 0;

        while (processed < input_buffer_length) {
            if (is_mouse_event(input_buffer + processed, input_buffer_length - processed)) {
                size_t mouse_event_length = get_mouse_event_length(input_buffer + processed, input_buffer_length - processed);
                if (mouse_event_length > 0) {
                    parse_sgr_mouse_event(input_buffer + processed, &event);

                    if (unlock) {
                        if (event.left_click) {
                            mouse_left_event_callback(screen, p, event.target_x, event.target_y);
                        }
                        if (event.right_click) {
                            mouse_right_event_callback(screen, p);
                        }
                    }

                    processed += mouse_event_length;  // Skip the mouse event
                } else {
                    break;  // Wait for more data
                }
            } else if (is_arrow_key(input_buffer + processed, input_buffer_length - processed)) {
                if (unlock)
                    arrow_key_callback(screen, p, (input_buffer + processed)[2]);

                processed += 3;  // Arrow keys are 3 bytes
            } else if ((input_buffer[processed] >= 32 && input_buffer[processed] <= 126) ||
                       input_buffer[processed] == '\n' || input_buffer[processed] == '\r') {
                unsigned char key = (unsigned char)input_buffer[processed];

                if (unlock)
                    printable_char_callback(screen, p, key);

                key_states[key] = true;

                processed++;
            } else {
                // Skip unrecognized or invalid input
                processed++;
            }
        }

        // Store the current state before resetting key_states
        memcpy(key_pressed_last_frame, key_states, sizeof(key_states));

        // Reset key states for next loop iteration
        memset(key_states, 0, sizeof(key_states));

        // Move unprocessed data to the beginning of the buffer
        if (processed < input_buffer_length) {
            memmove(input_buffer, input_buffer + processed, input_buffer_length - processed);
        }

        input_buffer_length -= processed;
    }
}
