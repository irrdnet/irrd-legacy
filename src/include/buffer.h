/*
 * $Id: buffer.h,v 1.1.1.1 2000/02/29 22:28:41 labovit Exp $
 */

#ifndef _BUFFER_H
#define _BUFFER_H

#define BUFFER_TYPE_STREAM	0x01
#define BUFFER_TYPE_MEMORY	0x02

typedef struct _buffer_t {
    u_long type; /* may not be used by all */
    int len;
    u_char *data;
    int data_len;
    FILE *stream;
} buffer_t;


#define buffer_reset(buffer) buffer_adjust (buffer, 0)
#define buffer_data(buffer) ((char *)(buffer)->data)
#define buffer_data_len(buffer) (((buffer)->data)?(buffer)->data_len:-1)

buffer_t *New_Buffer (int len);
buffer_t *Copy_Buffer (buffer_t *origbuff);
buffer_t *New_Buffer_Stream (FILE *stream);
void Delete_Buffer (buffer_t *buffer);
int buffer_adjust (buffer_t *buffer, int len);
int buffer_putc (int c, buffer_t *buffer);
int buffer_poke (int c, buffer_t *buffer, int pos);
int buffer_puts (char *s, buffer_t *buffer);
int buffer_gets (buffer_t *buffer);
int buffer_peek (buffer_t *buffer, int pos);
int buffer_insert (int c, buffer_t *buffer, int pos);
int buffer_delete (buffer_t *buffer, int pos);
int buffer_vprintf (buffer_t *buffer, char *fmt, va_list ap);
int buffer_printf (buffer_t *buffer, char *fmt, ...);

#endif /* _BUFFER_H */
