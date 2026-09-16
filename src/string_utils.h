#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include <stdbool.h>

bool strBeginsWith(const char *str, const char *control, bool caseSensitive);
void strToLower(char *str);
int strOccurence(const char *str, const char *substr);
void strStripWhitespace(char **str);
bool strIsInteger(const char *str);

#endif // STRING_UTILS_H