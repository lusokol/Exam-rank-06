#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>

typedef struct  s_client {
    int id;
    int fd;
    char *str;
    struct s_client *next;
}               t_client;

fd_set cpy_read, cpy_write, curr_sock;
int id = 0;
int sockfd;
char buffer[1001];
char message[42 * 4096];
t_client *clients = NULL;

void fatal() {
    write(2, "Fatal error\n", strlen("Fatal error\n"));
    close(sockfd);
    exit(1);
}

void add_client_to_lst(int fd) {
    t_client *tmp = clients;
    t_client *new = malloc(sizeof(t_client));
    new->id = id;
    id++;
    new->fd = fd;
    new->str = NULL;
    new->next = NULL;
    if (!new)
        fatal();
    if (!tmp) {
        clients = new;
        return ;
    }
    while (tmp && tmp->next)
        tmp = tmp->next;
    tmp->next = new;
}

void del_client_to_lst(int fd) {
    t_client *tmp = clients;
    t_client *del = tmp;

    if (tmp && tmp->fd == fd) {
        clients = clients->next;
        free(tmp->str);
        free(tmp);
        FD_CLR(fd, &curr_sock);
        return ;
    }
    while (tmp && tmp->next && tmp->next->fd != fd)
        tmp = tmp->next;
    if (tmp && tmp->next && tmp->next->fd == fd) {
        del = tmp->next;
        tmp->next = del->next;
        free(del->str);
        free(del);
        FD_CLR(fd, &curr_sock);
        return ;
    }
}

t_client *get_client_by_fd(int fd) {
    t_client *tmp = clients;
    while (tmp) {
        if (tmp->fd == fd)
            return tmp;
        tmp = tmp->next;
    }
    return NULL;
}

int get_max_fd() {
    int max = sockfd;
    t_client *tmp = clients;
    while (tmp) {
        if (tmp->fd > max)
            max = tmp->fd;
        tmp = tmp->next;
    }
    return max;
}

void send_all(int fd, char *str) {
    t_client *tmp = clients;
    while (tmp) {
        if (tmp->fd != fd && FD_ISSET(tmp->fd, &cpy_write)) {
            if (send(tmp->fd, str, strlen(str), 0) < 0)
                fatal();
        }
        tmp = tmp->next;
    }
}

void accept_client() {
    struct sockaddr_in cli;
    socklen_t len = sizeof(cli);
	int connfd = accept(sockfd, (struct sockaddr *)&cli, &len);
	if (connfd < 0) { 
       fatal();
    }
    add_client_to_lst(connfd);
    FD_SET(connfd, &curr_sock);
    t_client *actual = get_client_by_fd(connfd);
    bzero(message, strlen(message));
    sprintf(message, "server: client %d just arrived\n", actual->id);
    send_all(connfd, message);
}

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

void ext_msg(t_client *actual) {
    char *line = NULL;
    while (extract_message(&(actual->str), &line)) {
        bzero(message, strlen(message));
        sprintf(message, "client %d: %s", actual->id, line);
        send_all(actual->fd, message);
    }
}

int main(int ac, char **av) {
	struct sockaddr_in servaddr; 

    if (ac != 2) {
        write(2, "Wrong number of arguments\n", strlen("Wrong number of arguments\n"));
        exit(1);
    }

	// socket create and verification 
	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sockfd == -1) { 
		fatal();
	} 
	bzero(&servaddr, sizeof(servaddr)); 

	// assign IP, PORT 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(av[1])); 
  
	// Binding newly created socket to given IP and verification 
	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) { 
		fatal();
	} 
	if (listen(sockfd, 10) != 0) {
		fatal();
	}

    FD_ZERO(&curr_sock);
    FD_SET(sockfd, &curr_sock);

    while (42) {
        cpy_read = cpy_write = curr_sock;
        if (select(get_max_fd() + 1, &cpy_read, &cpy_write, 0, 0) <= 0)
            continue;
        for (int fd = 0; fd <= get_max_fd(); fd++) {
            if (FD_ISSET(fd, &cpy_read)) {
                if (fd == sockfd) {
                    accept_client();
                }
                else {
                    t_client *actual = get_client_by_fd(fd);
                    bzero(buffer, 1001);
                    int ret_recv = recv(fd, buffer, 1000, 0);
                    actual->str = str_join(actual->str, buffer);
                    if (ret_recv <= 0) {
                        close(fd);
                        sprintf(message, "server: client %d just left\n", actual->id);
                        send_all(fd, message);
                        del_client_to_lst(fd);
                    }
                    else {
                        ext_msg(actual);
                    }
                }
            }
        }
    }
}
