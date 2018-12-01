#ifndef STMUTILS_H_
#define STMUTILS_H_

#ifdef __cplusplus
extern "C" {
#endif

char * ftostr(float value);
char * ftostrp(float value, unsigned char prec);
int _write(int fd, char *str, int len);

#ifdef __cplusplus
}
#endif

#endif
