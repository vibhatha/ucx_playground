#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>

/** @brief
This code sets up an epoll instance to efficiently monitor multiple file descriptors for events. It starts by creating an epoll instance using epoll_create1(0). If the creation fails, an error message is printed and the program exits. Then, it adds a file descriptor (in this case, STDIN_FILENO) to the epoll instance using epoll_ctl() with the EPOLL_CTL_ADD command. If the addition fails, an error message is printed and the program exits.

The code then enters a loop where it waits for events using epoll_wait(). When an event occurs, it processes the events by iterating over the events array. In this case, it checks if the event is associated with STDIN_FILENO. If it is, it reads input from stdin using read() and prints the read bytes.

This code can be used as a starting point for building event-driven applications that need to handle multiple file descriptors efficiently.
*/

#define MAX_EVENTS 10

int main() {
    int epoll_fd, num_events;
    struct epoll_event events[MAX_EVENTS];

    // Create epoll instance
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    // Add file descriptor to epoll instance
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = STDIN_FILENO;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, STDIN_FILENO, &event) == -1) {
        perror("epoll_ctl");
        exit(EXIT_FAILURE);
    }
    int is_exit = 1;
    while (is_exit) {
        // Wait for events
        num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        printf("num_events: %d\n", num_events);
        if (num_events == -1) {
            perror("epoll_wait");
            exit(EXIT_FAILURE);
        }

        // Handle events
        for (int i = 0; i < num_events; i++) {
            if (events[i].data.fd == STDIN_FILENO) {
                printf("Input available on stdin\n");
                char buffer[256];
                fgets(buffer, sizeof(buffer), stdin);
                printf("You entered: %s", buffer);
                if (strcmp(buffer, "exit\n") == 0) {
                    printf("Exiting program\n");
                    close(epoll_fd);
                    is_exit = 0;
                }
            }
                
        }
    }

    // Cleanup
    close(epoll_fd);
    return 0;
}
