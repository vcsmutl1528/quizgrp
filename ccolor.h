
#ifndef __CCOLOR_H
#define __CCOLOR_H

#ifdef __cplusplus
extern "C" {
#endif

void cclrinit ();
unsigned char csetclr (unsigned char c);
unsigned char cgetclr (void);
void csetclrv (unsigned char c);

#ifdef __cplusplus
}
#endif

#endif /* __CCOLOR_H */