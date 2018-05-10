#include <stdio.h>
#include "ftp.h"

static int send_until_all(int sockfd, char *data, int len)
{
    int sent = 0;
    int bytes = 0;

    for (;;) {
        bytes = send(sockfd, data + sent, len - sent, 0);
        if (bytes <= 0) {
            return -1;
        }
        sent += bytes;
        if (sent == len) {
            return 0;
        }
    }
}

static int recv_until_all(int sockfd, char *buf, int len)
{
    int recvd = 0;
    int bytes = 0;
    for (;;) {
        bytes = recv(sockfd, buf + recvd, len - recvd, 0);
        if (bytes <= 0) {
            return -1;
        }
        recvd += bytes;
        if (recvd == len) {
            return 0;
        }
    }
}

static int send_head(int sockfd, struct FTP_head *head)
{
    int buf[2];

    buf[0] = htonl(head->type);
    buf[1] = htonl(head->len);

    if (send_until_all(sockfd, buf, sizeof(int) * 2) == -1) {
        return -1;
    }

    return 0;
}

int FTP_send_msg(int sockfd, struct FTP_msg *msg)
{
    if (send_head(sockfd, &msg->head) == -1) {
        return -1;
    }

    if (msg->head.len != 0) {
        if (send_until_all(sockfd, msg->data, msg->head.len) == -1) {
            return -1;
        }
    }

    return 0;
}

static int send_end(int sockfd)
{
    struct FTP_msg msg;

    msg.head.type = END;
    msg.head.len = 0;
    msg.data = NULL;

    if (FTP_send_msg(sockfd, &msg) == -1) {
        printf("sending end error\n");
        return -1;
    }

    return 0;
}

int FTP_send_cmd(int sockfd, enum FTP_msg_type type, char *command,
        struct FTP_msg *msg)
{
    msg->head.type = type;
    msg->data = command;
    msg->head.len = strlen(command) + 1;

    if (FTP_send_msg(sockfd, msg) == -1) {
        printf("sending command error\n");
        return -1;
    }

    return 0;
}

int FTP_recv_msg(int sockfd, struct FTP_msg *msg)
{
    int buf[2];

    if (recv_until_all(sockfd, buf, sizeof(int) * 2) == -1) {
        printf("receiving head error\n");
        return -1;
    }

    msg->head.type = ntohl(buf[0]);
    msg->head.len = ntohl(buf[1]);

    if (msg->head.len == 0) {
        msg->data = NULL;
    } else {
        msg->data = malloc(msg->head.len);
        if (!msg->data) {
            printf("malloc erroe\n");
            return -1;
        }

        if (recv_until_all(sockfd, msg->data, msg->head.len) == -1) {
            printf("receiving data error\n");
            return -1;
        }
    }

    return 0;
}

int FTP_upload(int sockfd, FILE *fp, int mode)
{
    if (fp == NULL || sockfd == -1) {
        fprintf(stderr, "FILE or SOCKET error\n");
        return -1;
    }

    int count;
    int maxread = 2048;		/* ÿһ�ζ�����ֽ��������Ըĵ� */
    char *buf = malloc(maxread);	/* ��ʱ����һ��ռ����Ż�������� */
    if (buf == NULL) {
        fprintf(stderr, "read_and_send: malloc fails\n");
        return -1;
    }
    struct FTP_msg msg;

    switch (mode) {
        case 0:
            /* asciiģʽ */
            //break;
        case 1:
            /* binģʽ */
            while ((count = fread(buf, sizeof(char), maxread, fp)) != 0) {
                msg.data = buf;
                msg.head.len = count;

                FTP_send_msg(sockfd, &msg);
            }
            /* ���Ҫ����һ��������־��ȥ�����߶Է��ļ��Ѿ������� */
            if (send_end(sockfd) != 0)
                return -1;
            break;
        default:
            fprintf(stderr, "error mode\n");
            return -1;
    }

    free(buf);
    fclose(fp);
    return 0;
}


int FTP_download(int sockfd, FILE * fp, int mode)
{
    if (fp == NULL || sockfd == -1) {
        return -1;
    }

    struct FTP_msg msg;
    int count;		
    while (1) {
        if (FTP_recv_msg(sockfd, &msg) != 0) {
            return -1;
        }

        /* �ж�һ�·������ǲ��ǽ�����־ */
        if (msg.head.type == END) {
            break;
        } else {
            count = fwrite(msg.data, sizeof(char), msg.head.len, fp);
        }

        free(msg.data);	
    }

    fclose(fp);
    return 0;
}
