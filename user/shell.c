#include "shell.h"
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#define LINE_LENGTH     64
#define HISTORY_COUNT   8

#define QUEUE_SIZE      16
#define QUEUE_EMPTY     0x10000

#define queue_next(queue, attr) \
    ((queue)->attr < QUEUE_SIZE - 1 ? (queue)->attr + 1 : 0)

struct queue {
    volatile int front, rear;
    char array[QUEUE_SIZE];
};

struct shell {
    int linepos, linecount, newline;
    int hiscur, hiscnt;
    char line[LINE_LENGTH + 1];
    char history[HISTORY_COUNT][LINE_LENGTH + 1];
    struct queue queue;
    const char *prompt;
};

static struct shell m_shell;

#define queue_isempty(queue)    ((queue)->front == (queue)->rear)

static int queue_push(struct queue *queue, int ch)
{
    int next = queue_next(queue, front);
    if (next != queue->rear) { /* not full */
        queue->array[queue->front] = (char)ch;
        queue->front = next;
        return 0;
    }
    return 1;
}

static int queue_pop(struct queue *queue)
{
    if (!queue_isempty(queue)) {
        int res = queue->array[queue->rear];
        queue->rear = queue_next(queue, rear);
        return res;
    }
    return QUEUE_EMPTY;
}

static int wait_char(struct shell *shell)
{
    int ch;
    while ((ch = queue_pop(&shell->queue)) ==  QUEUE_EMPTY);
    return ch;
}

void pack_line(struct shell *shell)
{
    shell->line[shell->linecount] = '\0';
    if (shell->linecount) {
        int hiscur = shell->hiscnt % HISTORY_COUNT;
        strcpy(shell->history[hiscur], shell->line);
        shell->hiscur = ++shell->hiscnt;
    }
    shell->linecount = shell->linepos = 0;
}

static void remove_char(struct shell *shell)
{
    int pos = --shell->linepos, count = shell->linecount;
    while (pos < count) {
        shell->line[pos] = shell->line[pos + 1];
        ++pos;
    }
    shell->line[--shell->linecount] = '\0';
}

static void insert_char(struct shell *shell, int ch)
{
    int pos = shell->linepos, count = shell->linecount;
    if (count < LINE_LENGTH) {
        while (count >= pos) {
            shell->line[count + 1] = shell->line[count];
            --count;
        }
        shell->line[pos] = ch;
        ++shell->linecount;
        ++shell->linepos;
        for (count = shell->linecount; pos < count; ++pos) {
            fputc(shell->line[pos], stdout);
        }
        for (count -= shell->linepos; count > 0; --count) {
            fputc('\b', stdout);
        }
    }
}

static void backspace(struct shell *shell)
{
    if (shell->linepos > 0) {
        remove_char(shell);
        if (shell->linepos < shell->linecount) {
            int i;
            printf("\b%s \b", shell->line + shell->linepos);
            fflush(stdout);
            for (i = shell->linepos; i < shell->linecount; ++i) {
                fputc('\b', stdout);
            }
        } else {
            fputs("\b \b", stdout);
            fflush(stdout);
        }
    }
}

static int addchar(struct shell *shell, int ch)
{
    switch (ch) {
    case '\r':
        pack_line(shell);
        fputc('\n', stdout);
        shell->newline = 1;
        return 1;
    case '\n':
        if (shell->newline) {
            shell->newline = 0;
            break;
        }
        pack_line(shell);
        return 1;
    case '\b': case '\177':
        backspace(shell);
        break;
    default:
        if (isprint(ch)) {
            insert_char(shell, ch);
        }
    }
    return 0;
}

static void change_history(struct shell *shell)
{
    int pos = shell->hiscur % HISTORY_COUNT;
    char *line = shell->history[pos];
    strcpy(shell->line, line);
    printf("\r%s%s\033[K", shell->prompt, line);
    fflush(stdout);
    shell->linecount = shell->linepos = strlen(line);
}

static void up_key(struct shell *shell)
{
    if (shell->hiscur > 0 && shell->hiscur
            + HISTORY_COUNT > shell->hiscnt) {
        --shell->hiscur;
        change_history(shell);
    }
}

static void down_key(struct shell *shell)
{
    if (shell->hiscur + 1 < shell->hiscnt) {
        ++shell->hiscur;
        change_history(shell);
    }
}

static void left_key(struct shell *shell)
{
    if (shell->linepos > 0) {
        --shell->linepos;
        fputc('\b', stdout);
    }
}

static void right_key(struct shell *shell)
{
    if (shell->linepos < shell->linecount) {
        fputc(shell->line[shell->linepos++], stdout);
    }
}

const char* p_readline(struct shell *shell)
{
    int state = 0;
    for (;;) {
        int ch = wait_char(shell);
        if (ch == 0x1B) {
            state = 1;
        } else if (state == 1) {
            state = ch == 0x5B ? 2 : 0;
        } else if (state == 2) {
            state = 0;
            switch (ch) {
            case 0x41: up_key(shell); break;
            case 0x42: down_key(shell); break;
            case 0x43: right_key(shell); break;
            case 0x44: left_key(shell); break;
            default: break;
            }
        } else if (addchar(shell, ch)) {
            return shell->line;
        }
        shell->newline = 0;
    }
    return NULL;
}

const char* shell_readline(const char *prompt)
{
    m_shell.prompt = prompt;
    fputs(prompt, stdout);
    fflush(stdout);
    return p_readline(&m_shell);
}

int shell_addchar(int ch)
{
    queue_push(&m_shell.queue, ch);
    return ch;
}
