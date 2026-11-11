// port.h - what the phone/PC layer gives the drawing code.
#ifndef KK_PORT_H
#define KK_PORT_H
void port_log(const char *fmt, ...);
void port_target_size(int *w, int *h);              /* the size of the 16:9 picture the game is drawn into */
void port_present(unsigned tex, int w, int h);      /* show the finished picture (and the touch controls), then swap */
#endif
